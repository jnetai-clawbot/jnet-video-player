#!/bin/bash
# J~NET Video Player - Start Script
cd "$(dirname "$0")"

echo "[JNET] Starting J~NET Video Player..."

# Activate virtual environment if it exists
if [ -d "venv" ]; then
    source venv/bin/activate
fi

# Run with -B to skip .pyc cache
python3 -B jnet-video-player.py "$@"

echo ""
echo "[JNET] Player closed."
