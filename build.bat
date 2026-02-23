@echo off
:: ChronoStore build script for Windows
:: Adds MSYS2 MinGW64 DLLs to PATH before invoking g++

set "MINGW=C:\msys64\mingw64\bin"
set "PATH=%MINGW%;%PATH%"

cd /d "C:\Users\Bhanu\Downloads\projects\CPP\ChronoStore"

echo.
echo [1/2] Building chronostore.exe ...
"%MINGW%\g++.exe" -std=c++17 -O2 -Wall -pthread main.cpp store.cpp -o chronostore.exe
if %ERRORLEVEL% EQU 0 (echo  OK  chronostore.exe) else (echo  FAIL  exit %ERRORLEVEL% & goto :end)

echo.
echo [2/2] Building chronostore_bench.exe ...
"%MINGW%\g++.exe" -std=c++17 -O2 -Wall -pthread benchmark.cpp store.cpp -o chronostore_bench.exe
if %ERRORLEVEL% EQU 0 (echo  OK  chronostore_bench.exe) else (echo  FAIL  exit %ERRORLEVEL% & goto :end)

echo.
echo All done! Run with:
echo   chronostore.exe
echo   chronostore_bench.exe

:end
