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
- **Scenery**: a full-screen background made for that game,
  `Artwork/Scenery/<set>.png` (or `.bmp`), scaled to cover the screen
  without stretching. Today the image is supplied by the user (drawn,
  edited, or generated with any tool); see "AI scenery" below for the
  planned generator. Falls back to Zen1 neon when there is no image.
- **Black**, or **my bezels** (`Artwork/Bezels/<set>.bmp`).

## Picture position

SETUP > Display > *Picture position* (and per game): **auto**, centre, top
or bottom. Auto is centred, except with the Scenery surround, where a wide
game sits at the bottom so the scenery shows above it. Moving the picture
never changes its size or shape. (Unit-tested; headless run checked on the
build server with a test image.)

## AI scenery: investigation (November 2026)

Goal: when a game does not fill the 5:4 cabinet screen, extend the scene
into the free space (above, below or beside the picture) so it looks
designed, never touching the game picture itself.

**What is in Zen1 now** (the parts that do not need AI): Smart fit finds
the real picture area; *Picture position* moves it (bottom for a wide
game leaves the space above); the *Scenery* surround draws a cached
per-game image behind it. The game is always drawn last, from the
original frame, so nothing can cover the picture, the HUD or any text.
A generated scenery image would simply be another file in
`Artwork/Scenery`.

**Generation options, local first:**

| Technique | Licence | What it does well | Cost on the Steam Deck (estimate, not measured) |
| --- | --- | --- | --- |
| Edge extension / blur (Zen1 "game glow", done) | Zen1 | colour continuity, always safe | < 1 ms a frame |
| LaMa inpainting (big-lama, 51 M parameters, ONNX) | Apache-2.0 (Places2 data, CC-BY) | extends textures and backgrounds across large masks; fast; trained on photos, not pixel art | seconds per image on CPU |
| Stable Diffusion inpainting/outpainting through stable-diffusion.cpp (C/C++, Vulkan backend, MIT) with SD 1.5 inpainting or newer models | engine MIT; model licences vary (SD 1.5: CreativeML OpenRAIL-M, with use restrictions) | invents new matching scenery from a prompt plus the game frame | about one to a few minutes per 512x512 image on the Deck's GPU; 2-4 GB memory |
| Cloud generation (optional, user's own key) | service terms | best quality | network, cost; never required |

Notes: the Deck's GPU is an 8-unit RDNA 2 part, so diffusion works but
slowly; a desktop GPU is many times faster. Vulkan results on some AMD
integrated GPUs have been reported as broken in stable-diffusion.cpp,
so this needs testing on the Deck before relying on it.

**Proposed pipeline (not implemented):**

1. After Smart fit has learned the picture area, Zen1 saves a few clean
   reference frames of the game (title screen, gameplay) in
   `Artwork/Scenery/<set>/frames/`.
2. Layout: from the screen and picture shapes, Zen1 picks where the free
   space is (above for a wide game at the bottom; both sides for a
   vertical game) and builds a mask that is exactly the area outside the
   picture plus a small blend margin.
3. Generation runs **outside gameplay**: from SETUP ("Make scenery for
   this game"), in a separate low-priority process, never while a game is
   running. Pixel-art games are generated at low resolution and enlarged
   with nearest-neighbour so the scenery matches the pixels.
4. The result and its settings (model, seed, prompt, source frame,
   layout) are saved as `Artwork/Scenery/<set>.png` + `.json`; nothing is
   regenerated at launch. Keep / regenerate / delete from SETUP; a user
   image always wins.
5. Runtime cost: the same as any surround image (one texture draw).

**Expected quality, honestly:** convincing for games with one dominant
look (a shooter's starfield, a fighting game's stage). Weak where the
scene changes all the time (each fighting-game stage is different: one
image per stage would need stage detection). Diffusion models may invent
details that do not match the art style; that is why a user image or the
plain glow stays the default.

Not tested on the Steam Deck or the cabinet.

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
