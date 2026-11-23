# Mime Notes

A minimalist Linux desktop plain-text notepad built with C++17 and Qt6, matching the Figma design specification (`503×477`, `#212421` background).

## Features

- **Exact Figma layout & colors** (`themes/default.json`):
  - `503 × 477` window size with `#212421` background.
  - Top bar with `14px` Semi-Bold tabs (`x = 20, y = 10`) and `+` button (`x = 479, y = 9`).
  - Bottom-left color bar (`x = 20, y = 445`, `96 × 12`) with four `12 × 12` circular color buttons spaced `16px` apart:
    1. `#FFFFFF` (White)
    2. `#B4C9AE` (Sage Green)
    3. `#C3B7DB` (Soft Lavender)
    4. `#A8C9DA` (Sky Blue)
- **Live typing color (`jj` chord or click)**: Click any of the 4 bottom-left color buttons or type `jj` quickly followed by `j`, `k`, `l`, or `;` to switch the active typing color (and recolor any active selection).
- **16px default editor font**: Clean `16px` body text (`Ctrl+0` resets to the theme's `editor.font_size`).
- **Auto-divider (`---`)**: Typing `---` automatically breaks to a new line, renders a full-width horizontal divider across the note, and moves the cursor to a new line below it (while saving cleanly as `---` in the underlying `.txt` file). Pressing `Backspace` right after a divider removes the divider in a single keystroke.
- **Hot-reloading `config.json` & `themes/`**: Editing and saving `config.json` or the active theme JSON file in `themes/` live-updates the running window immediately.
- **Folder-backed tabs & auto-save**: Each tab is a `.txt` file inside `~/Notes/` (or the path in `config.json` / `--dir <path>`). Saves automatically 300ms after typing stops and immediately on tab switch, rename, close, focus loss, or exit.

## Configuration (`config.json`)

Mime Notes looks for `config.json` in:
1. `--config <path>` (or `$MIMENOTES_CONFIG`)
2. `./config.json` (current directory or binary directory)
3. `~/.config/mimenotes/config.json`

Example [config.json](file:///home/psurrena/Projects/notes/config.json):

```json
{
  "theme": "default",
  "notes_dir": "~/Notes",
  "keybindings": {
    "new_tab": "Ctrl+T",
    "new_tab_alt": "Ctrl+N",
    "close_tab": "Ctrl+W",
    "reopen_closed_tab": "Ctrl+Shift+T",
    "save": "Ctrl+S",
    "rename_tab": "F2",
    "rename_tab_alt": "Ctrl+R",
    "next_tab": "Ctrl+Tab",
    "next_tab_alt": "Ctrl+PgDown",
    "prev_tab": "Ctrl+Shift+Tab",
    "prev_tab_alt": "Ctrl+PgUp",
    "zoom_in": "Ctrl+=",
    "zoom_out": "Ctrl+-",
    "zoom_reset": "Ctrl+0",
    "divider_trigger": "---",
    "color_mode_trigger": "jj",
    "color_mode_timeout_ms": 350,
    "color_keys": ["j", "k", "l", ";"]
  }
}
```

## Themes (`themes/*.json`)

Themes live in the `themes/` folder and control window dimensions, background/selection/divider/scrollbar colors, header and editor font families, font weights, font sizes, and the 4 typing colors:

- [themes/default.json](file:///home/psurrena/Projects/notes/themes/default.json) — Figma dark theme (`#212421`, `Inter` `16px` editor / `14px` Semi-Bold header).
- [themes/monospace-dark.json](file:///home/psurrena/Projects/notes/themes/monospace-dark.json) — Dark monospace theme (`JetBrainsMono Nerd Font`, `16px`).
- [themes/paper-light.json](file:///home/psurrena/Projects/notes/themes/paper-light.json) — Warm light paper theme (`#F7F4EE`, `Inter` `16px`).

Switch themes either by changing `"theme"` in `config.json` or passing `--theme <name>`:

```bash
./mimenotes --theme monospace-dark
./mimenotes --theme paper-light
```

## Build & Run

```bash
make
./mimenotes
```
