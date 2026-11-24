# Zen1 Arcade — Storage

Status: **proposed.** Not implemented or tested.

## Where everything lives

On first launch Zen1 Arcade creates its **data folder**:

```
/home/deck/Zen1Arcade/        (shown as "Zen1Arcade" in your Home folder in Dolphin)
```

It is a normal visible folder in your home directory, so you can open it in
Desktop Mode's file manager and copy games straight in. The location can be
changed in Settings → Library (for example to a microSD card); Zen1 then
offers to move the folder for you.

The program itself is a separate file, e.g.
`/home/deck/Applications/Zen1Arcade.AppImage`.
**Updating Zen1 replaces only that file.** The data folder is never
deleted, moved or rewritten by an update.

## Folder layout

```
Zen1Arcade/
├── README.txt            what each folder is for
├── Games/                drop arcade ROM sets here (zip files, not extracted)
│   └── (any subfolders)  e.g. Games/Capcom/ — Zen1 identifies games by content, not folder
├── BIOS/                 BIOS sets some systems need, supplied by you
├── Saves/                high scores, NVRAM, EEPROM, per game
├── SaveStates/           save states, per game
├── Configurations/
│   ├── settings.toml     application settings
│   ├── controls.toml     cabinet layout and Player 1/2 bindings
│   ├── display.toml      cabinet screen profile
│   └── games/<set>.toml  per-game overrides (controls, display, presentation)
├── Artwork/
│   ├── bezels/  backgrounds/  marquees/  titles/  snaps/  flyers/  logos/
├── Videos/               gameplay preview videos
├── Metadata/
│   └── library.db        library, playtime, favourites, recently played, collections
├── ShaderCache/          rebuilt automatically, safe to delete
└── Logs/                 last 10 sessions
```

## Rules

- **Adding games:** copy files into `Games/` (any subfolder), then choose
  *Refresh library* or restart Zen1. New and changed files are scanned in
  the background; the UI stays responsive.
- **Removing from library** hides a game; it does not delete the file.
  Deleting a file is a separate, confirmed action.
- **Updates and migrations:** the folder carries a version marker
  (`.zen1-data-version`). If a new Zen1 version needs to change a file
  format, it first copies the old files to `Configurations/backup-<date>/`.
- **Offline:** everything works without internet once games and artwork are
  in place. Optional online artwork scraping only ever adds files.
- **Backups:** copying the whole `Zen1Arcade` folder backs up everything.

## Content

Zen1 never bundles or downloads ROMs or BIOS files. You add files you
legally own or are authorised to use.
