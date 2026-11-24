# Zen1 Arcade on the Steam Deck

Everything here is done in **Desktop Mode** (Steam button → Power →
Switch to Desktop). Zen1 Arcade never changes how the Deck boots.

## Where things live

| What | Location on the Deck |
| --- | --- |
| Your games, saves, settings, artwork | `/home/deck/Zen1Arcade/` (Dolphin: Home → Zen1Arcade) |
| Put ROM sets here | `/home/deck/Zen1Arcade/Games/` (zip files, not extracted) |
| The program | e.g. `/home/deck/Applications/Zen1Arcade-x86_64.AppImage` |

Updating Zen1 Arcade = replacing the AppImage file. The `Zen1Arcade` folder
is never deleted, moved or overwritten by an update.

## 1. Hardware check (zen1-diag)

`zen1-diag` is a single, read-only program that reports what Linux sees:
your two panels (names, USB IDs, serials, port paths), your screens
(including the 1280 × 1024 cabinet screen) and any "Player 1"/"Player 2"
names stored in Steam's settings. It changes nothing unless you ask it to
save bindings or create the data folder.

1. Copy `zen1-diag` to your Home folder.
2. Open **Konsole** and run:

   ```
   chmod +x ~/zen1-diag
   ~/zen1-diag
   ```

   The report is printed and saved to `~/zen1-diagnostic.txt`.
3. Live input test: `~/zen1-diag --watch 60`, then press every button and
   move both sticks. Each line shows which panel and which button.
4. Optional: `~/zen1-diag --bind-players` asks you to press a button on
   Player 1's panel, then Player 2's, and saves that in
   `~/Zen1Arcade/Configurations/controls.toml`.

## 2. Running Zen1 Arcade (Phase 2 shell)

1. Copy `Zen1Arcade-x86_64.AppImage` to `~/Applications/` (create the folder
   if needed).
2. In Dolphin: right-click it → Properties → Permissions → tick
   *Is executable* (or in Konsole: `chmod +x ~/Applications/Zen1Arcade-x86_64.AppImage`).
3. Double-click it. On first start it creates `~/Zen1Arcade/` and opens
   fullscreen on the 1280 × 1024 screen.

What Phase 2 does:

- Shows the data folder, chosen screen, and Player 1 / Player 2 status.
- **Any Player 1 button** opens the **test card** (the engine, fullscreen):
  - the circle must look **round** on the cabinet (aspect ratio correct)
  - the white border must be visible on all four sides
  - the pink bar should glide smoothly
  - each panel's stick and buttons light up live (P1 left, P2 right)
  - **hold any button on both panels for 3 seconds** to return
- Holding a button on both panels for 3 seconds in the menu closes Zen1
  Arcade.

There are no games yet: emulation (CPS1) is Phase 4.

## Steam and controllers

Zen1 reads the physical panels directly and ignores Steam's virtual
gamepads. If the panels do not respond while Steam is running, note it in
the diagnostic report; it may need a Steam controller setting changed.
