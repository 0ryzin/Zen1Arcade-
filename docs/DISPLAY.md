# Zen1 Arcade — Display

Status: **proposed.** Not implemented or tested.

## Rule

Every game keeps its original proportions, is made as large as the
1280 × 1024 screen allows, and the space around it is **designed, not
empty**. Nothing is ever stretched by Zen1 itself.

## The screen

- Target: **1280 × 1024, 5:4**, 60 Hz, the cabinet screen connected through
  the Steam Deck dock.
- Zen1 detects the connected outputs and their modes. When the Deck's own
  panel is also active, Zen1 asks once which screen is the cabinet and
  remembers it.
- Zen1 always renders at the screen's native mode. It never switches the
  screen to another resolution to launch a game (under Wayland,
  applications don't change modes at all). All scaling happens on the GPU.
- First-run calibration shows a grid of circles: if they look round, the
  panel's shape is confirmed.

## How games are fitted to 1280 × 1024

| Game | Native | Shown on 1280 × 1024 (default) | Space around it |
| --- | --- | --- | --- |
| *Street Fighter II* (CPS1), most horizontal arcade games | 384 × 224, displayed 4:3 | **1280 × 960**, centred | 32-pixel bands top and bottom |
| Same, with integer scaling | 4 × 224 = 896 lines | 1195 × 896 | Bands on all sides |
| Vertical games (e.g. *1941* on CPS1) | 224 × 384, displayed 3:4 | **768 × 1024**, centred | 256-pixel panels left and right |
| A 16:9 widescreen image | e.g. 1920 × 1080 | **1280 × 720** | 152-pixel bands top and bottom |

### About the *Street Fighter II* problem you saw

*Ultra Street Fighter II: The Final Challengers* is a 2017 Nintendo Switch
game, not an arcade board. It renders a 16:9 HD picture, so on a 5:4 screen
a correct fit leaves 152-pixel bands top and bottom (last row above), and
filling them would stretch the fighters. Zen1's answer is to keep the
picture correct and fill the bands with presentation (below).

Note that the Switch game itself is outside Zen1's scope (Zen1 emulates
arcade hardware). The arcade originals, such as *Street Fighter II* on
CPS1, are 4:3 and fit the 5:4 cabinet far better.

## Filling the unused space

Presentation layers drawn behind/around the game, chosen per game or globally:

1. **Game-specific bezel artwork** (if present in `Artwork/bezels/`).
2. **Zen1 default bezel** designed for 5:4, with marquee art in the top band
   and control hints in the bottom band.
3. **Blurred, darkened game artwork** filling the space.
4. **Solid colour** (configurable), for purists.

Default: 1 if available, otherwise 2. The game image itself is never
covered by any layer.

## Smart fit (default)

Zen1 learns where each game really draws. While a game runs it looks at one
frame in 15, every second pixel, and keeps the box of everything that has
ever lit up since the game started: score, lives and credits included. Rows
and columns that have stayed black are border. After 10 seconds, if the
border is worth removing (more than 3 % of a side), the picture is fitted
from that box instead of the whole frame. The box only ever grows, so a
HUD that appears later is never cut; never more than a quarter of a side
is removed. The result is saved per game (`Configurations/games/<set>.toml`,
`display.crop`), so the next launch starts with it.

The box keeps its original pixel shape (aspect ratio). It is scaled to a
whole multiple of its lines when that fills at least 90 % of the best fit
(sharp, even scanlines), otherwise to the best fit. It is never stretched.
On the 1280x1024 cabinet, a CPS-1 game shows at 4x (1195x896); a vertical
game is fitted to the full height.

Why not an AI model: the question "which part of this frame is the game"
has an exact, cheap answer for arcade games (black is black), and a model
would cost Steam Deck power and frame time for no gain. The tracker costs
well under a millisecond every quarter of a second.

## Surround

- **Zen1 neon** (default): the neon grid backdrop and a glow round the picture.
- **Game glow**: the game itself, shrunk to 24x18 pixels and smoothed back up,
  darkened: a soft, moving glow of its own colours.
- **Black**, or **my bezels** (`Artwork/Bezels/<set>.bmp`).

## Per-game settings

SETUP > Display: picture and surround for the selected game ("same as all"
or any mode), and **Reset display for this game**, which forgets the
learned picture area too.

## Scaling modes

| Mode | Behaviour |
| --- | --- |
| Smart fit (default) | Learned picture area, whole multiples when they fill 90 %, otherwise best fit |
| Original shape | Largest size that keeps the original aspect ratio of the whole frame |
| Integer | Largest whole-number multiple of the native lines |
| Pixel perfect | Whole-number multiple in both directions, square pixels |
| Stretch | Only by editing a config file; not offered in SETUP |

Rotation per game, and refresh handling (CPS-1 runs at about 59.64 Hz;
Zen1 runs one game frame per display refresh when they are within 1.5 %).

Tested on the build server only, at 1280x1024 and other sizes in unit
tests and headless runs: not yet on the Deck's own 800x1280 screen, a
docked TV or the cabinet.

## Safety (planned; not implemented yet)

- Any display change that could leave the screen unusable is applied with a
  **10-second countdown: "Keep this setting? Press P1"**. No press = the
  previous setting comes back automatically.
- If a game's display plan fails (no frames, presenter error), Zen1
  relaunches it once with the last known-good plan and marks the override.
- At start-up, holding **P1 + P2 for 5 seconds** resets all display
  settings to safe defaults. No keyboard is ever required.
