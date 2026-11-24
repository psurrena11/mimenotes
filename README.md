# Mime Notes

A minimalist Linux desktop plain-text notepad built with C++17 and Qt6, matching the Figma design specification (`484×467`, `#212421` background, `20px` outer frame padding).

## Features

- **Clean layout & colors** (`themes/default.json`):
  - `484 × 467` window size with `#212421` background and `20px` outer frame padding.
  - Top bar (`y = 20`) with `14px` Semi-Bold active tab, `50%` opacity Regular inactive tabs (`20px` gap), and `14 × 14` SVG `+` icon.
  - **Bottom-left formatting bar**: 4 SVG icons (`Bold` `9×12`, `Italic` `12×12`, `Up Arrow` `9×12`, `Down Arrow` `8×12`, `20px` gap) for toggling bold/italic and increasing/decreasing font size by `4px` (configurable via `"font_size_step"`). Active tools highlight in `"active_tool_color"` (`#AAFFE4` by default).
  - **Bottom-right color & highlighter swatches**: Four `12 × 12` swatches spaced `16px` apart (circles in text color mode, squares in highlighter mode, with a centered `#212421` dot marking the active swatch):
    1. `#FFFFFF` (White)
    2. `#FF8FBE` (Pink)
    3. `#F4D76E` (Warm Yellow)
    4. `#AAFFE4` (Mint / Bright Aqua)
- **Home-row leader commands (`jj`), tab commands (`ff`), delete line (`dd`), & quick search (`//`)**:
  - Type `jj` quickly followed by:
    - `a` → Toggle **Bold**
    - `s` → Toggle *Italic*
    - `d` → Increase font size (`+4px`)
    - `f` → Decrease font size (`-4px`)
    - `j` → Color 1 (`#FFFFFF` White)
    - `k` → Color 2 (`#FF8FBE` Pink)
    - `l` → Color 3 (`#F4D76E` Warm Yellow)
    - `;` → Color 4 (`#AAFFE4` Mint)
    - `h` → **Reset** formatting (`16px`, `#FFFFFF` White, no bold/italic/highlight)
    - `u` → Toggle **Highlighter Mode** (swatches turn into squares; text color automatically switches between `#000000` and `#FFFFFF` based on WCAG contrast ratio)
  - Type `ff` quickly followed by:
    - `t` → **New Tab** (`new_tab`)
    - `j` → **Previous Tab** (`prev_tab`)
    - `k` → **Next Tab** (`next_tab`)
    - `r` → **Rename Tab** (`rename_tab`)
  - Type `dd` quickly → **Delete Current Line** (`delete_line_trigger`)
  - Type `//` quickly → Open **Quick Search Overlay** to filter and open any `.txt` file in your Mimenotes directory (`search_trigger`)
- **16px default editor font**: Clean `16px` body text (`Ctrl+0` resets to the theme's `editor.font_size`).
- **Auto-divider (`---`)**: Typing `---` automatically breaks to a new line, renders a full-width horizontal divider across the note, and moves the cursor to a new line below it (while saving cleanly as `---` in the underlying `.txt` file). Pressing `Backspace` right after a divider removes the divider in a single keystroke.
- **Hot-reloading `config.json` & `themes/`**: Editing and saving `config.json` or the active theme JSON file in `themes/` live-updates the running window immediately.
- **Timestamped files, non-destructive tab close, & auto-save**: Each note is saved as `YYYY-MM-DD_HHMMSS_<Title>.txt` inside `~/Documents/Mimenotes/` (or `~/Mimenotes/` if `~/Documents` is not present, or a custom path in `config.json` / `--dir <path>`). The timestamp prefix is hidden in the tab bar so tabs stay clean while your folder stays chronologically sorted. Closing a tab never deletes files from disk—reopen any note anytime via `//` or `Ctrl+Shift+T`. Saves automatically 300ms after typing stops and immediately on tab switch, rename, close, focus loss, or exit.

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
  "active_tool_color": "#AAFFE4",
  "notes_dir": "~/Documents/Mimenotes",
  "keybindings": {
    "new_tab": "ff+t",
    "new_tab_alt": "Ctrl+T",
    "close_tab": "Ctrl+W",
    "reopen_closed_tab": "Ctrl+Shift+T",
    "save": "Ctrl+S",
    "rename_tab": "ff+r",
    "rename_tab_alt": "F2",
    "next_tab": "ff+k",
    "next_tab_alt": "Ctrl+Tab",
    "prev_tab": "ff+j",
    "prev_tab_alt": "Ctrl+Shift+Tab",
    "zoom_in": "Ctrl+=",
    "zoom_out": "Ctrl+-",
    "zoom_reset": "Ctrl+0",
    "divider_trigger": "---",
    "delete_line_trigger": "dd",
    "delete_line_timeout_ms": 350,
    "search_trigger": "//",
    "search_timeout_ms": 350,
    "leader_trigger": "jj",
    "leader_timeout_ms": 350,
    "tab_leader_trigger": "ff",
    "tab_leader_timeout_ms": 350,
    "reset_format_key": "h",
    "highlight_mode_key": "u",
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
  sudo pacman -S base-devel qt6-base qt6-svg
  ```
- **Ubuntu / Debian**:
  ```bash
  sudo apt install build-essential pkg-config qt6-base-dev qt6-svg-dev
  ```
- **Fedora**:
  ```bash
  sudo dnf install gcc-c++ make pkgconf-pkg-config qt6-qtbase-devel qt6-qtsvg-devel
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
