@echo off
setlocal
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
set "OUTPUT=MiniCraft3D.exe"
set "TEMP_OUTPUT=MiniCraft3D.new.exe"

if exist "%TEMP_OUTPUT%" del /q "%TEMP_OUTPUT%"

g++ main.cpp src/gameplay/gameplay.cpp -o "%TEMP_OUTPUT%" -std=c++17 -O2 -Wall -Wextra -IC:\msys64\ucrt64\include -LC:\msys64\ucrt64\lib -lglfw3 -lglew32 -lopengl32 -lgdi32 -luser32 -lshell32
if errorlevel 1 (
    if exist "%TEMP_OUTPUT%" del /q "%TEMP_OUTPUT%"
    echo.
    echo Build failed. The existing %OUTPUT% was left unchanged.
    exit /b 1
)

move /Y "%TEMP_OUTPUT%" "%OUTPUT%" >nul
if errorlevel 1 (
    if exist "%TEMP_OUTPUT%" del /q "%TEMP_OUTPUT%"
    echo.
    echo Could not replace %OUTPUT%. Make sure the game is closed and try again.
    exit /b 1
)

echo.
echo Build successful. Existing output was replaced if it was present.
echo Run %OUTPUT%
