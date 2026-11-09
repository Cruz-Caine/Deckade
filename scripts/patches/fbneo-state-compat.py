"""Load official Fightcade Neo Geo savestates correctly.

The states Fightcade ships were written by an FBNeo build that did not save
`Neo68KFix` as an offset, so each is exactly 4 bytes shorter than this tree
expects. Savestates are restored by position with no size checks, which meant
the trailing LED bytes (nLEDLatch, nLED) were read as that offset. Any state
saved with a lit LED latch (kof98: 0x97) then mapped the program ROM that many
bytes out of place and crashed into the BIOS crosshatch screen on frame 1.

Detect the older layout by its exact size and skip the field it does not have.
GGPO rollback states never set the flag, so their format is unchanged.
"""
from pathlib import Path


def replace_once(text, old, new, label):
    if old not in text:
        raise SystemExit(f'fbneo-state-compat: {label} not found')
    return text.replace(old, new, 1)


p = Path('src/burner/statec.cpp')
s = p.read_text()
if 'BurnStateInflatedLen' not in s:
    s += '''
// DECKADE: size of a compressed state block once inflated.
INT32 BurnStateInflatedLen(UINT8* Def, INT32 nDefLen)
{
	z_stream stream;
	UINT8 scratch[0x4000];
	memset(&stream, 0, sizeof(stream));
	if (inflateInit(&stream) != Z_OK) {
		return -1;
	}
	stream.next_in = Def;
	stream.avail_in = nDefLen;
	INT32 nResult;
	do {
		stream.next_out = scratch;
		stream.avail_out = sizeof(scratch);
		nResult = inflate(&stream, Z_NO_FLUSH);
	} while (nResult == Z_OK);
	const INT32 nTotal = (nResult == Z_STREAM_END) ? (INT32)stream.total_out : -1;
	inflateEnd(&stream);
	return nTotal;
}
'''
    p.write_text(s)

p = Path('src/burner/state.cpp')
s = p.read_text()
if 'bNeoStateLacksFixOffset' not in s:
    s = replace_once(s, '// from dynhuff.cpp\n',
                     'INT32 BurnStateInflatedLen(UINT8* Def, INT32 nDefLen); // DECKADE\n'
                     'extern INT32 bNeoStateLacksFixOffset; // DECKADE\n\n'
                     '// from dynhuff.cpp\n', 'state.cpp header')
    s = replace_once(s, '\tnRet = BurnStateDecompress(Def, nDefLen, bAll);',
                     '''\tconst INT32 nInflatedLen = BurnStateInflatedLen(Def, nDefLen); // DECKADE
\tbNeoStateLacksFixOffset = (bAll && nInflatedLen + 4 == nLen) ? 1 : 0;

\tnRet = BurnStateDecompress(Def, nDefLen, bAll);
\tbNeoStateLacksFixOffset = 0;''', 'BurnStateDecompress call')
    p.write_text(s)

p = Path('src/burn/drv/neogeo/neo_run.cpp')
s = p.read_text()
if 'bNeoStateLacksFixOffset' not in s:
    s = replace_once(s, 'INT32 NeoScan(INT32 nAction, INT32* pnMin)\n',
                     'INT32 bNeoStateLacksFixOffset = 0; // DECKADE: see fbneo-state-compat.py\n\n'
                     'INT32 NeoScan(INT32 nAction, INT32* pnMin)\n', 'NeoScan')
    s = replace_once(s, '\t\tSCAN_OFF(Neo68KFix[nNeoActiveSlot], Neo68KROM[nNeoActiveSlot], nAction);',
                     '''\t\tif ((nAction & ACB_WRITE) && bNeoStateLacksFixOffset) {
\t\t\tNeo68KFix[nNeoActiveSlot] = Neo68KROM[nNeoActiveSlot];
\t\t} else {
\t\t\tSCAN_OFF(Neo68KFix[nNeoActiveSlot], Neo68KROM[nNeoActiveSlot], nAction);
\t\t}''', 'Neo68KFix scan')
    p.write_text(s)

# Report the outcome so tests can tell "state loaded" from "silently rebooted".
p = Path('src/burner/sdl/fbn_quark_sdl.cpp')
s = p.read_text()
if 'Deckade state load' not in s:
    s = replace_once(s, '   return file_exists(state) && BurnStateLoad(state, 1, &DrvInitCallback) == 0;',
                     '''   if (!file_exists(state)) {
      fprintf(stderr, "Deckade state load: %s missing\\n", name);
      return false;
   }
   const INT32 result = BurnStateLoad(state, 1, &DrvInitCallback);
   fprintf(stderr, "Deckade state load: %s result=%d\\n", name, result);
   return result == 0;''', 'load_fightcade_state return')
    p.write_text(s)
