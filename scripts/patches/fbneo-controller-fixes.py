#!/usr/bin/env python3
"""Apply Deckade's controller fixes to a Macade FBNeo tree (run from FBNeo root)."""
from pathlib import Path

# 1. Keep SDL_GameController handles so D-pad buttons can be read even when a
#    driver exposes the D-pad as buttons instead of a POV hat.
p = Path('src/intf/input/sdl/inp_sdl2.cpp')
s = p.read_text()
s = s.replace(
    "static SDL_Joystick* JoyList[MAX_JOYSTICKS];\nstatic int* JoyPrevAxes = NULL;",
    "static SDL_Joystick* JoyList[MAX_JOYSTICKS];\n"
    "static SDL_GameController* ControllerList[MAX_JOYSTICKS];\n"
    "static int* JoyPrevAxes = NULL;", 1)

s = s.replace(
"""   temp = SDL_GameControllerOpen(i);
   if (temp) {
       const SDL_GameControllerButton controls[8] = {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
           SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
           SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START};
       for (int button = 0; button < 8; ++button) {
           bind = SDL_GameControllerGetBindForButton(temp, controls[button]);
           buttons[i][button] = bind.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON ? bind.value.button : -1;
       }
       SDL_GameControllerClose(temp);
   }""",
"""   temp = SDL_GameControllerOpen(i);
   ControllerList[i] = temp;
   if (temp) {
       const SDL_GameControllerButton controls[8] = {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
           SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
           SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START};
       for (int button = 0; button < 8; ++button) {
           bind = SDL_GameControllerGetBindForButton(temp, controls[button]);
           buttons[i][button] = bind.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON ? bind.value.button : -1;
       }
   }""", 1)

s = s.replace(
"""		if (JoyList[i]) {
			SDL_JoystickClose(JoyList[i]);
			JoyList[i] = NULL;
		}""",
"""		if (ControllerList[i]) {
			SDL_GameControllerClose(ControllerList[i]);
			ControllerList[i] = NULL;
		}
		if (JoyList[i]) {
			SDL_JoystickClose(JoyList[i]);
			JoyList[i] = NULL;
		}""", 1)

start = s.find('int SDLinpJoyAxis(int i, int nAxis)')
if start < 0:
    raise SystemExit('SDLinpJoyAxis not found')
end = s.find('\n}\n', start)
if end < 0:
    raise SystemExit('SDLinpJoyAxis end not found')
new_axis = '''int SDLinpJoyAxis(int i, int nAxis)
{
	if (i < 0 || i >= nJoystickCount) {				// This joystick number isn't connected
		return 0;
	}

	if (ReadJoystick() != 0) {						// There was an error polling the joystick
		return 0;
	}

	if (nAxis >= SDL_JoystickNumAxes(JoyList[i])) {
		return 0;
	}

	int value = SDL_JoystickGetAxis(JoyList[i], nAxis) << 1;

	// Steam Deck and most console-style pads expose the D-pad as a POV hat,
	// while FBNeo's legacy joystick bindings use axes for directions. Fold hat
	// 0 and game-controller D-pad buttons into the left stick X/Y axes so both
	// the stick and D-pad work.
	if (nAxis == 0 || nAxis == 1) {
		bool left = false, right = false, up = false, down = false;
		if (SDL_JoystickNumHats(JoyList[i]) > 0) {
			const Uint8 hat = SDL_JoystickGetHat(JoyList[i], 0);
			left = (hat & SDL_HAT_LEFT) != 0;
			right = (hat & SDL_HAT_RIGHT) != 0;
			up = (hat & SDL_HAT_UP) != 0;
			down = (hat & SDL_HAT_DOWN) != 0;
		}
		if (ControllerList[i]) {
			left = left || SDL_GameControllerGetButton(ControllerList[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
			right = right || SDL_GameControllerGetButton(ControllerList[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
			up = up || SDL_GameControllerGetButton(ControllerList[i], SDL_CONTROLLER_BUTTON_DPAD_UP);
			down = down || SDL_GameControllerGetButton(ControllerList[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
		}
		if (nAxis == 0) {
			if (left) value = -65536;
			else if (right) value = 65534;
		} else {
			if (up) value = -65536;
			else if (down) value = 65534;
		}
	}

	return value;
}'''
s = s[:start] + new_axis + s[end+2:]

p.write_text(s)

