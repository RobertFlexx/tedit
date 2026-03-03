#!/usr/bin/env pwsh
# TEdit Windows Install Script
# Usage: .\install.ps1 [-Path <install-path>] [-AddToPath] [-CreateShortcut]

param(
    [string]$Path = "$env:LOCALAPPDATA\tedit",
    [switch]$AddToPath,
    [switch]$CreateShortcut,
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$BuildDir = Join-Path $ProjectRoot "build"

function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-Host "====================================" -ForegroundColor Cyan
    Write-Host "  $Text" -ForegroundColor Cyan
    Write-Host "====================================" -ForegroundColor Cyan
    Write-Host ""
}

function Write-Step {
    param([string]$Text)
    Write-Host "[*] $Text" -ForegroundColor Yellow
}

function Write-Ok {
    param([string]$Text)
    Write-Host "[OK] $Text" -ForegroundColor Green
}

function Write-Err {
    param([string]$Text)
    Write-Host "[ERROR] $Text" -ForegroundColor Red
}

# Handle uninstall
if ($Uninstall) {
    Write-Header "TEdit Uninstaller"

    if (Test-Path $Path) {
        Write-Step "Removing installation directory..."
        Remove-Item -Recurse -Force $Path
        Write-Ok "Removed $Path"
    }

    # Remove from PATH
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($currentPath -like "*$Path*") {
        Write-Step "Removing from PATH..."
        $newPath = ($currentPath -split ';' | Where-Object { $_ -ne $Path }) -join ';'
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        Write-Ok "Removed from PATH"
    }

    # Remove shortcut
    $shortcutPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "TEdit.lnk"
    if (Test-Path $shortcutPath) {
        Remove-Item $shortcutPath
        Write-Ok "Removed desktop shortcut"
    }

    Write-Host ""
    Write-Host "TEdit has been uninstalled." -ForegroundColor Green
    exit 0
}

Write-Header "TEdit Windows Installer"

# Check if build exists
if (!(Test-Path (Join-Path $BuildDir "tedit.exe"))) {
    Write-Err "tedit.exe not found in build directory"
    Write-Host "Please run build.ps1 -Release first" -ForegroundColor Yellow
    exit 1
}

# Create install directory
Write-Step "Creating installation directory..."
if (!(Test-Path $Path)) {
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}
Write-Ok "Created $Path"

# Copy files
Write-Step "Copying files..."
Copy-Item "$BuildDir\*" -Destination $Path -Recurse -Force
Write-Ok "Copied files"

# Create config directory
$ConfigDir = Join-Path $env:APPDATA "tedit"
if (!(Test-Path $ConfigDir)) {
    New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $ConfigDir "plugins") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $ConfigDir "themes") -Force | Out-Null
    Write-Ok "Created config directory"
}

# Copy default plugins and themes to config
$PluginsSrc = Join-Path $Path "plugins"
$ThemesSrc = Join-Path $Path "themes"
$PluginsDst = Join-Path $ConfigDir "plugins"
$ThemesDst = Join-Path $ConfigDir "themes"

if (Test-Path $PluginsSrc) {
    Copy-Item "$PluginsSrc\*" -Destination $PluginsDst -Force -ErrorAction SilentlyContinue
}
if (Test-Path $ThemesSrc) {
    Copy-Item "$ThemesSrc\*" -Destination $ThemesDst -Force -ErrorAction SilentlyContinue
}

# Add to PATH
if ($AddToPath) {
    Write-Step "Adding to PATH..."
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($currentPath -notlike "*$Path*") {
        [Environment]::SetEnvironmentVariable("Path", "$currentPath;$Path", "User")
        Write-Ok "Added to PATH (restart terminal to use)"
    } else {
        Write-Ok "Already in PATH"
    }
}

# Create desktop shortcut
if ($CreateShortcut) {
    Write-Step "Creating desktop shortcut..."
    $WshShell = New-Object -ComObject WScript.Shell
    $ShortcutPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "TEdit.lnk"
    $Shortcut = $WshShell.CreateShortcut($ShortcutPath)
    $Shortcut.TargetPath = Join-Path $Path "tedit.exe"
    $Shortcut.WorkingDirectory = [Environment]::GetFolderPath("UserProfile")
    $Shortcut.Description = "TEdit - Terminal Text Editor"
    $Shortcut.Save()
    Write-Ok "Created desktop shortcut"
}

Write-Host ""
Write-Host "====================================" -ForegroundColor Cyan
Write-Host "  Installation Complete!" -ForegroundColor Green
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Installation path: $Path" -ForegroundColor White
Write-Host "Config directory:  $ConfigDir" -ForegroundColor White
Write-Host ""

if (!$AddToPath) {
    Write-Host "To add to PATH, run:" -ForegroundColor Yellow
    Write-Host "  .\install.ps1 -AddToPath" -ForegroundColor White
    Write-Host ""
    Write-Host "Or manually add to PATH:" -ForegroundColor Yellow
    Write-Host "  $Path" -ForegroundColor White
    Write-Host ""
}

Write-Host "To run TEdit:" -ForegroundColor Yellow
if ($AddToPath) {
    Write-Host "  tedit [file]" -ForegroundColor White
} else {
    Write-Host "  $Path\tedit.exe [file]" -ForegroundColor White
}
Write-Host ""
Write-Host "To uninstall:" -ForegroundColor Yellow
Write-Host "  .\install.ps1 -Uninstall" -ForegroundColor White
