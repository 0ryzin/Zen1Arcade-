Automatic build of `main`.

**New in this build**
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
- Real games: **none verified yet**. No commercial ROMs are used on the build server.
- Steam Deck, cabinet and panels: **not tested yet**.

ROM sets must match current MAME versions (e.g. `sf2.zip`, `ghouls.zip`, `pacman.zip` + `puckman.zip`, `mspacman.zip`) in `~/Zen1Arcade/Games`. Zen1 Arcade does not include or download games.
