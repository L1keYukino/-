param(
    [switch]$SkipModels
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projRoot = Split-Path -Parent $scriptDir

Write-Host "=== Voice Input Method - Dependency Setup ===" -ForegroundColor Cyan

# Check prerequisites
Write-Host "[1/4] Checking prerequisites..." -ForegroundColor Yellow

$cmakeVer = cmake --version 2>$null
if (-not $?) { throw "CMake is required but not found." }
Write-Host "  CMake: OK"

$pythonVer = python --version 2>$null
if (-not $?) { Write-Host "  Python: not found (optional, for scripts)" }
else { Write-Host "  Python: OK" }

# Detect compiler
if (Get-Command "cl" -ErrorAction SilentlyContinue) {
    Write-Host "  Compiler: MSVC (detected)"
}
elseif (Get-Command "g++" -ErrorAction SilentlyContinue) {
    g++ --version | Select-Object -First 1
    Write-Host "  Compiler: MinGW/g++ (detected)"
}
else {
    Write-Host "  WARNING: No supported compiler detected (MSVC or MinGW)"
}

# ─── Model download section (placeholder) ──────────────────
if (-not $SkipModels) {
    Write-Host "[2/4] Model download..." -ForegroundColor Yellow
    $modelDir = Join-Path $projRoot "models"
    New-Item -ItemType Directory -Force -Path $modelDir | Out-Null

    Write-Host "  NOTE: Model downloads are manual for now."
    Write-Host "  Place the following in $modelDir :"
    Write-Host "    - SenseVoice model files → $modelDir\sensevoice\"
    Write-Host "    - Qwen2.5-3B GGUF file   → $modelDir\qwen2.5-3b-q4_k_m.gguf"
    Write-Host ""
    Write-Host "  Download URLs:"
    Write-Host "    SenseVoice: https://github.com/k2-fsa/sherpa-onnx/releases"
    Write-Host "    Qwen GGUF:  https://huggingface.co/Qwen/Qwen2.5-3B-Instruct-GGUF"
}

# ─── FetchContent note ─────────────────────────────────────
Write-Host "[3/4] CMake FetchContent dependencies..." -ForegroundColor Yellow
Write-Host "  The following will be auto-fetched by CMake on first build:"
Write-Host "    - nlohmann/json (v3.11.3)"
Write-Host "    - spdlog (v1.14.1)"
Write-Host "    - Catch2 (v3.7.0, for tests)"

# ─── Done ──────────────────────────────────────────────────
Write-Host "[4/4] Setup complete." -ForegroundColor Green
Write-Host "  Next: run '.\scripts\build.ps1' to configure and build."
