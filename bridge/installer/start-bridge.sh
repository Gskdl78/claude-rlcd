#!/usr/bin/env bash
# Daemon launcher for macOS/Linux — invoked by launchd or systemd or run
# directly with ./start-bridge.sh. Self-locating; the bridge dir is the
# parent of this file's directory.

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BRIDGE="$(cd "$HERE/.." && pwd)"
LOG="${HOME}/Library/Logs/claude-rlcd-bridge.log"
[[ "$OSTYPE" == linux* ]] && LOG="${XDG_STATE_HOME:-$HOME/.local/state}/claude-rlcd-bridge.log"
mkdir -p "$(dirname "$LOG")"

# ---- user-tunable: edit or set in environment before running ----
: "${CLAUDE_RLCD_HOST:=claude-rlcd.local}"
export CLAUDE_RLCD_HOST
# -----------------------------------------------------------------

cd "$BRIDGE" || { echo "[$(date)] bridge dir missing at $BRIDGE" >> "$LOG"; exit 1; }

while true; do
    echo "[$(date)] starting daemon in $PWD (host=$CLAUDE_RLCD_HOST)" >> "$LOG"
    node src/index.js >> "$LOG" 2>&1
    code=$?
    echo "[$(date)] daemon exited (code $code); retry in 5s" >> "$LOG"
    sleep 5
done
