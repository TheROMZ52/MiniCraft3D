@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update.ps1"
if errorlevel 1 (
    echo.
    echo Update failed. Your current game was not replaced.
    pause
    exit /b 1
)
echo.
pause
