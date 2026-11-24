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

Zen1 Arcade runs only games whose hardware its own engine implements. Right
now that is **Capcom CPS1**, with these four games:

| Game | File(s) needed in `~/Zen1Arcade/Games` |
| --- | --- |
| Street Fighter II: The World Warrior | `sf2.zip` |
| Street Fighter II': Champion Edition | `sf2ce.zip` **and** `sf2.zip` |
| Final Fight | `ffight.zip` |
| 1941: Counter Attack (vertical) | `1941.zip` |

The zips must match **current MAME** naming and versions; files are checked
by CRC. Subfolders are fine (up to four levels). Anything else in the folder
shows in the library as **NOT SUPPORTED**. A supported game with missing or
different files shows as **ROMS NEEDED**, and the bottom panel says which
files are wrong. Zen1 Arcade does not include or download ROMs.

## 3. Using it

- **Intro** plays, then the **library** opens (5 games per row).
- Player 1 drives the menus:

  | Control | In the library |
  | --- | --- |
  | Stick | move (up from the first row reaches the collections) |
  | LP or the player button | play |
  | MP | favourite on/off |
  | HP | search (letter grid) |
  | MK / HK | previous / next collection |
  | LK | back |

  The collections row ends with **SETTINGS** (intro, picture, surround,
  frame timing, controllers, test card, rescan, exit).
- **In a game:** the player button inserts a coin and presses start (press
  it again to join as Player 2 on the other panel). **Hold both player
  buttons for 2 seconds** (or one for 4 seconds) to return to the library.
- **Leave Zen1 Arcade:** hold both player buttons for 3 seconds in the
  library, or Settings → Exit.

Keyboard (development, and the Deck's controls in Desktop Mode, which type
keys there): arrows move, Enter = LP, Esc = LK/back, F = MP, / = HP,
Q/E = MK/HK, 1 = player button.

## 4. Panels (Player 1 / Player 2)

Settings → **Controllers**:

1. *Assign the Player 1 panel*, then press any button on the left panel.
   Do the same for Player 2. This is saved by USB port, serial and name;
   plug order never matters again.
2. *Set Player 1 buttons*: press LP, MP, HP, LK, MK, HK and then the player
   button, in that order. Same for Player 2.
3. *Cabinet test card* shows both panels live, with the arcade layout and
   raw button numbers.

Swap and Reset are on the same screen. Without panels, the Steam Deck's own
controls (or a keyboard) stand in for Player 1 **inside games only**, so you
can test tonight with the panels unplugged; they are never saved as a player.
Deck layout in games: X Y RB = LP MP HP, A B RT = LK MK HK, Start = player
button, Start + View held 2 s = back to the library.

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
(BMP, screen-sized), with Screen surround = MY BEZEL ARTWORK. Library card
art: `Artwork/<set>/card.png` (also `flyer`, `boxart`, `title`, `snap`, or
`Artwork/<set>.png`).

## 6. What to test and report

Please send `~/Zen1Arcade/Logs/` (newest files) with any report.

1. Intro: plays on the cabinet screen, sound OK, skip works.
2. Library: your zips show as READY.
3. Each game: starts, picture right (shape, no cropping), sound, both
   panels, coin/start, returning to the library, no slow-down or crackle.
4. Test card: circle round, corner arrows visible, bar glides, every
   button lights.

## Testing status

| Where | What has been checked |
| --- | --- |
| Build server | Compiles; 43 unit tests (CPU boot and interrupts, input ports, sprite drawing, ROM loading layouts, controller rules, display maths); the engine runs Zen1's own CPS1 self-test program headless; frontend screens and the intro rendered headless (screenshots and intro video on the release page). |
| Real games | **Not verified yet.** No commercial ROMs are used on the build server. |
| Steam Deck / cabinet | **Not tested yet.** |

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
