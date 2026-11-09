@echo off
setlocal
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
g++ main.cpp -o MiniCraft3D.exe -std=c++17 -O2 -Wall -Wextra -IC:\msys64\ucrt64\include -LC:\msys64\ucrt64\lib -lglfw3 -lglew32 -lopengl32 -lgdi32 -luser32 -lshell32
if errorlevel 1 (
    echo.
    echo Build failed. Check the compiler output above.
    exit /b 1
)
echo.
echo Build successful.
echo Run MiniCraft3D.exe
