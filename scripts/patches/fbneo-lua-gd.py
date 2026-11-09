"""Make Lua training overlays work on the SDL build.

Adds Deckade's built-in gd module (see deckade_lua_gd.h), and paints Lua's gui
layer each frame. FBNeo only makes that FBA_LuaGui call from its Windows video
backends, so on SDL the menu and damage text were drawn nowhere.
"""
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

shutil.copy(ROOT / 'runtime' / 'fbneo' / 'deckade_lua_gd.h', 'src/burner/deckade_lua_gd.h')

p = Path('src/burner/luaengine.cpp')
s = p.read_text()
if 'deckadeOpenLuaGd' not in s:
    for old, new, label in (
        ('#include "luaengine.h"\n', '#include "luaengine.h"\n#include <algorithm>\n#include "deckade_lua_gd.h"\n', 'include'),
        ('\t\tluaL_openlibs(LUA);\n', '\t\tluaL_openlibs(LUA);\n\t\tdeckadeOpenLuaGd(LUA);\n', 'luaL_openlibs'),
    ):
        if old not in s:
            raise SystemExit(f'fbneo-lua-gd: {label} not found')
        s = s.replace(old, new, 1)
    p.write_text(s)

p = Path('src/intf/video/sdl/vid_sdl2.cpp')
s = p.read_text()
if 'FBA_LuaGui' not in s:
    old = """\tSDL_RenderClear(sdlRenderer);

\tif (nRotateGame)
\t{
\t\tSDL_UpdateTexture(sdlTexture, NULL, pVidImage, nVidImagePitch);"""
    new = """\tif (kNetLua) {
\t\tFBA_LuaGui((unsigned char*)pVidImage, nVidImageWidth, nVidImageHeight, nVidImageBPP, nVidImagePitch);
\t}

\tSDL_RenderClear(sdlRenderer);

\tif (nRotateGame)
\t{
\t\tSDL_UpdateTexture(sdlTexture, NULL, pVidImage, nVidImagePitch);"""
    if old not in s:
        raise SystemExit('fbneo-lua-gd: SDL Paint not found')
    s = s.replace(old, new, 1)
    include = '#include "macade_embedded.h"\n'
    if include not in s:
        raise SystemExit('fbneo-lua-gd: vid_sdl2 include not found')
    s = s.replace(include, include + 'void FBA_LuaGui(unsigned char *s, int width, int height, int bpp, int pitch);\n', 1)
    p.write_text(s)
