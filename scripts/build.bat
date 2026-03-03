@echo off
REM TEdit Windows Build Script (Batch wrapper)
REM Usage: build.bat [-Release] [-Clean]

setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%build.ps1"

REM Check for PowerShell
where pwsh >nul 2>nul
if %ERRORLEVEL% equ 0 (
    pwsh -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*
    goto :end
)

where powershell >nul 2>nul
if %ERRORLEVEL% equ 0 (
    powershell -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*
    goto :end
)

echo [ERROR] PowerShell not found
exit /b 1

:end
exit /b %ERRORLEVEL%
