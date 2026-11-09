from pathlib import Path

p = Path('src/intf/video/vid_interface.cpp')
s = p.read_text()
if 'DECKADE_FRAME_DUMP' not in s:
    s = s.replace('#include "macade_embedded.h"',
                  '#include "macade_embedded.h"\n\n#include <cstdio>\n#include <cstdlib>', 1)
    helper = '''
static int deckade_frame_dump_counter = 0;
static void DeckadeMaybeDumpFrame()
{
    const char* path = getenv("DECKADE_FRAME_DUMP");
    if (path == nullptr || path[0] == 0 || pVidImage == nullptr) return;
    const char* frames_env = getenv("DECKADE_FRAME_DUMP_FRAMES");
    const int target = frames_env ? atoi(frames_env) : 90;
    if (deckade_frame_dump_counter++ != target) return;
    FILE* file = fopen(path, "wb");
    if (file == nullptr) return;
    int header[5] = { 0x44464D52, nVidImageWidth, nVidImageHeight, nVidImageDepth, nVidImagePitch };
    fwrite(header, sizeof(int), 5, file);
    fwrite(pVidImage, 1, (size_t)nVidImagePitch * (size_t)nVidImageHeight, file);
    fclose(file);
}
'''
    s = s.replace('INT32 VidPaint(INT32 bValidate)', helper + '\nINT32 VidPaint(INT32 bValidate)', 1)
    # Must run after the VidPaint rewrite in build-fbneo.sh. Fail loudly: a
    # silent miss here leaves tests/game_matrix.sh with no frames to check.
    present = '''\treturn pVidOut[nVidActive]->Paint(bValidate);
}'''
    if present not in s:
        raise SystemExit('fbneo-frame-dump: rewritten VidPaint not found')
    s = s.replace(present, '''\tconst INT32 result = pVidOut[nVidActive]->Paint(bValidate);
\tDeckadeMaybeDumpFrame();
\treturn result;
}''', 1)
    p.write_text(s)
