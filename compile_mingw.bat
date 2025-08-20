@echo off
echo Compiling with MinGW...

REM 检查 g++ 是否可用
g++ --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: g++ not found!
    echo Please install MinGW-w64 or add it to PATH
    echo Download from: https://www.mingw-w64.org/
    pause
    exit /b 1
)

echo Found g++ compiler
echo Compiling ftool.cpp...

g++ -O2 -std=c++11 -static-libgcc -static-libstdc++ ftool.cpp -lws2_32 -o ftool.exe
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Compilation successful!
    echo.
    echo Usage examples:
    echo   ftool.exe server 8080 C:\received_files
    echo   ftool.exe client 127.0.0.1 8080 test.txt
) else (
    echo.
    echo Compilation failed!
)
pause
