#!/bin/bash

set -e

echo "[1/3] Updating package list..."
sudo apt update

echo "[2/3] Installing dependency libraries..."
sudo apt install -y \
    libpcap-dev libssl-dev

echo "dependency installation complete."