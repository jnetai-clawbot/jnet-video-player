# J~NET Video Player - Python Version

A dark-themed video player for Linux using PyQt6 and python-mpv.

## Features

- Play/Pause, Stop, Seek
- Volume control with slider
- Fullscreen mode
- Playlist management
- File browser
- Keyboard shortcuts
- Dark theme

## Installation

```bash
pip install PyQt6 python-mpv
python3 jnet-video-player.py
```

Or use the setup script:
```bash
chmod +x setup.sh
./setup.sh
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Space | Play/Pause |
| F | Toggle Fullscreen |
| Escape | Exit Fullscreen |
| Left/Right | Seek ±10s |
| Up/Down | Volume ±10% |
| O | Open File |
| Ctrl+O | Open File |
