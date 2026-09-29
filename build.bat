@echo off
setlocal

set SOURCE=qingyise_tingpai_train.cpp
set OUTPUT=qingyise_tingpai_train.exe
set COMPILER=D:\mingw64\bin\g++.exe

if not exist "%COMPILER%" (
    echo Error: compiler not found -> %COMPILER%
    echo Please check the MinGW path and update build.bat if needed.
    exit /b 1
)

if not exist "%SOURCE%" (
    echo Error: source file not found -> %SOURCE%
    exit /b 1
)

echo Compiling %SOURCE% ...
"%COMPILER%" -std=c++17 -O2 "%SOURCE%" -o "%OUTPUT%"

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo Build successful: %OUTPUT%

if "%1"=="run" (
    echo Running %OUTPUT%...
    "%OUTPUT%"
)

exit /b 0
