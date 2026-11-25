Automatic build of `main`.

**New in 0.6**
- **Home consoles and handhelds** in the same library: NES/Famicom, SNES/Super Famicom, Game Boy, Game Boy Color, Master System, Game Gear, SG-1000, Mega Drive/Genesis and PC Engine/TurboGrafx-16 HuCards. Put cartridge files (or a .zip holding one) anywhere in `Games`. Each system is a Zen1 machine module around an open-source core compiled into Zen1 (Gearboy, Gearsystem, Geargrafx, QuickNES, LakeSnes, clownmdemu): no other emulator is started. Per-system pad layouts on the panel, battery saves, box art from libretro-thumbnails. Verified with test and homebrew cartridges only (see docs/COMPATIBILITY.md); no commercial console game has been tried yet. GBA, N64, Atari 2600/7800 and 32X files are recognised and explained, not playable.
- **Street Fighter II' Champion Edition**: `sf2ce.zip` no longer reports missing files when it is a correct set. Zen1 had sf2ce as a clone of sf2 (it is a parent in MAME) and knew only one revision. All CPS-1 revisions are now known (136 sets), a zip is identified by its contents (any name, any ROM-set version), shared files are found in any zip in Games or BIOS, and a missing parent is named.
- **Start-up**: one scene. The intro (ZEN1 assembling from glowing pixels in a dark, nebula-lit space) settles into INSERT COIN without a zoom or a second screen; a button press opens the library.
- **New menu music**: "Night Signal", an original 40-second loop (D minor, 96 BPM): breathing pads, pulsing bass, a plucked arpeggio, bell calls and a lead melody. Same on/off, volume and fades as before.

**Earlier in 0.5**
- **Picture position** (auto, centre, top, bottom; global and per game) and a **Scenery** surround that shows your own full-screen background for a game (`Artwork/Scenery/<set>.png`) around the picture, never over it.
- `docs/SWITCH.md`: research on Switch 1 emulation (Ryujinx/Ryubing, Eden), feasibility, risks and a roadmap. No Switch emulation is in Zen1.
- `docs/DISPLAY.md`: investigation of AI-generated scenery (local options, performance, caching). Not implemented yet.
- **Capcom CPS-2** in Zen1's own engine: 291 sets (Super Street Fighter II Turbo, Street Fighter Alpha 1-3, Darkstalkers / Vampire Savior, X-Men, Marvel Super Heroes, Marvel vs. Capcom, D&D Tower of Doom / Shadow over Mystara, Alien vs. Predator, Armored Warriors, 19XX, 1944, Progear, Giga Wing, Super Puzzle Fighter II Turbo...). Uses each set's own `.key` file (current MAME sets include it). For exact QSound sound add MAME's `qsound.zip`; without it Zen1 plays every voice with its own simpler tables.
- **Capcom 1942** (and three clones).
- **1P / 2P buttons**: single press = coin + start, **double press = volume** (1P down, 2P up; exactly 0 / 50 / 100 %, with a big VOLUME panel), hold 5 s = back to the library (1P) / restart (2P). A double press never inserts a coin.
- **Controllers**: a detected-controller list (name, USB IDs, serial, port) to assign Player 1, Player 2 or both; every action remappable (directions, buttons 1-6, coin, start, player button, volume, exit, restart) with conflict confirmation; the Steam Deck's own controls work the menus but are never a player.
- **Menu music**: an original chiptune loop on INSERT COIN, the library and SETUP; fades out for games; on/off and its own volume in SETUP.
- **Smart display fit** (new default): Zen1 learns each game's real picture area, never stretches, uses sharp whole multiples when they fill 90 %. "Game glow" surround. Per-game picture settings and reset.
- **Artwork**: new games in the Games folder are noticed automatically; artwork appears as it downloads; failed lookups are retried; choose your own image with the stick (from Artwork/import, Pictures, Downloads); look online again; back to automatic.
- **Switch packages** (.nsp/.xci) are listed (name and title ID from their headers and file name) as NOT SUPPORTED. Zen1 cannot run Switch games.

**Earlier in 0.4**
- **Two new arcade systems in Zen1's own engine:**
  - **SNK Neo Geo MVS**: 122 games, including Metal Slug and Metal Slug 2, The King of Fighters '94-'97, Samurai Shodown 1-4, Fatal Fury, Neo Bomberman, Puzzle Bobble, Blazing Star and Pulstar. Needs `neogeo.zip` in Games or BIOS. Encrypted later games (Metal Slug X and later, KOF '98 and later, Garou) are not supported yet.
  - **Namco Galaxian**: Galaxian (Namco, Midway and Taito sets) and Super Galaxians.
- **Compatibility list**: `docs/COMPATIBILITY.md` lists every supported game with hardware, players, joystick, buttons and whether it has been tested on a real ROM set (none yet).
- Neo Geo settings and high scores are saved in `~/Zen1Arcade/Saves`.
- Arcade start-up: the same animation with new arcade sounds (power-up jingle, synth stabs, bleeps, a zap). Then an **INSERT COIN** screen that waits for a button: the coin drops, CREDIT 01, PLAYER 1 START, and the library opens.
- **One library** of every game in your Games folder, artwork first, 4 per row. There are no categories, favourites or search. Push up from the top row for **SETUP**.
- **Pixel fonts** throughout, plus menu sounds.
- An **info panel** for the selected game: year, maker, hardware, format, players, joystick, buttons per player, and SUPPORTED / NOT SUPPORTED. Each game's own button layout appears on the real panel layout, with what the 1P / 2P buttons do.
- **Controllers:** assign a panel to Player 1 or 2 by pressing a button on it, plus swap, unassign, set buttons, a live controller test, and **joystick calibration** (centre, range, dead zone; digital sticks are detected and only tested).
- **In games:** hold **1P for 5 s** to return to the library on the same game; hold **2P for 5 s** to restart the game. A short press is coin + start. The holds never reach the game.
- **Engine:** 21 Capcom CPS-1 games (17 new, including Ghouls'n Ghosts, Strider, Street Fighter II' Hyper Fighting, Knights of the Round, Captain Commando, The King of Dragons and Mega Man) and the new **Namco Pac-Man hardware** (Pac-Man, Puck Man, Ms. Pac-Man).
- **Artwork:** downloaded automatically for the games you have (can be switched off), cached offline; Change or reset it in SETUP.

**Testing status, please read**
- Build server: everything compiles. The unit tests pass, covering the CPU, video, sound chips, ROM loading, Ms. Pac-Man layout, controller rules and calibration maths. Engine and frontend run there with no screen; screenshots and `zen1-intro-preview.mp4` were made there.
- Real games: **none verified yet**, on any of the six systems. No commercial ROMs or BIOS files are used on the build server; Neo Geo and Galaxian are checked with hand-written test programs and synthetic graphics only.
- Steam Deck, cabinet and panels: **not tested yet**.

ROM sets must match current MAME versions (e.g. `sf2.zip`, `ghouls.zip`, `pacman.zip` + `puckman.zip`, `mspacman.zip`, `galaxian.zip`, `mslug.zip` + `neogeo.zip`) in `~/Zen1Arcade/Games`. Zen1 Arcade does not include or download games.
