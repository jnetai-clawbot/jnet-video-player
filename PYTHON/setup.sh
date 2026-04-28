#!/bin/bash
# J~NET Video Player - Python Setup Script
# Install dependencies and desktop entry

set -e

echo "Installing J~NET Video Player..."

# Install Python dependencies
echo "Installing Python packages..."
pip install --user PyQt6 python-mpv 2>/dev/null || pip install PyQt6 python-mpv

# Create desktop entry
DESKTOP_FILE="$HOME/.local/share/applications/jnet-video-player.desktop"
mkdir -p "$(dirname "$DESKTOP_FILE")"
cat > "$DESKTOP_FILE" << 'EOF'
[Desktop Entry]
Name=J~NET Video Player
Comment=Dark themed video player for Linux
Exec=python3 /path/to/jnet-video-player.py %F
Icon=video-x-generic
Terminal=false
Type=Application
MimeType=video/*;audio/*;
Categories=AudioVideo;Player;Video;
EOF

echo "Done! Run: python3 jnet-video-player.py [file]"
