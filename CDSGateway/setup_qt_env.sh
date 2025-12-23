#!/bin/bash

QT_ROOT="/opt/Qt"
latest_version=$(ls -1 $QT_ROOT | grep -E "^[0-9]+\.[0-9]+\.[0-9]+$" | sort -Vr | head -n1)

if [[ -z "$latest_version" ]]; then
    echo "Qt 버전을 /opt/Qt 아래에서 찾을 수 없습니다."
    exit 1
fi

if [[ -z "$latest_version" ]]; then
    echo "Qt 버전을 /opt/Qt 아래에서 찾을 수 없습니다."
    exit 1
fi

QT_DIR="$QT_ROOT/$latest_version/gcc_64"

if [[ ! -d "$QT_DIR" ]]; then
    echo "Qt gcc_64 디렉토리가 존재하지 않습니다: $QT_DIR"
    exit 1
fi

BLOCK_START="# === Qt environment begin ==="
BLOCK_END="# === Qt environment end ==="

ENV_BLOCK=$(cat <<EOF
$BLOCK_START
export PATH="$QT_DIR/bin:\$PATH"
export LD_LIBRARY_PATH="$QT_DIR/lib:\$LD_LIBRARY_PATH"
export CMAKE_PREFIX_PATH="$QT_DIR/lib/cmake:\$CMAKE_PREFIX_PATH"
export Qt6_DIR="$QT_DIR/lib/cmake/Qt6"
$BLOCK_END
EOF
)

if grep -Fq "# === Qt environment begin ===" $HOME/.bashrc; then
    echo "Qt 환경변수 블록이 이미 ~/.bashrc에 존재합니다. 수정하지 않습니다."
else
    echo -e "\n$ENV_BLOCK" >> $HOME/.bashrc
    echo "환경변수 설정 완료. vscode 재실행 및 `source ~/.bashrc`를 해주세요."
fi