#!/bin/bash
# J~NET Video Player - Python Setup Script
# Install dependencies and desktop entry
function venv() {
    #python3 -m venv venv
    python3.12 -m venv venv
    PYTHON_PATH=$(which python3.12)
    echo "alias python='$PYTHON_PATH'" >> venv/bin/activate
    source venv/bin/activate
    echo "Virtual Environment setup and ready!"
    echo ""
}


set -e

venv

echo "Starting J~NET Video Player..."

python3 jnet-video-player.py $@
