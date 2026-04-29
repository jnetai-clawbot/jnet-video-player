#!/bin/bash
# J~NET Video Player - Python Setup Script
# Installs dependencies in a virtual environment

set -e

echo "[JNET] Installing J~NET Video Player dependencies..."

# Detect if we're in a venv already
if [ -n "$VIRTUAL_ENV" ]; then
    echo "[JNET] Already in virtual environment: $VIRTUAL_ENV"
else
    # Create a venv if not exists
    if [ ! -d "venv" ]; then
        echo "[JNET] Creating virtual environment..."
        python3 -m venv venv
    fi
    echo "[JNET] Activating virtual environment..."
    source venv/bin/activate
fi

# Install Python packages
echo "[JNET] Installing Python packages..."
pip install PyQt6 python-mpv 2>&1 | tail -3

echo ""
echo "[JNET] Setup complete!"
echo "[JNET] Run: python3 jnet-video-player.py [file]"
echo "[JNET] Or use: ./start.sh"
