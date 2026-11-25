# Zen1 Arcade — Controls

Status: **proposed.** Not implemented or tested.

## The cabinet layout

Each player has exactly this, and Zen1 stores it as the **logical cabinet layout**:

| Control | Player 1 | Player 2 |
| --- | --- | --- |
| 8-way stick | Up, Down, Left, Right (diagonals are two directions at once) | same |
| Top row | Light Punch · Medium Punch · Heavy Punch | same |
| Bottom row | Light Kick · Medium Kick · Heavy Kick | same |
| Player button (7th) | **P1** — starts a one-player game | **P2** — starts/joins a two-player game |

The player buttons are **not** action buttons. They map to each game's
*Start* input for that player, never to a punch or kick.

### The 1P / 2P buttons

Each dedicated player button has three gestures (`libs/input` `ButtonGesture`,
unit-tested). They work the same in the menus and in every game, and they
never reach the game as a gameplay button:

| Gesture | 1P | 2P |
| --- | --- | --- |
| Single press | Coin + Start for Player 1 | Coin + Start for Player 2 |
| Double press (second press within 0.35 s) | Volume down: 100 → 50 → 0 % (stays at 0) | Volume up: 0 → 50 → 100 % (stays at 100) |
| Hold 5 s (progress bar) | End the game, back to the library | Restart the game |

- A single press is only acted on once the double-press window has passed
  (about a third of a second), so a double press never inserts a coin.
- A hold released before 5 s does nothing; a long hold never changes the
  volume; the second press of a double press never becomes a hold.
- Switch bounce shorter than 25 ms is ignored.
- Hold **1P + 2P together for 3 s** in the menus to leave Zen1 Arcade.

The volume has exactly three levels (0 % muted, 50 % low, 100 % full). It
applies to the game sound, the menu music and the menu sounds, is saved in
`Configurations/settings.toml` (`audio.volume`) and shows a large
"VOLUME 50%" panel for two seconds.

### Console games

Console pads are laid on the same panel. The bottom row carries a pad's main
buttons; on two- and three-button pads the top row repeats them, so either
row works. The player button is the pad's Start (Run on PC Engine, Pause on
Master System); a single press reaches the game as Start, double presses
and 5-second holds keep their usual meaning and never reach the game.

| System | Top row (LP MP HP) | Bottom row (LK MK HK) |
| --- | --- | --- |
| NES, Game Boy, Game Boy Color | B, A, Select | B, A, Select |
| Master System, Game Gear, SG-1000 | 1, 2, - | 1, 2, - |
| PC Engine | II, I, Select | II, I, Select |
| SNES | Y, X, L | B, A, R |
| Mega Drive (six-button pad) | X, Y, Z | A, B, C |

Not on the panel: SNES Select and the Mega Drive Mode button. Game Boy,
Game Gear and other handhelds are one-player.

## Identifying Player 1 and Player 2

Rule: **never assign players by detection order.** Assignment uses, in order:

1. **A saved binding** for that exact device: physical connection path
   (`/dev/input/by-path/…`, stable while the panel stays in the same dock
   port), USB vendor/product ID, and serial number when the device has one.
2. **The device name.** If a device's name is "Player 1" or "Player 2", it
   is assigned automatically.
3. **The assignment wizard**: "Press any button on Player 1's panel", then
   Player 2's. Shown only when 1 and 2 cannot settle it.

### About the names "Player 1" and "Player 2"

There are two places a name can live, and Phase 2 will find out which one
yours are in:

- **Linux's own device name** comes from the USB device itself. Zen1 can
  always read it. If your panels report "Player 1"/"Player 2" here, step 2
  above works directly.
- **A name given in Steam's controller settings** is stored by Steam in its
  own configuration files, not in Linux. Linux and other programs still see
  the USB name (often something like "DragonRise Inc. Generic USB
  Joystick"). Zen1 will try to read Steam's stored names (read-only) and
  match them to devices by vendor/product ID and serial. If that is not
  reliable, the saved binding from step 1 does the job instead.

Phase 2 ships a diagnostic screen (and `zen1-arcade --list-input-devices`)
that shows, for each connected device: name, vendor/product ID, serial,
connection path, and whether it is a Steam virtual device.

### Steam's virtual controllers

In Desktop Mode Steam can expose a *virtual* gamepad for physical
controllers. Zen1 reads the physical devices and ignores Steam's virtual
ones, so no input is doubled or swapped. Verified on the Deck in Phase 2.

### Reconnecting

- A bound panel that disconnects pauses that player and shows
  "Player 2 controller disconnected". When it comes back on the same port
  (or with the same identity), the binding is restored automatically.
- A new, unknown device is never assigned automatically during a game.
- Player 1 never drives Player 2's inputs or the reverse; each binding
  belongs to one player only.

## Menus

- Player 1 drives the menus: stick to move, Light Punch to choose, Light
  Kick to go back.
- With no Player 1 panel assigned, the Steam Deck's own controls drive the
  menus (A choose, B back, Start = 1P button, View = 2P button). They are
  never assigned as a cabinet player.

## Control screens (SETUP > Controllers)

- **Choose from detected list**: every controller Linux reports, with its
  name, USB vendor:product, serial number (or "no serial"), button/axis
  count and port. Pick one and assign it to **Player 1**, **Player 2** or
  **both** (for a single encoder board wired to both panels; then each
  player has its own button layout on that board).
- **Assign by pressing**, **Swap**, **Unassign** (an unassigned player stays
  unassigned even if the panel's name says "Player 1"), **Detect again**.
  Assignments are found again after a reboot by USB port, then serial
  number, then model, never by detection order.
- **Player 1 / Player 2 buttons**: one row per action (up, down, left,
  right, buttons 1-6, coin, start, the player button, volume down/up, exit
  game, restart game) showing the current input. Choose a row and press
  the new button, stick direction, hat or axis; the detected input is shown
  straight away. If it is already used, Zen1 says for what and asks for a
  second press to confirm. MP clears a row. Also: quick setup (six buttons
  and the player button in order), reset to defaults, swap the two layouts.
  Cancel with Esc or by holding B on the Steam Deck for 1.5 s. With no
  press at all it stops after 60 s and says so.
- **Assign by pressing** listens only to arcade panels and game
  controllers, and lists them on screen. The Steam Deck's own controls and
  Steam's virtual controllers are never assigned. In Game Mode Steam
  mirrors every controller as a virtual one, so a press also arrives there;
  that no longer cancels the assignment (it did before 0.6.1, which is why
  assigning said "Cancelled"). If a press reaches only Steam's virtual
  controller, Zen1 says so: Steam Input is then hiding the real device
  (in Steam: Zen1 Arcade > Controller settings > Disable Steam Input). If
  no panel is connected, or Zen1 may not read it, it says that at once.
- System functions (player button, volume, exit, restart) are never sent
  to the game, even if the same button is also mapped as a gameplay button.
- **Calibrate** each stick, **Controller test** (live), **Reset all**.

Everything is saved in `Configurations/controls.toml`
(`[player1]`, `[player1_map]`, ...). Files from 0.3/0.4 are still read.

Controls are entirely Zen1's own system; no other emulator's controller
settings are involved.
