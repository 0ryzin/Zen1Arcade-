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
| Namco Pac-Man | pacman (needs puckman.zip too), puckman, mspacman |

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
4. **In a game:**
   - 1P / 2P (short press): coin + start / join.
   - **Hold 1P for 5 seconds:** back to the library, on the same game.
   - **Hold 2P for 5 seconds:** restart the game.
   - A bar shows while you hold; let go to cancel. The holds never reach the
     game.
5. **Leave Zen1 Arcade:** SETUP → Exit, or hold both player buttons for 3
   seconds in the menus.

Keyboard (development, and the Deck's own controls in Desktop Mode, which
type keys there): arrows move, Enter = LP, Esc = back, 1 = 1P button.
Inside games without panels, the Deck's controls stand in for Player 1
(X Y RB = LP MP HP, A B RT = LK MK HK, Start = 1P, View = 2P).

## 4. Panels: SETUP → Controllers

- **Assign Player 1 / Player 2 panel:** press any button on the panel you
  mean. It is saved by USB port, serial and name, so plug-in order never
  matters and reconnecting keeps it.
- **Swap**, **Unassign**.
- **Set buttons:** press LP, MP, HP, LK, MK, HK, then the player button.
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
Games keep their original shape (CPS1: 4:3 → 1280 × 960 on the cabinet, with
a neon surround in the 32-pixel bands). Settings → Game picture offers
integer, pixel-perfect and stretch. Per-game overrides go in
`Configurations/games/<set>.toml`, for example:

```
[display]
scale = "integer"
rotate = 0        # extra rotation on top of the game's own
bezel = "black"   # neon | black | artwork
```

Your own bezel: `Artwork/Bezels/<set>.bmp` or `Artwork/Bezels/default.bmp`
(BMP, screen-sized), with Screen surround = MY BEZELS.

**Artwork:** with SETUP → Library → *Download artwork automatically* on,
Zen1 fetches the flyer, title and in-game pictures for the games you have
and keeps them offline in `Artwork/downloaded`. Your own pictures always
win: `Artwork/<set>/card.png` (also `flyer`, `boxart`, `title`, `snap`,
`marquee`, `.png` or `.jpg`). *Next artwork* cycles the pictures for the
selected game, and *Reset artwork* goes back to automatic.

## 6. What to test and report

Please send `~/Zen1Arcade/Logs/` (newest files) with any report.

1. Start-up: animation and sound on the cabinet screen; INSERT COIN waits; coin sound.
2. Library: your zips appear; stick movement and repeat feel right; info panel and buttons correct.
3. Each game: picture shape, sound, both panels, coin/start, 1P 5 s hold (same game selected afterwards), 2P 5 s restart.
4. Controllers: assign, swap, set buttons, calibrate, test.

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
