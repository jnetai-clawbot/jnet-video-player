# J~NET Video Player - C/Wayland Version

A native Wayland video player for Raspberry Pi 5 (arm64) using FFmpeg and EGL.

## Dependencies

```bash
# Debian/Ubuntu (RPi OS)
sudo apt-get install -y \
  wayland-protocols \
  libwayland-dev \
  libwayland-egl-backend-dev \
  libegl1-mesa-dev \
  libgles2-mesa-dev \
  libxkbcommon-dev \
  libpulse-dev \
  libavcodec-dev \
  libavformat-dev \
  libavutil-dev \
  libswscale-dev \
  libswresample-dev \
  libavfilter-dev \
  libavdevice-dev \
  cmake \
  pkg-config
```

## Build

```bash
cd C
make
```

## Features

- Play/Pause, Stop, Seek
- Volume control
- Fullscreen mode
- Playlist management
- Dark themed UI
- arm64/RPi5 optimized

## Usage

```bash
./jnet-video              # No file - press 'o' to open
./jnet-video video.mp4   # Play a file
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Space | Play/Pause |
| F | Toggle Fullscreen |
| Escape | Exit Fullscreen / Close |
| Left/Right | Seek ±10s |
| Up/Down | Volume ±10% |
| Tab | Toggle Playlist |
| O | Open File |
