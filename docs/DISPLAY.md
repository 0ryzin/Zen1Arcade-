# Zen1 Arcade — Display

Status: **proposed.** Not implemented or tested.

## Rule

Every game keeps its original proportions, is made as large as the
1280 × 1024 screen allows, and the space around it is **designed, not
empty**. Nothing is stretched unless the user chooses "Stretch" for a game.

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

### Future, experimental: intelligent screen expansion

Not part of the first release, and it will not delay it. The idea: fill the
unused screen area by extending the game's surroundings (image analysis,
background reconstruction, or AI-assisted generation) instead of using a
bezel.

How the presenter is designed so this can be added later without
changes elsewhere:

- The presenter draws two independent layers: the **game layer** (the
  untouched game frame, scaled per the display plan) and the **surround
  layer** (everything outside the game rectangle).
- Surround content comes from a pluggable *surround source*: bezel,
  blurred artwork, solid colour today; an expansion generator later.
- A surround source can only write **outside** the game rectangle. The
  game layer is drawn last, on top, from the original frame, so the game
  image can never be altered or covered.
- Surround sources run asynchronously and may update slower than the game.
  If one is late, the previous surround image is reused. The game frame
  never waits for it, so it cannot add input lag.
- An expansion source would be off by default, per game, and labelled
  experimental. It is disabled automatically if frame times suffer.

## Scaling modes

| Mode | Behaviour |
| --- | --- |
| Original (default) | Largest size that keeps the original aspect ratio |
| Integer | Largest whole-number multiple of the native lines; sharpest pixels, slightly smaller |
| Pixel-perfect | Integer scaling in both directions with square pixels (for games meant to be shown that way) |
| Custom | User-set zoom and aspect, saved per game |
| Stretch | Fills the screen; only when explicitly chosen |

Plus per game: rotation (0°/90°/180°/270°), optional smoothing, optional
scanlines/CRT presets, and native-refresh handling (CPS1 runs at ~59.63 Hz;
Zen1 syncs audio so it stays smooth on 60 Hz).

## Safety

- Any display change that could leave the screen unusable is applied with a
  **10-second countdown: "Keep this setting? Press P1"**. No press = the
  previous setting comes back automatically.
- If a game's display plan fails (no frames, presenter error), Zen1
  relaunches it once with the last known-good plan and marks the override.
- At start-up, holding **P1 + P2 for 5 seconds** resets all display
  settings to safe defaults. No keyboard is ever required.
