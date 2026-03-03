#!/usr/bin/env pwsh
# TEdit Windows Build Script
# Usage: .\build.ps1 [-Release] [-Clean]

param(
    [switch]$Release,
    [switch]$Clean,
    [string]$Runtime = "win-x64"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$SrcDir = Join-Path $ProjectRoot "src"
$OutputDir = Join-Path $ProjectRoot "build"

Write-Host "====================================" -ForegroundColor Cyan
Write-Host "  TEdit Windows Build Script" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""

# Check for .NET SDK
try {
    $dotnetVersion = dotnet --version
    Write-Host "[OK] .NET SDK $dotnetVersion found" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] .NET SDK not found. Please install from https://dotnet.microsoft.com" -ForegroundColor Red
    exit 1
}

# Clean if requested
if ($Clean) {
    Write-Host "[*] Cleaning build artifacts..." -ForegroundColor Yellow
    if (Test-Path $OutputDir) {
        Remove-Item -Recurse -Force $OutputDir
    }

    $binDir = Join-Path $SrcDir "bin"
    $objDir = Join-Path $SrcDir "obj"
    if (Test-Path $binDir) { Remove-Item -Recurse -Force $binDir }
    if (Test-Path $objDir) { Remove-Item -Recurse -Force $objDir }

    Write-Host "[OK] Cleaned" -ForegroundColor Green
}

# Create output directory
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

# Build configuration
$Config = if ($Release) { "Release" } else { "Debug" }
Write-Host "[*] Building $Config for $Runtime..." -ForegroundColor Yellow

Push-Location $SrcDir
try {
    if ($Release) {
        # Publish as single file
        dotnet publish -c Release -r $Runtime -o $OutputDir --self-contained true /p:PublishSingleFile=true /p:PublishTrimmed=true
    } else {
        # Standard debug build
        dotnet build -c Debug -o $OutputDir
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Build failed" -ForegroundColor Red
        exit 1
    }

    Write-Host "[OK] Build successful" -ForegroundColor Green
} finally {
    Pop-Location
}

# Copy default plugins and themes
$PluginsSrc = Join-Path $ProjectRoot "plugins"
$ThemesSrc = Join-Path $ProjectRoot "themes"
$PluginsDst = Join-Path $OutputDir "plugins"
$ThemesDst = Join-Path $OutputDir "themes"

if (Test-Path $PluginsSrc) {
    if (!(Test-Path $PluginsDst)) { New-Item -ItemType Directory -Path $PluginsDst | Out-Null }
    Copy-Item "$PluginsSrc\*" -Destination $PluginsDst -Recurse -Force
    Write-Host "[OK] Copied plugins" -ForegroundColor Green
}

if (Test-Path $ThemesSrc) {
    if (!(Test-Path $ThemesDst)) { New-Item -ItemType Directory -Path $ThemesDst | Out-Null }
    Copy-Item "$ThemesSrc\*" -Destination $ThemesDst -Recurse -Force
    Write-Host "[OK] Copied themes" -ForegroundColor Green
}

Write-Host ""
Write-Host "====================================" -ForegroundColor Cyan
Write-Host "  Build Complete!" -ForegroundColor Green
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Output: $OutputDir" -ForegroundColor White
Write-Host ""
Write-Host "To run:" -ForegroundColor Yellow
Write-Host "  $OutputDir\tedit.exe [file]" -ForegroundColor White
Write-Host ""
Write-Host "To install system-wide:" -ForegroundColor Yellow
Write-Host "  .\scripts\install.ps1" -ForegroundColor White
