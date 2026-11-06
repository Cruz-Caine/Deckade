# Local match/netplay testing

A real Fightcade challenge needs a second human or account. To test the
match path without the lobby, Deckade ships a local two-peer GGPO loopback:

```sh
./tests/quark_loopback.sh sfiii3nr1
```

It starts two copies of the native FBNeo runtime with:

```text
quark:direct,sfiii3nr1,6000,127.0.0.1,6001,0,2,0
quark:direct,sfiii3nr1,6001,127.0.0.1,6000,1,2,0
```

The first instance listens on port 6000 and dials 6001; the second does the
reverse with player id 1. `quark:direct` is the local peer route implemented by
the same GGPO-native runtime used for Fightcade `quark:served` matches. It
therefore exercises:

- FBNeo ROM loading and the SDL video path,
- GGPO peer connection and rollback/input exchange,
- the P1/P2 names drawn at the top of the screen,
- delay/ranked metadata.

The overlay names come from `DECKADE_OVERLAY_P1` / `DECKADE_OVERLAY_P2` in this
test. In a real `quark:served` match FBNeo receives the names from the
Fightcade match-info message instead; the overlay reads those through
`MacadeEmbeddedSetOverlayGameInfo`.

## Fullscreen

Deckade intentionally launches FBNeo windowed. SDL's fullscreen-desktop path
was producing a transparent/black window under XWayland and some tiling window
managers. Let i3 handle the window, or let Steam Deck Game Mode auto-fullscreen
it. `SDL_VIDEODRIVER=x11` is selected automatically when an X11 `DISPLAY`
exists.

## Interpreting the result

- Both windows at the character select screen with the two test names means the
  peer connection, video, and overlay are working.
- Insert coin (keyboard `5`) and start (`1`) in either window should advance
  both players; that is GGPO input propagation.
- If one window stays on "INSERT COIN", check the two logs:
  `/tmp/deckade-quark-p1.log` and `/tmp/deckade-quark-p2.log`.
