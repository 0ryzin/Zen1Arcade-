# Zen1 Arcade on the Steam Deck

Everything here is done in **Desktop Mode** (Steam button → Power →
Switch to Desktop). Zen1 Arcade never changes how the Deck boots: you
start it yourself.

## Where things live

| What | Location on the Deck |
| --- | --- |
| ROM sets (zip files, not extracted) | `/home/deck/Zen1Arcade/Games/` |
| Settings, controls, library state | `/home/deck/Zen1Arcade/Configurations/` |
| Your own artwork (optional) | `/home/deck/Zen1Arcade/Artwork/` |
| Logs (send these when something goes wrong) | `/home/deck/Zen1Arcade/Logs/` |
| The program | e.g. `/home/deck/Applications/Zen1Arcade-x86_64.AppImage` |

Updating Zen1 Arcade means replacing the AppImage file. The `Zen1Arcade`
folder is never deleted, moved or overwritten by an update.

## 1. Install

1. Download `Zen1Arcade-x86_64.AppImage` from the
   [development build](https://github.com/0ryzin/Zen1Arcade-/releases/tag/dev-latest).
2. Move it to `~/Applications/` (create the folder if needed).
3. Make it executable: in Dolphin, right-click → Properties → Permissions →
   tick *Is executable*. Or in Konsole:
   `chmod +x ~/Applications/Zen1Arcade-x86_64.AppImage`
4. Double-click it. On first start it creates `~/Zen1Arcade/`.

## 2. Add games

Zen1 Arcade runs only games whose hardware its own engine implements:

| Hardware | Games (MAME set names) |
| --- | --- |
| Capcom CPS-1 | sf2, sf2ce, sf2hf, ffight, 1941, ghouls, strider, dynwar, willow, unsquad, mercs, mtwins, msword, cawing, nemo, 3wonders, kod, captcomm, knights, varth, megaman |
| Capcom CPS-2 | ssf2t, sfa, sfa2, sfa3, dstlk, vsav, xmcota, msh, mvsc, ddtod, ddsom, avsp, 19xx, 1944, progear, spf2t, ... (291 sets; each set includes its `.key` file; add `qsound.zip` for exact sound) |
| Capcom 1942 | 1942, 1942a, 1942b, 1942w |
| Namco Pac-Man | pacman (needs puckman.zip too), puckman, mspacman |
| Namco Galaxian | galaxian, galaxiana, galaxianm, galaxiant, superg |
| SNK Neo Geo MVS | mslug, mslug2, kof94-kof97, samsho-samsho4, ... (122; needs `neogeo.zip` in Games or BIOS) |

The full list is [COMPATIBILITY.md](COMPATIBILITY.md). New files are
noticed while Zen1 runs; the library updates a few seconds later.

Put the zips in `~/Zen1Arcade/Games` (subfolders are fine). They must match
**current MAME** versions; files are checked by CRC. Clones need their
parent zip next to them (sf2ce/sf2hf need sf2.zip, pacman needs puckman.zip).
Every zip shows in the library. Games Zen1 cannot run yet show **NOT
SUPPORTED**; for recognised ones, the panel says what hardware is missing.
Zen1 Arcade does not include or download games.

## 3. Using it

1. The start-up animation plays (any button skips it).
2. **INSERT COIN** waits for a button press: the coin drops and the library opens.
3. The **library** shows every game, artwork first. Move with Player 1's stick.
   **1P or LP plays** the selected game. Push **up from the top row** to reach
   **SETUP**.
4. **In a game (and in the menus):**
   - 1P / 2P (short press): coin + start / join.
   - **Double press 1P:** volume down (100 → 50 → 0 %). **Double press 2P:**
     volume up (0 → 50 → 100 %). A big VOLUME panel shows the level.
   - **Hold 1P for 5 seconds:** the game is ended and you are back in the
     library, on the same game.
   - **Hold 2P for 5 seconds:** restart the game.
   - A bar shows while you hold; let go to cancel. None of this reaches the
     game.
5. **Leave Zen1 Arcade:** SETUP → Exit, or hold both player buttons for 3
   seconds in the menus.

Keyboard (development, and the Deck's own controls in Desktop Mode, which
type keys there): arrows move, Enter = LP, Esc = back, 1 = 1P button.
Inside games without panels, the Deck's controls stand in for Player 1
(X Y RB = LP MP HP, A B RT = LK MK HK, Start = 1P, View = 2P).

## 4. Panels: SETUP → Controllers

- **Choose from detected list:** every controller with its name, USB IDs,
  serial and port; choose Player 1, Player 2 or Both (one board for both
  panels). Or **assign by pressing** any button on the panel you mean. It
  is saved by USB port, serial and model, so plug-in order never matters
  and reconnecting keeps it.
- **Swap**, **Unassign**, **Detect again**.
- **Player 1 / Player 2 buttons:** every action on its own row; choose a
  row, press the new input (it shows straight away). Already used? Zen1
  says where and asks you to press it again to confirm. MP clears a row.
  Quick setup, reset to defaults and swap layouts are at the bottom.
- **Calibrate stick:** let go (resting position and drift), move it fully
  round (range), set the dead zone (LP smaller / HP bigger), test all 8
  directions, then 1P saves. This is saved per controller, and **Reset**
  restores the defaults. Digital (micro-switch) sticks are detected and only
  tested, because software has nothing to correct on them. Software cannot
  repair a worn or broken switch.
- **Controller test:** both panels live, with how 8-way, 4-way and 2-way
  games read the stick. Hold 1P for 2 seconds to leave.
- The Steam Deck's own controls are never made a cabinet player.

## 5. Display

Zen1 Arcade never changes the screen resolution. With the Deck docked it
uses the 1280 × 1024 screen if one is connected, otherwise the main screen.
The default **Smart fit** learns each game's real picture area in its first
10 seconds (saved per game), keeps its shape and uses sharp whole multiples
when they fill 90 % of the screen (CPS-1: 1195 × 896). Nothing is
stretched. SETUP → Display also has Original shape, Integer and Pixel
perfect, a *Game glow* surround, and per-game picture settings with a reset.
See [DISPLAY.md](DISPLAY.md). Per-game overrides go in
`Configurations/games/<set>.toml`, for example:

```
[display]
scale = "integer"
rotate = 0        # extra rotation on top of the game's own
bezel = "black"   # neon | blur | black | artwork
```

Your own bezel: `Artwork/Bezels/<set>.bmp` or `Artwork/Bezels/default.bmp`
(BMP, screen-sized), with Screen surround = MY BEZELS.

**Artwork:** with SETUP → Library → *Download artwork automatically* on,
Zen1 fetches the flyer, title and in-game pictures for the games you have
and keeps them offline in `Artwork/downloaded`. Your own pictures always
win: `Artwork/<set>/card.png` (also `flyer`, `boxart`, `title`, `snap`,
`marquee`, `.png` or `.jpg`). *Next artwork* cycles the pictures for the
selected game. *Choose my own artwork* opens a picture browser (images in
`Artwork/import`, Pictures, Downloads, Desktop); *Look online again* and
*Back to automatic artwork* are next to it. Failed downloads are retried
later, and new games get their artwork as soon as they are noticed.

## 6. What to test and report

Please send `~/Zen1Arcade/Logs/` (newest files) with any report.

1. Start-up: animation and sound on the cabinet screen; INSERT COIN waits; coin sound.
2. Library: your zips appear; stick movement and repeat feel right; info panel and buttons correct.
3. Each game: picture shape, sound, both panels, coin/start, 1P 5 s hold (same game selected afterwards), 2P 5 s restart.
4. Controllers: the detected list (are both panels there, with different ports?), assign, swap, per-action buttons, calibrate, test. Especially: is Player 2 Player 2, and is the 2P button never a gameplay button?
5. Volume: double press 1P and 2P in the library and in a game; mute really silent; level kept after a restart of Zen1.
6. Menu music on INSERT COIN and the library; it fades out for a game and comes back after.
7. Smart fit: after 10 s of a game, does the picture fill the screen better without cutting the score?
8. A CPS-2 game (with qsound.zip if you have it) and 1942.

## Testing status

| Where | What has been checked |
| --- | --- |
| Build server | Compiles. Unit tests pass (68000 boot and interrupts, CPS-1 inputs and sprites, Pac-Man Z80 interrupts, tiles and sound, ROM layouts, controller rules, calibration). The engine runs Zen1's own CPS-1 test program without a screen. Every frontend screen loads with no QML errors, and screenshots and the intro video were rendered there. |
| Real games | **Not verified yet.** No commercial ROMs are used on the build server. |
| Steam Deck / cabinet / panels | **Not tested yet.** |

## Steam and controllers

Zen1 reads the physical panels directly. If the panels do not respond while
Steam is running, note it in your report; a Steam controller setting may
need changing.

## Hardware check (zen1-diag)

`zen1-diag` (on the release page) is a read-only report of what Linux sees:
panels, screens and Steam's Player 1/2 names.

```
chmod +x ~/zen1-diag
~/zen1-diag                 # report, also saved to ~/zen1-diagnostic.txt
~/zen1-diag --watch 60      # live button test
```
