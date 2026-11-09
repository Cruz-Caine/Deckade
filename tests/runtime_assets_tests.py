#!/usr/bin/env python3
"""Checks the bundled FBNeo runtime assets and build wiring without any ROMs.

tests/game_matrix.sh proves real games boot but needs ROMs, so it cannot run
in CI. This covers what can be checked from the repository alone, including
the regressions that let broken savestates and an unusable frame check ship.
"""
import pathlib
import re
import struct
import sys
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
FBNEO = ROOT / 'runtime' / 'fbneo'
ENGINE_VERSION = 0x029744
HEADER_SIZE = 12 + 0x40

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def check_savestate(path):
    data = path.read_bytes()
    label = path.name
    if data[:4] != b'FB1 ' or data[4:8] != b'FS1 ':
        check(False, f'{label}: not an FBNeo savestate')
        return
    file_version, _, min_all, deflated = struct.unpack('<4i', data[12:28])
    game = data[28:60].split(b'\0')[0].decode()
    expected = re.sub(r'_fbneo(_ranked)?\.fs$', '', label)
    check(game == expected, f'{label}: holds state for {game}')
    check(file_version <= ENGINE_VERSION, f'{label}: newer than the engine ({file_version:#x})')
    check(min_all <= ENGINE_VERSION, f'{label}: needs a newer engine ({min_all:#x})')
    try:
        zlib.decompress(data[HEADER_SIZE:HEADER_SIZE + deflated])
    except zlib.error as error:
        check(False, f'{label}: corrupt state block ({error})')


def check_savestates():
    states = sorted((FBNEO / 'savestates').glob('*.fs'))
    check(len(states) > 400, f'expected the full savestate set, found {len(states)}')
    check((FBNEO / 'savestates' / 'kof98_fbneo.fs').exists(), 'kof98 savestate is missing')
    for path in states:
        check_savestate(path)


def check_overlay_assets():
    ui = FBNEO / 'ui'
    names = ['background.png', 'spectators.png', 'font_0.png'] + [f'rank{i}.png' for i in range(7)]
    for name in names:
        path = ui / name
        check(path.exists() and path.read_bytes()[:4] == b'\x89PNG', f'ui/{name}: missing or not a PNG')
    font = ui / 'font.fnt'
    check(font.exists() and font.read_bytes()[:3] == b'BMF', 'ui/font.fnt: missing or not a binary BMFont')

    detectors = list((FBNEO / 'detector').glob('*.inf'))
    check(len(detectors) > 300, f'expected the score detector set, found {len(detectors)}')
    check((FBNEO / 'detector' / 'kof98.inf').exists(), 'kof98 score detector is missing')


def check_build_wiring():
    script = (ROOT / 'scripts' / 'build-fbneo.sh').read_text()
    # The frame dump hooks the rewritten VidPaint; run earlier it installs nothing.
    check(script.find('fbneo-frame-dump.py') > script.find('Could not find the Macade VidPaint block'),
          'fbneo-frame-dump.py must run after the VidPaint rewrite')
    for patch in sorted((ROOT / 'scripts' / 'patches').glob('*.py')):
        check(patch.name in script, f'{patch.name} is never applied by build-fbneo.sh')


def check_lua_overlay_wiring():
    patch = (ROOT / 'scripts' / 'patches' / 'fbneo-lua-gd.py').read_text()
    check('FBA_LuaGui' in patch, 'SDL build must paint the Lua gui layer')
    check('deckadeOpenLuaGd' in patch, 'SDL build must register the built-in gd module')
    check(not (FBNEO / 'gd.so').exists(), 'gd.so needs a system libgd; use the built-in module')


def check_no_fightcade_dependency():
    # Deckade must run without a Fightcade install and leave its files alone.
    forbidden = ('com.fightcade.Fightcade', 'emulator/fcade', 'fcade://', 'fcade-quark', 'emulator/flycast')
    for folder in ('src', 'qml', 'scripts', 'packaging'):
        for path in (ROOT / folder).rglob('*'):
            if not path.is_file() or path.suffix not in {'.cpp', '.h', '.qml', '.sh', '.py', '.desktop', '.xml'}:
                continue
            text = path.read_text(errors='replace')
            for needle in forbidden:
                check(needle not in text, f'{path.relative_to(ROOT)}: depends on Fightcade ({needle})')


def check_training_scripts():
    # Windows-only paths abort the training script on Linux.
    pattern = re.compile(r'(dofile\([^)]*\\\\|resourcepath\s*=\s*"[^"]*\\\\)')
    for path in (FBNEO / 'fbneo-training-mode').rglob('*.lua'):
        for number, line in enumerate(path.read_text(errors='replace').splitlines(), 1):
            if pattern.search(line) and not line.lstrip().startswith('--'):
                check(False, f'{path.name}:{number}: Windows path separator')
            # Without -p a missing parent folder crashes the script's GUI callback.
            if 'os.execute' in line and 'mkdir' in line and 'mkdir -p' not in line:
                check(False, f'{path.name}:{number}: mkdir needs -p on Linux')


def main():
    check_savestates()
    check_overlay_assets()
    check_build_wiring()
    check_lua_overlay_wiring()
    check_training_scripts()
    check_no_fightcade_dependency()
    for failure in failures:
        print(f'FAIL {failure}')
    print(f'runtime assets: {len(failures)} failure(s)')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
