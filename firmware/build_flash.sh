#!/usr/bin/env bash
# Build (and optionally flash + monitor) the claude-rlcd firmware on
# macOS or Linux.
#
#   build_flash.sh                 # build only
#   build_flash.sh /dev/cu.usbmodem...  # build + flash + monitor
#
# Requires ESP-IDF v5.5 — `. $IDF_PATH/export.sh` must succeed in your
# shell. If it doesn't, install ESP-IDF first per
# https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/get-started/
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT="${1:-}"

if [[ -z "${IDF_PATH:-}" ]]; then
    echo "[ERROR] IDF_PATH unset — source ESP-IDF's export.sh first."
    echo "        Typical path: . \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

cd "$HERE"

if [[ ! -f sdkconfig ]]; then
    echo "==> first-time setup: idf.py set-target esp32s3"
    idf.py set-target esp32s3
fi

echo "==> idf.py build"
idf.py build

if [[ -n "$PORT" ]]; then
    echo "==> idf.py -p $PORT flash monitor"
    idf.py -p "$PORT" flash monitor
else
    echo "==> build done. Flash with: $0 /dev/cu.usbmodem..."
    echo "    (find the port: ls /dev/cu.* on macOS, ls /dev/ttyUSB* on Linux)"
fi
