# RBX External - Automatic Offset Updater (PowerShell)
# Fetches latest offsets from rbxoffsets.xyz API

$ErrorActionPreference = "Stop"

$API_BASE = "https://rbxoffsets.xyz"
$HEADERS = @{"rbxoffsets.xyz" = "apiv1"}
$OFFSET_FILE = "src\engine\offset.h"

function Write-ColorOutput($ForegroundColor) {
    $fc = $host.UI.RawUI.ForegroundColor
    $host.UI.RawUI.ForegroundColor = $ForegroundColor
    if ($args) {
        Write-Output $args
    }
    $host.UI.RawUI.ForegroundColor = $fc
}

function Get-LatestVersion {
    try {
        $response = Invoke-WebRequest -Uri "$API_BASE/api/version/raw" -Headers $HEADERS -UseBasicParsing
        return $response.Content.Trim()
    }
    catch {
        Write-ColorOutput Red "[-] Error fetching version: $_"
        return $null
    }
}

function Get-LatestOffsets {
    try {
        $response = Invoke-WebRequest -Uri "$API_BASE/api/latest/raw" -Headers $HEADERS -UseBasicParsing
        return $response.Content
    }
    catch {
        Write-ColorOutput Red "[-] Error fetching offsets: $_"
        return $null
    }
}

function Get-CurrentVersion {
    try {
        $content = Get-Content $OFFSET_FILE -Raw
        if ($content -match 'ClientVersion = "([^"]+)"') {
            return $matches[1]
        }
    }
    catch {
        Write-ColorOutput Red "[-] Error reading current version: $_"
    }
    return $null
}

function Update-OffsetFile {
    param(
        [string]$content,
        [string]$version
    )
    
    try {
        $output = @"
#pragma once

#include <cstdint>
#include <string>
namespace offset {
    inline std::string ClientVersion = "$version";

$content
}

"@
        
        Set-Content -Path $OFFSET_FILE -Value $output -NoNewline
        return $true
    }
    catch {
        Write-ColorOutput Red "[-] Error writing offset file: $_"
        return $false
    }
}

# Main execution
Write-Host "=" * 50
Write-Host "   RBX External - Offset Updater"
Write-Host "=" * 50
Write-Host ""

# Get current version
Write-Host "[*] Reading current version..."
$currentVersion = Get-CurrentVersion
if (-not $currentVersion) {
    Write-ColorOutput Red "[-] Could not read current version"
    Read-Host "Press Enter to exit"
    exit 1
}

Write-ColorOutput Green "[*] Current version: $currentVersion"
Write-Host "[*] Checking for updates..."
Write-Host ""

# Get latest version
$latestVersion = Get-LatestVersion
if (-not $latestVersion) {
    Write-ColorOutput Red "[-] Failed to fetch latest version from API"
    Read-Host "Press Enter to exit"
    exit 1
}

Write-ColorOutput Cyan "[*] Latest version: $latestVersion"

# Check if update needed
if ($latestVersion -eq $currentVersion) {
    Write-ColorOutput Green "[+] Offsets are already up to date!"
    Read-Host "Press Enter to exit"
    exit 0
}

Write-ColorOutput Yellow "[!] Update available: $currentVersion -> $latestVersion"
Write-Host "[*] Downloading latest offsets..."

# Get latest offsets
$offsetsContent = Get-LatestOffsets
if (-not $offsetsContent) {
    Write-ColorOutput Red "[-] Failed to fetch latest offsets from API"
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "[*] Updating offset.h..."

# Update file
$success = Update-OffsetFile -content $offsetsContent -version $latestVersion
if (-not $success) {
    Write-ColorOutput Red "[-] Failed to update offset file"
    Read-Host "Press Enter to exit"
    exit 1
}

Write-ColorOutput Green "[+] Successfully updated offsets!"
Write-ColorOutput Green "[+] $currentVersion -> $latestVersion"
Write-Host ""
Write-ColorOutput Yellow "[!] Please rebuild the project to apply changes:"
Write-Host "    msbuild rbx-external.sln /p:Configuration=Release /p:Platform=x64"
Write-Host ""

Read-Host "Press Enter to exit"
exit 0
