# Terminal Pixel Editor
TPE is a pixel-level image editor for the terminal.

## Building
You need to install the Notcurses library to compile TPE. (`libnotcurses-dev` on Debian)
Run `make` to build the `tpe` executable. `make help` explains more.

## Usage
To open a file with TPE pass its path as a command line argument. You can pass
multiple paths to open more than one file:
```
tpe path/to/file1.png path/to/file2.png
```

### Keybindings
The mouse can be additionally used to invoke tool actions and select items in modals.
| Keybind | Action |
|---------|--------|
| `a`     | Move left |
| `d`     | Move right |
| `w`     | Move up |
| `s`     | Move down |
| `A`     | Move view left |
| `D`     | Move view right |
| `W`     | Move view up |
| `S`     | Move view down |
| `t`     | Change tool (dialog) |
| Digits  | Change tool |
| `Enter` | Invoke primary tool action |
| `Space` | Invoke secondary tool action |
| `Alt-S` | Save the image |
| `Left`  | Switch to the tab on the left |
| `Right` | Switch to the tab on the right |
| `q`     | Close current tab |
