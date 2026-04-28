@echo off
REM Daemon launcher invoked by Task Scheduler -> wscript -> start-bridge.vbs.
REM Self-locating: resolves the bridge dir from this file's own path.
REM Override the device IP by editing CLAUDE_RLCD_HOST below; leave it
REM empty to fall back to mDNS (claude-rlcd.local).

setlocal EnableExtensions
set "HERE=%~dp0"
set "BRIDGE=%HERE%.."
set "LOG=%LOCALAPPDATA%\claude-rlcd-bridge.log"

REM ---- user-tunable ----
set "CLAUDE_RLCD_HOST=192.168.0.51"
REM ----------------------

cd /d "%BRIDGE%" || (
    echo [%DATE% %TIME%] BRIDGE not found at "%BRIDGE%" >> "%LOG%"
    exit /b 1
)

:loop
echo [%DATE% %TIME%] starting daemon in %CD% >> "%LOG%"
node src\index.js >> "%LOG%" 2>&1
echo [%DATE% %TIME%] daemon exited (code %ERRORLEVEL%); retry in 5s >> "%LOG%"
timeout /t 5 /nobreak >nul
goto loop
