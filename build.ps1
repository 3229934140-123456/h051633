# Moonjit Build Script (PowerShell)
# Usage: .\build.ps1
# This script builds both the moonjit executable and the host integration demo.
# Uses TCC (Tiny C Compiler) which is included in the repository.

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$TccPath = Join-Path $ScriptDir "tcc\tcc.exe"
$SrcDir = Join-Path $ScriptDir "src"
$SrcFiles = @(
    "src\main.c",
    "src\memory.c",
    "src\value.c",
    "src\object.c",
    "src\chunk.c",
    "src\table.c",
    "src\lexer.c",
    "src\compiler.c",
    "src\vm.c",
    "src\gc.c",
    "src\host.c"
)

Write-Host "=== Moonjit Build Script ===" -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $TccPath)) {
    Write-Host "[ERROR] TCC not found at: $TccPath" -ForegroundColor Red
    Write-Host "Please ensure tcc/ directory with tcc\tcc.exe" -ForegroundColor Red
    exit 1
}

Write-Host "[1/3] Compiling moonjit.exe ..." -ForegroundColor Green
& $TccPath -Isrc -o moonjit.exe @SrcFiles
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] Build moonjit.exe compilation failed" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] moonjit.exe built successfully" -ForegroundColor Green

Write-Host ""
Write-Host "[2/3] Compiling host_demo.exe ..." -ForegroundColor Green
$DemoSrc = @("examples\host_demo.c", "src\memory.c", "src\value.c", "src\object.c", "src\chunk.c", "src\table.c", "src\lexer.c", "src\compiler.c", "src\vm.c", "src\gc.c", "src\host.c")
& $TccPath -Isrc -o host_demo.exe @DemoSrc
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAIL] host_demo.exe compilation failed" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] host_demo.exe built successfully" -ForegroundColor Green

Write-Host ""
Write-Host "[3/3] Running tests ..." -ForegroundColor Yellow

Write-Host ""
Write-Host "--- Running hello.mj script test:" -ForegroundColor Yellow
.\moonjit.exe examples\hello.mj

Write-Host ""
Write-Host ""
Write-Host "--- Running host integration demo:" -ForegroundColor Yellow
.\host_demo.exe

Write-Host ""
Write-Host ""
Write-Host "=== Build Complete ===" -ForegroundColor Cyan
Write-Host "Output files:" -ForegroundColor Cyan
Write-Host "  - moonjit.exe  (scripting language interpreter)" -ForegroundColor Cyan
Write-Host "  - host_demo.exe (host integration demo)" -ForegroundColor Cyan
Write-Host ""
Write-Host "Usage:" -ForegroundColor Cyan
Write-Host "  .\moonjit.exe              (start REPL)" -ForegroundColor Cyan
Write-Host "  .\moonjit.exe script.mj    (execute script file)" -ForegroundColor Cyan
Write-Host "  .\host_demo.exe            (run host demo)" -ForegroundColor Cyan
