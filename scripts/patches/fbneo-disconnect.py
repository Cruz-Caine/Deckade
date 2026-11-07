from pathlib import Path

p = Path('src/burner/sdl/fbn_quark_sdl.cpp')
s = p.read_text()
if 'deckadeRequestMatchExit' not in s:
    s = s.replace('#include <unistd.h>', '#include <unistd.h>\n#include <SDL.h>', 1)
    helper = '''
static void deckadeRequestMatchExit()
{
   if (MacadeEmbeddedEnabled()) return;
   SDL_Event event{};
   event.type = SDL_QUIT;
   SDL_PushEvent(&event);
}
'''
    s = s.replace('namespace {\n', 'namespace {\n' + helper, 1)
    s = s.replace('''      } else if (client->code == GGPOCLIENT_EVENTCODE_DISCONNECTED) {
         MacadeEmbeddedSetOverlayConnectionPhase(8);
         MacadeEmbeddedSetOverlaySystemMessage("Disconnected!", 300);''', '''      } else if (client->code == GGPOCLIENT_EVENTCODE_DISCONNECTED) {
         MacadeEmbeddedSetOverlayConnectionPhase(8);
         MacadeEmbeddedSetOverlaySystemMessage("Disconnected!", 300);
         deckadeRequestMatchExit();''', 1)
    s = s.replace('''   } else if (info->code == GGPO_EVENTCODE_DISCONNECTED_FROM_PEER) {
      MacadeEmbeddedSetOverlayConnectionPhase(8);
      MacadeEmbeddedSetOverlaySystemMessage("Disconnected from Peer", 300);''', '''   } else if (info->code == GGPO_EVENTCODE_DISCONNECTED_FROM_PEER) {
      MacadeEmbeddedSetOverlayConnectionPhase(8);
      MacadeEmbeddedSetOverlaySystemMessage("Disconnected from Peer", 300);
      deckadeRequestMatchExit();''', 1)
    p.write_text(s)
