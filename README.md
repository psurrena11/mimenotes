# Mime Notes

A minimalist Linux desktop plain-text notepad built with C++17 and Qt6, matching the Figma design specification (`503×477`, `#212421` background).

## Features

- **Clean layout & colors** (`themes/default.json`):
  - `503 × 477` window size with `#212421` background.
  - Top bar with `14px` Semi-Bold tabs (`x = 20, y = 10`) and `+` button (`x = 479, y = 9`).
  - **Bottom-left formatting bar**: 4 icons (`Bold`, `Italic`, `Up Arrow`, `Down Arrow`) for toggling bold/italic and increasing/decreasing font size by `4px` (configurable via `"font_size_step"`).
  - **Bottom-right color bar**: Four `12 × 12` circular color buttons spaced `16px` apart:
    1. `#FFFFFF` (White)
    2. `#B4C9AE` (Sage Green)
    3. `#C3B7DB` (Soft Lavender)
    4. `#A8C9DA` (Sky Blue)
- **Home-row leader commands (`jj`) & quick new tab (`ff`)**:
  - Type `jj` quickly followed by:
    - `a` → Toggle **Bold**
    - `s` → Toggle *Italic*
    - `d` → Increase font size (`+4px`)
    - `f` → Decrease font size (`-4px`)
    - `j` → Color 1 (`#FFFFFF` White)
    - `k` → Color 2 (`#B4C9AE` Sage Green)
    - `l` → Color 3 (`#C3B7DB` Soft Lavender)
    - `;` → Color 4 (`#A8C9DA` Sky Blue)
    - `h` → **Reset** formatting (`16px`, `#FFFFFF` White, no bold/italic)
  - Type `ff` quickly → Create and switch to a **New Tab**
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
  "default_font_size": 16,
  "font_size_step": 4,
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
    "leader_trigger": "jj",
    "leader_timeout_ms": 350,
    "new_tab_trigger": "ff",
    "new_tab_timeout_ms": 350,
    "reset_format_key": "h",
    "color_keys": ["j", "k", "l", ";"],
    "format_keys": ["a", "s", "d", "f"]
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

## Build & Install

### 1. Prerequisites

Ensure a C++17 compiler (`g++`), `make`, `pkg-config`, and Qt6 Widgets development headers are installed:

- **Arch Linux**:
  ```bash
  sudo pacman -S base-devel qt6-base
  ```
- **Ubuntu / Debian**:
  ```bash
  sudo apt install build-essential pkg-config qt6-base-dev
  ```
- **Fedora**:
  ```bash
  sudo dnf install gcc-c++ make pkgconf-pkg-config qt6-qtbase-devel
  ```

### 2. Build & Run Locally

```bash
git clone git@github.com:psurrena11/mimenotes.git
cd mimenotes
make
./mimenotes
```

### 3. Install as a Desktop GUI Application

To install **Mime Notes** into your Linux desktop application launcher (so it appears in GNOME/KDE app menus, Rofi/Wofi, and docks with its icon):

```bash
make install
```

This installs:
- **Binary**: `~/.local/bin/mimenotes`
- **Desktop Entry**: `~/.local/share/applications/mimenotes.desktop`
- **App Icon**: `~/.local/share/icons/hicolor/scalable/apps/mimenotes.svg`
- **User Config & Themes**: `~/.config/mimenotes/config.json` and `~/.config/mimenotes/themes/`

To install system-wide (e.g. under `/usr/local`) instead of `~/.local`:

```bash
sudo make install PREFIX=/usr/local
```

### 4. Uninstall

```bash
make uninstall
```
