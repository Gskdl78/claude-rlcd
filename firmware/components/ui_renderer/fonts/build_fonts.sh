#!/usr/bin/env sh
# Requires: npm i -g lv_font_conv
# Source TTF: NotoSansCJKtc-Regular.otf (download separately, do NOT commit)
set -e
HERE="$(dirname "$0")"
SRC="${SRC:-$HERE/NotoSansCJKtc-Regular.otf}"
RANGE="0x20-0x7E"

if [ ! -f "$SRC" ]; then
  echo "missing $SRC — see fonts/README.md" >&2
  exit 1
fi
if [ ! -f "$HERE/cjk_chars.txt" ]; then
  echo "missing $HERE/cjk_chars.txt — see fonts/README.md" >&2
  exit 1
fi

lv_font_conv --no-compress --bpp 1 --size 14 \
  --font "$SRC" -r "$RANGE" --symbols "$(cat "$HERE/cjk_chars.txt")" \
  --format lvgl --lv-include lvgl.h -o "$HERE/cjk_subset_14.c"

lv_font_conv --no-compress --bpp 1 --size 18 \
  --font "$SRC" -r "$RANGE" --symbols "$(cat "$HERE/cjk_chars.txt")" \
  --format lvgl --lv-include lvgl.h -o "$HERE/cjk_subset_18.c"
