#!/bin/bash

set -e

echo "[1/3] Updating package list..."
sudo apt update

echo "[2/3] Installing OpenCV development libraries..."
sudo apt install -y libopencv-dev
sudo apt install -y \
    libopencv-core-dev \
    libopencv-imgproc-dev \
    libopencv-highgui-dev \
    libopencv-video-dev \
    libopencv-features2d-dev \
    libopencv-calib3d-dev \
    libopencv-objdetect-dev \
    libopencv-photo-dev \
    libopencv-videoio-dev

echo "[3/3] file explorer..."
sudo apt install -y xdg-utils

echo "OpenCV installation complete."