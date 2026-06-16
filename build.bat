@echo off
REM Moonjit Build Script (Windows Batch)
REM Usage: build.bat
REM Uses TCC (Tiny C Compiler) included in the repository.

setlocal

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

set TCC="%SCRIPT_DIR%tcc\tcc.exe"
set INCLUDE=-I"%SCRIPT_DIR%src"
set SOURCES=src\main.c src\memory.c src\value.c src\object.c src\chunk.c src\table.c src\lexer.c src\compiler.c src\vm.c src\gc.c src\host.c
set DEMO_SOURCES=examples\host_demo.c src\memory.c src\value.c src\object.c src\chunk.c src\table.c src\lexer.c src\compiler.c src\vm.c src\gc.c src\host.c

echo === Moonjit Build Script ===
echo.

if not exist %TCC% (
    echo [ERROR] TCC not found at: %TCC%
    echo Please ensure tcc\tcc.exe exists.
    exit /b 1
)

echo [1/2] Compiling moonjit.exe ...
%TCC% %INCLUDE% -o moonjit.exe %SOURCES%
if errorlevel 1 (
    echo [FAIL] moonjit.exe build failed
    exit /b 1
)
echo [OK] moonjit.exe built successfully

echo.
echo [2/2] Compiling host_demo.exe ...
%TCC% %INCLUDE% -o host_demo.exe %DEMO_SOURCES%
if errorlevel 1 (
    echo [FAIL] host_demo.exe build failed
    exit /b 1
)
echo [OK] host_demo.exe built successfully

echo.
echo === Build Complete ===
echo.
echo Output: moonjit.exe host_demo.exe
echo.
pause
