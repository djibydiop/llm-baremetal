#!/usr/bin/env pwsh
# Deploy LLM Bare-Metal System to USB Drive for Hardware Boot
# Usage: .\deploy-usb.ps1 -DriveLetter E

param(
    [Parameter(Mandatory=$true)]
    [string]$DriveLetter
)

$ErrorActionPreference = "Stop"

Write-Host "=== LLM Bare-Metal USB Deployment ===" -ForegroundColor Cyan

# Remove colon if present
$DriveLetter = $DriveLetter.TrimEnd(':')
$USBPath = "${DriveLetter}:"

# Verify USB drive exists
if (!(Test-Path $USBPath)) {
    Write-Host "[ERROR] Drive $USBPath not found!" -ForegroundColor Red
    exit 1
}

Write-Host "[INFO] Target USB drive: $USBPath" -ForegroundColor Yellow

# Check if drive is formatted as FAT32
$volume = Get-Volume -DriveLetter $DriveLetter -ErrorAction SilentlyContinue
if ($volume.FileSystem -ne "FAT32") {
    Write-Host "[WARNING] Drive is not FAT32 (found: $($volume.FileSystem))" -ForegroundColor Yellow
    Write-Host "[INFO] UEFI boot requires FAT32. Please reformat drive if boot fails." -ForegroundColor Yellow
}

# Create EFI directory structure
$EFIBootPath = "$USBPath\EFI\BOOT"
Write-Host "[INFO] Creating EFI directory structure..." -ForegroundColor Cyan
New-Item -Path $EFIBootPath -ItemType Directory -Force | Out-Null

# Build the system first
Write-Host "`n[INFO] Building LLM system..." -ForegroundColor Cyan
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && make clean && make"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit 1
}

# Copy UEFI bootloader
Write-Host "[INFO] Copying UEFI bootloader..." -ForegroundColor Cyan
Copy-Item "llama2_efi.efi" -Destination "$EFIBootPath\BOOTX64.EFI" -Force
Write-Host "  ✓ BOOTX64.EFI copied" -ForegroundColor Green

# Copy model file
Write-Host "[INFO] Copying model (stories110M.bin - 420MB)..." -ForegroundColor Cyan
if (Test-Path "stories110M.bin") {
    Copy-Item "stories110M.bin" -Destination "$USBPath\stories110M.bin" -Force
    Write-Host "  ✓ stories110M.bin copied" -ForegroundColor Green
} else {
    Write-Host "  [WARNING] stories110M.bin not found - download it separately!" -ForegroundColor Yellow
}

# Copy tokenizer
Write-Host "[INFO] Copying tokenizer (tokenizer.bin - 434KB)..." -ForegroundColor Cyan
if (Test-Path "tokenizer.bin") {
    Copy-Item "tokenizer.bin" -Destination "$USBPath\tokenizer.bin" -Force
    Write-Host "  ✓ tokenizer.bin copied" -ForegroundColor Green
} else {
    Write-Host "  [WARNING] tokenizer.bin not found!" -ForegroundColor Yellow
}

# Create README on USB
Write-Host "[INFO] Creating README..." -ForegroundColor Cyan
$readmeContent = @"
LLM Bare-Metal UEFI System
===========================

This USB drive contains a complete LLM inference system that boots directly
on UEFI hardware without an operating system.

Contents:
- EFI\BOOT\BOOTX64.EFI - UEFI bootloader with LLM implementation
- stories110M.bin - 110M parameter language model (420MB)
- tokenizer.bin - BPE tokenizer for text encoding/decoding (434KB)

Boot Instructions:
1. Insert USB drive into target machine
2. Enter BIOS/UEFI settings (F2, F10, F12, or DEL key at boot)
3. Set USB drive as first boot device
4. Save and exit BIOS
5. System will boot and load LLM automatically

Hardware Requirements:
- UEFI firmware (most PCs since 2010)
- x86-64 CPU with AVX2 support
- 4GB+ RAM
- USB boot capability

Features:
- Interactive menu with 6 categories (Stories, Science, Adventure, Philosophy, History, Technology)
- 41 pre-configured prompts
- BPE tokenization for prompt understanding
- AVX2 optimized inference
- Temperature-controlled generation

Troubleshooting:
- If boot fails, ensure USB is FAT32 formatted
- Check Secure Boot is disabled in BIOS
- Verify UEFI boot mode (not Legacy/CSM)

GitHub: https://github.com/djibydiop/llm-baremetal
"@

Set-Content -Path "$USBPath\README.txt" -Value $readmeContent -Force
Write-Host "  ✓ README.txt created" -ForegroundColor Green

# Summary
Write-Host "`n=== Deployment Complete ===" -ForegroundColor Green
Write-Host "USB Drive: $USBPath" -ForegroundColor Cyan
Write-Host "Files deployed:" -ForegroundColor Cyan
Write-Host "  - EFI\BOOT\BOOTX64.EFI" -ForegroundColor White
Write-Host "  - stories110M.bin (420MB)" -ForegroundColor White
Write-Host "  - tokenizer.bin (434KB)" -ForegroundColor White
Write-Host "  - README.txt" -ForegroundColor White
Write-Host "`nYou can now boot from this USB drive on any UEFI x86-64 machine!" -ForegroundColor Green
Write-Host "Remember to disable Secure Boot in BIOS if boot fails." -ForegroundColor Yellow
