@echo off
REM Build (and optionally flash + monitor) the claude-rlcd firmware on
REM Windows. Self-locating; run from anywhere.
REM
REM   build_flash.bat              -- build only
REM   build_flash.bat COM5         -- build + flash + monitor on COM5
REM
REM Requires ESP-IDF v5.5. Edit IDF_EXPORT below if your install path
REM differs from the default.

setlocal EnableDelayedExpansion
set "HERE=%~dp0"
set "FW=%HERE:~0,-1%"

REM ---- locate ESP-IDF (try common 5.5.x paths) ----
set "IDF_EXPORT="
for %%V in (v5.5.3 v5.5.2 v5.5.1 v5.5 v5.5.4) do (
    if not defined IDF_EXPORT if exist "C:\Espressif\frameworks\esp-idf-%%V\export.bat" (
        set "IDF_EXPORT=C:\Espressif\frameworks\esp-idf-%%V\export.bat"
    )
)
if not defined IDF_EXPORT if exist "C:\Espressif\frameworks\esp-idf\export.bat" (
    set "IDF_EXPORT=C:\Espressif\frameworks\esp-idf\export.bat"
)
if not defined IDF_EXPORT (
    echo [ERROR] ESP-IDF export.bat not found under C:\Espressif\frameworks\
    echo         Edit this script to set IDF_EXPORT manually.
    pause
    exit /b 1
)

REM ---- build directory: ASCII path required (ESP-IDF ldgen subprocess
REM      can't pass non-ASCII paths to objdump on Windows). ----
set "BUILD_DIR=%LOCALAPPDATA%\esp_build\claude-rlcd"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

call "%IDF_EXPORT%"
if errorlevel 1 ( echo [ERROR] export.bat failed. & pause & exit /b 1 )

cd /d "%FW%"
echo [INFO] Source : %CD%
echo [INFO] Build  : %BUILD_DIR%

if not exist sdkconfig (
    echo ========== first-time setup: idf.py set-target esp32s3 ==========
    idf.py -B "%BUILD_DIR%" set-target esp32s3
    if errorlevel 1 ( echo [ERROR] set-target failed! & pause & exit /b 1 )
)

echo ========== building ==========
idf.py -B "%BUILD_DIR%" build
if errorlevel 1 ( echo [ERROR] build failed! & pause & exit /b 1 )

if not "%~1"=="" (
    echo ========== flashing on %~1 ==========
    idf.py -B "%BUILD_DIR%" -p %~1 flash monitor
) else (
    echo ========== build SUCCESS ==========
    echo Re-run with a COM port to flash:  build_flash.bat COM5
)
pause
