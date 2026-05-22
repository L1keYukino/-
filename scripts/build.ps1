param(
    [string]$BuildType = "Release",
    [string]$Generator = "Ninja"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projRoot = Split-Path -Parent $scriptDir
$buildDir = Join-Path $projRoot "build"

Write-Host "=== Voice Input Method - Build ===" -ForegroundColor Cyan
Write-Host "Build type : $BuildType"
Write-Host "Generator  : $Generator"
Write-Host "Build dir  : $buildDir"
Write-Host ""

# Configure
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Push-Location $buildDir
try {
    Write-Host "[1/3] Configuring CMake..." -ForegroundColor Yellow
    cmake .. -G $Generator -DCMAKE_BUILD_TYPE=$BuildType -DVIM_BUILD_TESTS=ON

    Write-Host "[2/3] Building..." -ForegroundColor Yellow
    cmake --build . --config $BuildType

    Write-Host "[3/3] Running tests..." -ForegroundColor Yellow
    ctest --output-on-failure -C $BuildType
}
finally {
    Pop-Location
}

Write-Host "`n=== Build complete ===" -ForegroundColor Green
