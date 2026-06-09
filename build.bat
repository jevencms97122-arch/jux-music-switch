@echo off
echo ============================================
echo   Jux Music - Switch Homebrew Builder
echo ============================================
echo.

REM Check if devkitPro is installed
if not exist "C:\devkitPro" (
    echo [!] devkitPro not found. Downloading installer...
    echo.
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/devkitPro/installer/releases/latest/download/devkitProUpdater-3.0.3.exe' -OutFile '%TEMP%\devkitpro-installer.exe'"
    echo [*] Launching devkitPro installer...
    echo     - Check "Switch Development" during install
    echo     - Install to C:\devkitPro (default)
    start /wait "" "%TEMP%\devkitpro-installer.exe"
    echo.
    echo [*] After install, run this script again.
    pause
    exit /b
)

echo [*] devkitPro found at C:\devkitPro

REM Check icon
if not exist "icon.jpg" (
    echo.
    echo [!] icon.jpg not found in this folder!
    echo     Save the app icon as "icon.jpg" (256x256) in:
    echo     %CD%
    echo.
    pause
    exit /b
)

echo [*] icon.jpg found

REM Set environment
set DEVKITPRO=C:\devkitPro
set DEVKITARM=%DEVKITPRO%\devkitARM
set DEVKITA64=%DEVKITPRO%\devkitA64
set PATH=%DEVKITPRO%\tools\bin;%DEVKITA64%\bin;%PATH%

echo [*] Building NRO...
echo.

make

if exist "jux-music.nro" (
    echo.
    echo ============================================
    echo   SUCCESS! jux-music.nro created.
    echo   Copy it to /switch/jux-music/ on your SD
    echo ============================================
) else (
    echo.
    echo [!] Build failed. Check errors above.
)

pause
