#!/usr/bin/env bash
# Install the claude-rlcd bridge on macOS or Linux.
#
#   1) Merges Claude Code hooks into ~/.claude/settings.json (BOM-safe).
#   2) On macOS: installs a LaunchAgent that runs the daemon at login.
#      On Linux: installs a systemd --user unit (if systemd is present),
#                otherwise prints manual run instructions.
#
# Re-run any time. Idempotent.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BRIDGE="$(cd "$HERE/.." && pwd)"

echo "==> bridge dir : $BRIDGE"

# 1) hooks ----------------------------------------------------------------
EMITTER="$HERE/emit.sh"
if [[ ! -x "$EMITTER" ]]; then
    cat > "$EMITTER" <<'EOF'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
node "$HERE/../src/hooks-emitter.js" "$1"
EOF
    chmod +x "$EMITTER"
    echo "==> wrote emit.sh"
fi

echo "==> merging Claude Code hooks"
node "$HERE/install.mjs" "$EMITTER"

# 2) daemon launcher ------------------------------------------------------
case "$OSTYPE" in
    darwin*)
        PLIST_SRC="$HERE/com.claude-rlcd.bridge.plist"
        PLIST_DST="$HOME/Library/LaunchAgents/com.claude-rlcd.bridge.plist"
        mkdir -p "$(dirname "$PLIST_DST")"

        # Substitute placeholders with the actual paths.
        sed -e "s|__BRIDGE_PATH__|$BRIDGE|g" \
            -e "s|__HOME__|$HOME|g" \
            "$PLIST_SRC" > "$PLIST_DST"

        echo "==> installed LaunchAgent at $PLIST_DST"

        # Reload (errors ignored if not yet loaded).
        launchctl unload "$PLIST_DST" 2>/dev/null || true
        launchctl load -w "$PLIST_DST"
        echo "==> LaunchAgent loaded; daemon running"
        echo "    log: ~/Library/Logs/claude-rlcd-bridge.log"
        ;;
    linux*)
        if command -v systemctl >/dev/null && systemctl --user >/dev/null 2>&1; then
            UNIT_DST="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/claude-rlcd-bridge.service"
            mkdir -p "$(dirname "$UNIT_DST")"
            cat > "$UNIT_DST" <<EOF
[Unit]
Description=claude-rlcd bridge daemon
After=network-online.target

[Service]
Type=simple
WorkingDirectory=$BRIDGE
ExecStart=/usr/bin/env node $BRIDGE/src/index.js
Environment=CLAUDE_RLCD_HOST=claude-rlcd.local
Restart=always
RestartSec=5
StandardOutput=append:%h/.local/state/claude-rlcd-bridge.log
StandardError=append:%h/.local/state/claude-rlcd-bridge.log

[Install]
WantedBy=default.target
EOF
            mkdir -p "$HOME/.local/state"
            systemctl --user daemon-reload
            systemctl --user enable --now claude-rlcd-bridge.service
            echo "==> systemd user unit installed and started"
            echo "    log: ~/.local/state/claude-rlcd-bridge.log"
            echo "    status: systemctl --user status claude-rlcd-bridge"
        else
            echo "==> systemd not available; run manually:"
            echo "    bash $HERE/start-bridge.sh"
        fi
        ;;
    *)
        echo "==> unknown OS ($OSTYPE); run manually: bash $HERE/start-bridge.sh"
        ;;
esac

echo "==> done"
