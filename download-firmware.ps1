#!/usr/bin/env pwsh
# Download Intel AX200 WiFi Firmware

$firmwareFile = "iwlwifi-cc-a0-72.ucode"
$targetPath = "$PSScriptRoot\$firmwareFile"

Write-Host "=== Intel AX200 Firmware Download ===" -ForegroundColor Cyan
Write-Host ""

# Remove old file if exists
if (Test-Path $targetPath) {
    Write-Host "Removing old file..." -ForegroundColor Yellow
    Remove-Item $targetPath -Force
}

# Try multiple sources
$sources = @(
    "https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/$firmwareFile",
    "https://anduin.linuxfromscratch.org/sources/linux-firmware/$firmwareFile",
    "https://mirrors.edge.kernel.org/pub/linux/kernel/firmware/$firmwareFile"
)

$success = $false
foreach ($url in $sources) {
    Write-Host "Trying: $url" -ForegroundColor Yellow
    
    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $url -OutFile $targetPath -UseBasicParsing -TimeoutSec 30
        
        if (Test-Path $targetPath) {
            $size = (Get-Item $targetPath).Length
            
            # Firmware should be > 100 KB (real firmware is ~450 KB)
            if ($size -gt 100000) {
                Write-Host "✓ Download successful!" -ForegroundColor Green
                Write-Host "  File: $firmwareFile" -ForegroundColor Cyan
                Write-Host "  Size: $([math]::Round($size/1KB, 2)) KB" -ForegroundColor Cyan
                Write-Host "  Path: $targetPath" -ForegroundColor Cyan
                $success = $true
                break
            } else {
                Write-Host "✗ File too small ($size bytes) - likely HTML error page" -ForegroundColor Red
                Remove-Item $targetPath -Force
            }
        }
    } catch {
        Write-Host "✗ Failed: $_" -ForegroundColor Red
    }
}

if (-not $success) {
    Write-Host ""
    Write-Host "=== Manual Download Required ===" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Please download manually from:" -ForegroundColor White
    Write-Host "  https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/"
    Write-Host ""
    Write-Host "Steps:" -ForegroundColor Cyan
    Write-Host "  1. Navigate to the URL above"
    Write-Host "  2. Search for 'iwlwifi-cc-a0-72.ucode'"
    Write-Host "  3. Click on the file"
    Write-Host "  4. Click 'plain' link to download raw file"
    Write-Host "  5. Save to: $PSScriptRoot"
    Write-Host ""
    Write-Host "Alternative: Clone the repository" -ForegroundColor Cyan
    Write-Host "  git clone https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git"
    Write-Host "  cp linux-firmware/iwlwifi-cc-a0-72.ucode $PSScriptRoot"
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "=== Next Steps ===" -ForegroundColor Green
    Write-Host "1. Add firmware to USB image:"
    Write-Host "   wsl bash -c 'make usb-image'"
    Write-Host "   # Then manually add firmware to mounted image"
    Write-Host ""
    Write-Host "2. Or recreate disk image:"
    Write-Host "   wsl bash -c 'make disk'"
    Write-Host ""
}
