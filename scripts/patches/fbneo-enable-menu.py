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
'''
if old in s:
    p.write_text(s.replace(old, '', 1))
