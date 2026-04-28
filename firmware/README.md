# claude-rlcd firmware

ESP-IDF v5.5 firmware for the Waveshare ESP32-S3-RLCD-4.2. Renders a 1-bit
400×300 LVGL UI showing Claude Code sessions, quota, and an alert overlay
on approval prompts. Communicates with the PC bridge over WebSocket and
discovers itself as `claude-rlcd.local` via mDNS.

## Toolchain

- ESP-IDF **v5.5** (tested with v5.5.3).
  - Windows: install at `C:\Espressif` via Espressif's installer.
  - macOS / Linux: clone `esp-idf` and run `./install.sh` per the
    [official guide](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/get-started/).
- Target chip: `esp32s3` (16 MB flash, 8 MB octal PSRAM).

## Build & flash

The included helper scripts handle target setup, the ASCII-build-dir
workaround for non-ASCII source paths on Windows, and flash + monitor in
one step.

**Windows**
```bat
cd firmware
build_flash.bat              :: build only
build_flash.bat COM5         :: build + flash + monitor on COM5
```

**macOS / Linux**
```sh
. ~/esp/esp-idf/export.sh    # source ESP-IDF env first
cd firmware
./build_flash.sh                            # build only
./build_flash.sh /dev/cu.usbmodem...        # build + flash + monitor
```

Or run `idf.py` directly if you prefer:
```sh
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

Press `Ctrl+]` to exit the monitor.

## First boot

1. Device powers on, shows `BOOT` then `SOFTAP`.
2. Connect a phone or laptop to Wi-Fi `Claude-RLCD-Setup` (open).
3. Open `http://192.168.4.1` and submit your home Wi-Fi SSID + password.
4. Device reboots, connects, and shows `LISTENING — claude-rlcd.local`.
5. On the PC, run the bridge (`bridge\installer\install.bat`).

If Wi-Fi connection fails 30 times in a row (with the documented backoff),
the device wipes the stored SSID and falls back to `SOFTAP` for re-config.

## Components

| Component       | Purpose                                                |
|-----------------|--------------------------------------------------------|
| `display_bsp`   | Vendored Waveshare driver + LVGL port                  |
| `net_manager`   | Wi-Fi STA/AP, captive portal, mDNS                     |
| `ws_server`     | Single-client NDJSON WebSocket on port 80              |
| `state_store`   | Pure C state model + cJSON parsers (host_test target)  |
| `ui_renderer`   | LVGL screens (boot/softap/connecting/listening/main)   |
| `sensors_clock` | SHTC3 + PCF85063 + SNTP                                |
| `audio_alert`   | ES8311 single-beep playback on new alert               |

## Host tests

State machine + protocol tests run on the host (no hardware):

```bash
cd firmware/test
idf.py --preview set-target linux
idf.py build
./build/claude_rlcd_test.elf
```

## Partition layout

`partitions.csv` reserves a small NVS partition for stored Wi-Fi creds and
a large `factory` app partition. No OTA in v1.

## Troubleshooting

- **`idf.py: command not found`**: open the ESP-IDF 5.5 CMD shortcut, not a
  generic terminal.
- **Display is blank but logs look healthy**: confirm the SPI pins in
  `main/display_glue.cpp` match your board revision. Defaults are MOSI=5
  SCLK=6 DC=7 CS=15 RST=14 (Waveshare reference).
- **Audio doesn't beep**: ES8311 needs the I2C bus at GPIO8/9. The
  `sensors_clock` component shares that bus — order of `audio_init()` /
  `sclk_init()` doesn't matter, but driver double-install will warn.
- **`mDNS hostname collision`**: the device renames itself to
  `claude-rlcd-2.local` (and so on, up to -5). The new name shows on the
  LISTENING screen.
