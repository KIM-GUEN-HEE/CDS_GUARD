#!/bin/bash

set -e

echo "install qt..."

sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential \
  libxcb-xinerama0-dev \
  build-essential \
  openssl \
  libssl-dev \
  libgl1-mesa-dev \
  libqt5x11extras5 \
  libx11-xcb-dev \
  libglu1-mesa-dev \
  libxrender-dev \
  libxi-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libxcb-cursor0

# Qt 온라인 설치 파일 다운로드
QT_INSTALLER_URL="https://download.qt.io/official_releases/online_installers/qt-online-installer-linux-x64-online.run"
INSTALLER_NAME="qt-installer.run"

wget $QT_INSTALLER_URL -O $INSTALLER_NAME

chmod +x $INSTALLER_NAME

sudo ./$INSTALLER_NAME --mirror http://ftp.jaist.ac.jp/pub/qtproject/