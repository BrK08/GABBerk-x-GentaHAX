@echo off
setlocal

where g++ >nul 2>nul
if errorlevel 1 (
    echo g++ not found. Install MSYS2 and add its UCRT64 bin folder to PATH.
    echo   https://www.msys2.org/
    pause
    exit /b 1
)

echo Building gabb_stabilizer.exe ...
g++ -O2 -static -std=c++17 src\gabb_stabilizer.cpp -o gabb_stabilizer.exe -ladvapi32 -luser32 -static-libgcc -static-libstdc++
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo Done: gabb_stabilizer.exe
pause
