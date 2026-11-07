from pathlib import Path
p = Path('src/burner/sdl/main.cpp')
s = p.read_text()
old = '''\tif (usemenu)
\t{
#ifdef BUILD_SDL2
\t\tprintf("SDL menu is unavailable in this macOS stock build. Launch a game directly.\\n");
\t\tBurnLibExit();
\t\tSDL_Quit();
\t\treturn 1;
#endif
\t}
\telse if (dat)
'''
new = '''\tif (dat)
'''
if old not in s:
    raise SystemExit('FBNeo command-line block not found')
p.write_text(s.replace(old, new, 1))
