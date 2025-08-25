@echo off
echo Compiling with MinGW (Fully Static)...

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
echo Compiling ftool.cpp with full static linking...

REM 使用完全静态链接，避免任何DLL依赖
g++ -O2 -std=c++11 -static ftool.cpp -lws2_32 -o ftool_static.exe
if errorlevel 1 (
    echo.
    echo Compilation failed!
    exit /b 1
) else (
    echo.
    echo Compilation successful!
    echo Generated: ftool_static.exe (fully static)
    echo.
    echo Usage examples:
    echo   ftool_static.exe server 8080 C:\received_files
    echo   ftool_static.exe client 127.0.0.1 8080 test.txt
)
pause
