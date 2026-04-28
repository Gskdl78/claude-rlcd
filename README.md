# claude-rlcd

A standalone Wi-Fi-connected reflective-LCD dashboard for [Claude Code](https://www.anthropic.com/claude-code).
A small Node.js bridge on your PC watches Claude Code's hooks and project
logs and pushes deltas to an ESP32-S3-RLCD-4.2 board over WebSocket. The
device renders a 1-bit, 400×300 LVGL UI with active sessions, quota usage,
clock/env, and a notification overlay when a session is waiting on
approval.

```
┌────────────────────────────────────────────────────┐
│ 20:17 Tue                              31°C · 39%  │
│ ────────────────────────────────────────────────── │
│ SESSIONS · 3 ACTIVE                                │
│  ●  claude-rlcd            Bash sleep 6 && cat...  │
│  ●  zotero-reforge-main    idle                    │
│  ○  notes                  Read CLAUDE.md          │
│                                                    │
│ QUOTA                                              │
│ 5h    20%   7d    11%   Opus    --                 │
│ ████░░░░    ██░░░░░░    ░░░░░░░░                   │
└────────────────────────────────────────────────────┘
```

## What you need

- **Hardware**: [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2) — 4.2" reflective LCD with ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB octal PSRAM), USB-C, ES8311 audio codec, PCF85063 RTC, QMI8658C IMU.
- **PC**: Windows 10/11, macOS 12+, or Linux. Node.js 18 or 20.
- **Wi-Fi**: a 2.4 GHz network the PC and device can both reach.
- **Claude Code**: installed and configured.

## Repo layout

```
firmware/                ESP-IDF v5.5 + LVGL v8 source for the device
  components/
    display_bsp/         Waveshare panel driver (vendored)
    net_manager/         Wi-Fi STA/AP, captive portal, mDNS
    ws_server/           Single-client NDJSON WebSocket
    state_store/         Pure-C state model + JSON parsers
    ui_renderer/         LVGL screens + CJK font
    sensors_clock/       I2C temp/humidity (if board supports), PCF85063 RTC, SNTP
    audio_alert/         ES8311 codec + 1 kHz beep
  build_flash.sh         Build / flash on macOS/Linux
  build_flash.bat        Build / flash on Windows

bridge/                  Node.js daemon
  src/
    index.js                Entrypoint w/ reconnect loop
    orchestrator.js         Periodic deltas, alerts, quota, time-sync
    protocol/               Wire-protocol schemas + encoders
    sources/                events-tail, projects-reader, quota-reader
    transport/              mDNS resolver + WebSocket client
  installer/
    install.bat / install.sh        merges hooks + sets up auto-start
    start-bridge.cmd / start-bridge.sh   the daemon launcher itself
    com.claude-rlcd.bridge.plist    macOS LaunchAgent template

.github/workflows/       CI: bridge tests + firmware build on every push
```

## Quick start

### 1. Flash the firmware

Plug the device in via USB-C. Install [ESP-IDF v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/get-started/) once, then:

**macOS / Linux**
```sh
. ~/esp/esp-idf/export.sh
cd firmware
./build_flash.sh /dev/cu.usbmodem*    # or /dev/ttyUSB0 on Linux
```

**Windows**
```bat
cd firmware
build_flash.bat COM5
```

> ⚠️ On Windows, source paths with non-ASCII characters break ESP-IDF's
> ldgen step. `build_flash.bat` works around it by redirecting the build
> directory to `%LOCALAPPDATA%\esp_build\claude-rlcd`.

### 2. Configure Wi-Fi (first boot)

The device boots into Soft AP mode. Connect a phone or laptop to the
SSID `Claude-RLCD-Setup` (open), browse to `http://192.168.4.1`, and
enter your home Wi-Fi credentials. The device reboots, joins your
network, and registers `claude-rlcd.local` over mDNS.

The LCD will show `LISTENING — claude-rlcd.local — <IP>`. **Note that
IP** in case Windows can't resolve `.local` names.

### 3. Install the PC bridge

**macOS / Linux**
```sh
cd bridge
npm install
./installer/install.sh
```

**Windows**
```bat
cd bridge
npm install
installer\install.bat
```

`install.{sh,bat}` does two things:

1. Adds `Notification`, `SessionStart`, `Stop` hooks to
   `~/.claude/settings.json` (BOM-safe; it preserves any existing hooks).
2. Sets up the daemon to auto-start at login:
   - **Windows**: a Task Scheduler task `claude-rlcd-bridge` (you need
     to register this manually — see `bridge/README.md`).
   - **macOS**: a LaunchAgent at
     `~/Library/LaunchAgents/com.claude-rlcd.bridge.plist`.
   - **Linux**: a systemd `--user` unit (if systemd is present).

If `claude-rlcd.local` doesn't resolve on your machine (Windows without
Bonjour Print Services, some Wi-Fi networks block multicast), edit the
launcher script (`start-bridge.cmd` or `start-bridge.sh`) and set
`CLAUDE_RLCD_HOST` to the IP you noted in step 2.

### 4. Test

In any project, start a Claude Code session and trigger an action that
asks for approval (e.g. a new shell command). Within ~1 second the
device should beep and show the alert overlay. When the prompt is
resolved, the overlay clears.

## How it works

- **Hooks emit point events.** When Claude Code fires `Notification` /
  `SessionStart` / `Stop`, it runs `bridge/installer/emit.{cmd,sh}`,
  which invokes `bridge/src/hooks-emitter.js` to write a JSON line into
  `~/.claude/claude-rlcd/events.jsonl`.
- **The daemon tails that file** plus `~/.claude/projects/*/*.jsonl`
  (for session metadata and tool history) and
  `~/.claude/status-tracker-cache.json` (for quota, optional).
- **It pushes NDJSON over WebSocket** to the device. Wire protocol
  has 13 message types — see `bridge/src/protocol/schema.js`.
- **The device** uses a single-client WebSocket server, an in-RAM state
  store, and LVGL on the 1-bit panel. State persists across the
  bridge's reconnects (the orchestrator re-sends a full `state` message
  on every fresh WS open).

## Known limitations

- **No multi-PC support.** One device tracks one PC's Claude Code state.
- **Windows mDNS is fragile** without Bonjour Print Services.
- **The board's QMI8658C IMU is unused** — env reads will fail unless
  you wire your own SHTC3 (or similar) on GPIO13/14.
- **CJK font is large** (~5 MB). Bumps the factory partition to 8 MB,
  but still well within the 16 MB N16R8 flash.

## License

MIT — see [LICENSE](LICENSE).

The vendored Waveshare BSP under `firmware/components/display_bsp/`
retains its original license; see
`firmware/components/display_bsp/LICENSE-WAVESHARE.txt`.

The CJK font in `firmware/components/ui_renderer/font_cjk.c` is
generated from
[Noto Sans TC](https://fonts.google.com/noto/specimen/Noto+Sans+TC)
(SIL Open Font License 1.1).
