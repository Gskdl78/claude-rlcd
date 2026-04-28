# CJK font subset

LVGL `lv_font_conv` (Node tool: `npm i -g lv_font_conv`) is used to subset
NotoSansCJKtc-Regular for the ~2 000 most-common Traditional Chinese characters
plus printable ASCII, at sizes 14 and 18.

Generated files (`cjk_subset_14.c`, `cjk_subset_18.c`, ~250 KB each) are NOT
committed yet — they're large and reproducibly generated. To produce them:

1. Download `NotoSansCJKtc-Regular.otf` into this directory (don't commit it).
2. Place a UTF-8 file `cjk_chars.txt` here with the characters to include.
   A starter list (TOCFL/MOE 2000) can be generated from public sources.
3. Install the conversion tool: `npm i -g lv_font_conv`
4. Run `bash build_fonts.sh`.

Until the .c files exist the firmware uses LVGL's built-in fonts; Chinese
characters will display as `□` boxes. The 'starter' UI strings have been kept
in English/digits where possible to make the firmware bootable before fonts
are generated.
