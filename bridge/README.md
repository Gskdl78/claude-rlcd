# claude-rlcd bridge

Node.js daemon that watches your local Claude Code state and pushes deltas
to a Waveshare ESP32-S3-RLCD-4.2 over WebSocket. Discovers the device on
the LAN via mDNS (`claude-rlcd.local`); falls back to a direct IP set via
`CLAUDE_RLCD_HOST`.

## Requirements

- Node.js 18 or 20
- Claude Code with hook support (Notification, SessionStart, Stop)
- A flashed ESP32-S3-RLCD-4.2 on the same Wi-Fi network as the PC

## Install

### macOS / Linux

```sh
cd bridge
npm install
./installer/install.sh
```

`install.sh` will:

1. Merge `Notification`, `SessionStart`, `Stop` hooks into
   `~/.claude/settings.json` (BOM-safe; existing hooks preserved).
2. **macOS** — install a LaunchAgent at
   `~/Library/LaunchAgents/com.claude-rlcd.bridge.plist` that runs the
   daemon at login.
3. **Linux (systemd)** — install a `--user` service unit and start it.

Logs:
- macOS: `~/Library/Logs/claude-rlcd-bridge.log`
- Linux: `~/.local/state/claude-rlcd-bridge.log`

To remove: `./installer/uninstall.sh`.

### Windows

```bat
cd bridge
npm install
installer\install.bat
```

`install.bat` merges the hooks. To make the daemon auto-start at login,
register it as a Scheduled Task:

```powershell
schtasks /Create /TN claude-rlcd-bridge `
  /TR "wscript ""<absolute path to>\bridge\installer\start-bridge.vbs""" `
  /SC ONLOGON /F
schtasks /Run /TN claude-rlcd-bridge
```

Log: `%LOCALAPPDATA%\claude-rlcd-bridge.log` (open with Notepad).

To remove:
```powershell
schtasks /Delete /TN claude-rlcd-bridge /F
installer\uninstall.bat
```

## Run manually (any OS)

```sh
cd bridge
npm start
# or with a forced device IP:
CLAUDE_RLCD_HOST=192.168.0.51 node src/index.js
```

## Configuration

Edit the launcher script (`start-bridge.cmd` on Windows,
`start-bridge.sh` elsewhere) to change:

- `CLAUDE_RLCD_HOST` — device IP or mDNS name (default: `claude-rlcd.local`).
- `CLAUDE_RLCD_FRESHNESS_S` (env var, optional) — sessions whose last
  JSONL row is older than this drop off the dashboard. Default: 1800 s.

## Tests

```sh
npm test           # 34 tests across 10 files
npm run test:watch # vitest watch mode
```

## Troubleshooting

- **`claude-rlcd.local` not resolved**: confirm the device's LISTENING
  screen shows the hostname. Try `ping claude-rlcd.local`. If that fails:
  - **Windows**: install
    [Bonjour Print Services](https://support.apple.com/en-us/106380),
    OR set `CLAUDE_RLCD_HOST` to the IP shown on the device.
  - **Some Wi-Fi networks block multicast**; same fix.
- **WebSocket repeatedly disconnects**: device accepts one client at a
  time. Make sure no other bridge instance is running.
- **Hooks don't fire**: rerun the installer. The hooks live in the
  user-scope `~/.claude/settings.json`; a per-project
  `.claude/settings.json` would override them, so replicate the entries
  there if you use one.
- **Quota always shows `--`**: `~/.claude/status-tracker-cache.json` is
  optional. The bridge logs `quota source missing` once and continues.
- **Device stuck on LISTENING after re-flash**: the daemon should
  reconnect on its own (built-in reconnect loop). If not, check the log;
  most often the device IP changed (DHCP) and `CLAUDE_RLCD_HOST` no
  longer matches.

## Architecture

```
events-tail   ──┐
projects-reader ├──> orchestrator ──> ws-client ──> device
quota-reader  ──┘                                       │
hooks-emitter (per CC hook fire) ───────────────────────┘
```

The wire protocol has 13 NDJSON message types. See
`src/protocol/schema.js` for the full zod schemas.
