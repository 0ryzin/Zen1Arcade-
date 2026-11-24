Automatic build of `main` (Phases 3 and 4 in progress).

**What's new**
- Zen1's own engine now emulates Capcom CPS1 hardware (68000, Z80, YM2151, OKI MSM6295, CPS-A/CPS-B video). Implemented games: Street Fighter II (sf2), Street Fighter II' Champion Edition (sf2ce, needs sf2.zip too), Final Fight (ffight), 1941 (1941, vertical).
- A library with 5 games per row, collections (All, Favourites, Recent, Capcom CPS1, Not supported), search, and clear READY / ROMS NEEDED / NOT SUPPORTED labels.
- A startup intro with sound made by Zen1. Settings let you turn it off, change its length, mute it, make it skippable or replay it.
- Controller screen: assign a panel to Player 1 or 2 by pressing a button on it, set the six buttons and the player button, swap the players, reset, and open the test card.
- A new neon test card.

**Testing status, please read**
- Build server: everything compiles; 43 unit tests pass; the engine runs Zen1's own CPS1 self-test program with no screen attached; the frontend screens and the intro are rendered with no screen attached (see the screenshots and `zen1-intro-preview.mp4`).
- Real games: **not yet verified**. No commercial ROMs are used on the build server. The first real-game test is yours, on the Steam Deck.
- Steam Deck and cabinet: not yet tested.

ROM sets must match current MAME naming and versions: `sf2.zip`, `sf2ce.zip` (plus `sf2.zip`), `ffight.zip` and `1941.zip`, placed in `~/Zen1Arcade/Games`. Zen1 Arcade does not include or download any ROMs.

Install and test steps: docs/STEAM_DECK.md in the repository.
