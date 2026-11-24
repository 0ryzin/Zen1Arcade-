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

### Coins

Arcade games expect coins and the cabinet has no coin button. Default:
**pressing a player button adds a credit for that player first, then
starts**, so it behaves like a free-play machine. Alternatives (setting):
use the game's own free-play DIP switch where it has one, or hold a player
button for one second to insert a coin without starting.

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

- Player 1 navigates. Default: stick to move, Light Punch = confirm,
  Medium Punch = back, Heavy Punch = game details, Light Kick = favourite,
  P1 button = menu.
- Player 2 stays free for gameplay. "Shared navigation" (Player 2 may also
  move menus) is an option, off by default.

## Leaving a game

Hold **P1 + P2 buttons together for 2 seconds** (configurable). A ring fills
from 0.5 s so an accidental press can be released. No keyboard needed.

## Control screens

- **Cabinet view**: both panels drawn as they look, every control labelled.
- **Bind**: select a control, press the physical input. Timeout after 5 s.
- **Test**: every input lights up live, stick shown as one of 8 directions.
- **Swap players**, **Clear**, **Reset to defaults**, **Re-detect**.
- **Per-game overrides**: e.g. a game that uses only two buttons, or a game
  whose button order differs. Saved in that game's config; the global layout
  is unchanged.

Controls are entirely Zen1's own system; no other emulator's controller
settings are involved.