# 2. Let a Deckade profile own direction inputs that legacy init maps to axes.
p = Path('src/macade/macade_gamepad.cpp')
s = p.read_text()
old = """        struct GameInp* input = &GameInp[i];
        if (!input->Input.pVal) continue;
        if (input->nType == BIT_DIGITAL) {
            if (input->nInput != 0 && input->nInput != GIT_SWITCH) continue;
            input->Input.nVal |= devices.state(binding->device, binding->control) != 0;
            if (copy) *input->Input.pVal = input->Input.nVal;"""
new = """        struct GameInp* input = &GameInp[i];
        if (!input->Input.pVal) continue;
        if (input->nType == BIT_DIGITAL) {
            const bool owns = MacadeGamepadOwnsInput(i);
            if (input->nInput != 0 && input->nInput != GIT_SWITCH && !owns) continue;
            // Direction inputs are normally GIT_JOYAXIS/GIT_JOYAXIS_NEG. If a
            // Deckade profile owns one, discard the legacy axis value before
            // applying the captured button/hat/axis input.
            if (owns) input->Input.nVal = 0;
            input->Input.nVal |= devices.state(binding->device, binding->control) != 0;
            if (copy) *input->Input.pVal = input->Input.nVal;"""
if old not in s:
    raise SystemExit('MacadeGamepadApply block not found')
p.write_text(s.replace(old, new, 1))
print('Applied Deckade FBNeo controller fixes')

# 3. Select which SDL joystick becomes Player 1. FBNeo's default -joy mapping
#    always uses joystick 0; Steam Input/virtual devices can occupy that slot.
p = Path('src/burner/sdl/input_sdl2.cpp')
s = p.read_text()
old = """INT32 Init_Joysticks(int p_one_use_joystick)
{
   if (!p_one_use_joystick)
   {
      /*keyboard p1, joy0 p2) */
      GameInpConfig(0, 0, 1);
      GameInpConfig(1, 1, 1);
   }
   else
   {
      /*init all joysticks (4 max atm) to map or the default will think 
      p1 is keyboard and p2 p3 p4 is joy 0 1 2 instead of joy 1 2 3 */
      
      for (int i = 0; i < 4; ++i)
      {
         GameInpConfig(i, i+1, 1);
      }
   }"""
new = """INT32 Init_Joysticks(int p_one_use_joystick)
{
   if (!p_one_use_joystick)
   {
      /*keyboard p1, joy0 p2) */
      GameInpConfig(0, 0, 1);
      GameInpConfig(1, 1, 1);
   }
   else
   {
      /*init all joysticks (4 max atm) to map or the default will think 
      p1 is keyboard and p2 p3 p4 is joy 0 1 2 instead of joy 1 2 3 */
      int p1Index = 0;
      if (const char* configured = getenv("DECKADE_P1_JOYSTICK_INDEX")) {
         p1Index = atoi(configured);
         if (p1Index < 0 || p1Index >= SDL_NumJoysticks()) p1Index = 0;
      }
      GameInpConfig(0, p1Index + 1, 1);
      for (int i = 1; i < 4; ++i)
      {
         int index = (i == p1Index) ? (p1Index == 0 ? 1 : 0) : i;
         if (index >= SDL_NumJoysticks()) index = 0;
         GameInpConfig(i, index + 1, 1);
      }
   }"""
if old not in s:
    raise SystemExit('Init_Joysticks block not found')
p.write_text(s.replace(old, new, 1))

# 4. Draw the username/connection overlay into the normal SDL window.
p = Path('src/intf/video/sdl/vid_sdl2.cpp')
s = p.read_text()
old = """void RenderMessage()
{
"""
new = """extern void MacadeEmbeddedRenderOverlay(SDL_Renderer *renderer);

void RenderMessage()
{
"""
if old not in s:
    raise SystemExit('RenderMessage start not found')
s = s.replace(old, new, 1)
old = """	if (messageFrames > 1)
	{
		inprint_shadowed(sdlRenderer, lastMessage, 10, 30);
		messageFrames--;
	}
}"""
new = """	if (messageFrames > 1)
	{
		inprint_shadowed(sdlRenderer, lastMessage, 10, 30);
		messageFrames--;
	}
	MacadeEmbeddedRenderOverlay(sdlRenderer);
}"""
if old not in s:
    raise SystemExit('RenderMessage end not found')
p.write_text(s.replace(old, new, 1))

