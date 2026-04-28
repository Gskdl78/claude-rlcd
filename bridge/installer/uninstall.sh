#!/usr/bin/env bash
# Remove the claude-rlcd bridge install (LaunchAgent / systemd unit) and
# strip our hooks from ~/.claude/settings.json.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "$OSTYPE" in
    darwin*)
        PLIST="$HOME/Library/LaunchAgents/com.claude-rlcd.bridge.plist"
        if [[ -f "$PLIST" ]]; then
            launchctl unload "$PLIST" 2>/dev/null || true
            rm "$PLIST"
            echo "==> removed LaunchAgent"
        fi
        ;;
    linux*)
        if command -v systemctl >/dev/null && systemctl --user >/dev/null 2>&1; then
            systemctl --user disable --now claude-rlcd-bridge.service 2>/dev/null || true
            rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/claude-rlcd-bridge.service"
            systemctl --user daemon-reload
            echo "==> removed systemd unit"
        fi
        ;;
esac

node "$HERE/uninstall.mjs"
echo "==> hooks removed from ~/.claude/settings.json"
