# ============================================================================
# Script: create-usb-boot.ps1
# Description: Crée une clé USB bootable avec LLM Bare-Metal
# Usage: .\create-usb-boot.ps1 -DriveLetter E
# ============================================================================

param(
    [Parameter(Mandatory=$true)]
    [string]$DriveLetter
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  USB BOOTABLE CREATION" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Vérifier que le lecteur existe
if (-not (Test-Path "${DriveLetter}:\")) {
    Write-Host "[ERROR] Drive ${DriveLetter}:\ not found!" -ForegroundColor Red
    Write-Host "Available drives:" -ForegroundColor Yellow
    Get-Volume | Where-Object {$_.DriveLetter} | Format-Table DriveLetter, FileSystemLabel, Size, FileSystem
    exit 1
}

Write-Host "[INFO] Target drive: ${DriveLetter}:\" -ForegroundColor Green
Write-Host ""

# Warning
Write-Host "[WARNING] This will DELETE ALL DATA on ${DriveLetter}:\!" -ForegroundColor Yellow
Write-Host "Press CTRL+C to cancel, or any key to continue..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
Write-Host ""

# Vérifier que les fichiers existent
$requiredFiles = @(
    "llama2.efi",
    "stories15M.bin",
    "tokenizer.bin"
)

Write-Host "[CHECK] Verifying required files..." -ForegroundColor Cyan
foreach ($file in $requiredFiles) {
    if (-not (Test-Path $file)) {
        Write-Host "[ERROR] Missing file: $file" -ForegroundColor Red
        Write-Host "Run 'make' first to compile llama2.efi" -ForegroundColor Yellow
        exit 1
    }
    $size = (Get-Item $file).Length / 1MB
    Write-Host "  [OK] $file ($([math]::Round($size, 2)) MB)" -ForegroundColor Green
}
Write-Host ""

# Formater la clé USB en FAT32
Write-Host "[STEP 1] Formatting ${DriveLetter}:\ as FAT32..." -ForegroundColor Cyan
Format-Volume -DriveLetter $DriveLetter -FileSystem FAT32 -NewFileSystemLabel "LLM-BOOT" -Confirm:$false
Write-Host "  [OK] Formatted" -ForegroundColor Green
Write-Host ""

# Créer la structure EFI
Write-Host "[STEP 2] Creating EFI boot structure..." -ForegroundColor Cyan
$efiPath = "${DriveLetter}:\EFI\BOOT"
New-Item -Path $efiPath -ItemType Directory -Force | Out-Null
Write-Host "  [OK] Created $efiPath" -ForegroundColor Green
Write-Host ""

# Copier les fichiers
Write-Host "[STEP 3] Copying files..." -ForegroundColor Cyan

# Copier llama2.efi -> BOOTX64.EFI
Write-Host "  Copying llama2.efi -> BOOTX64.EFI..." -ForegroundColor Yellow
Copy-Item -Path "llama2.efi" -Destination "$efiPath\BOOTX64.EFI" -Force
Write-Host "  [OK] BOOTX64.EFI" -ForegroundColor Green

# Copier stories15M.bin
Write-Host "  Copying stories15M.bin..." -ForegroundColor Yellow
Copy-Item -Path "stories15M.bin" -Destination "${DriveLetter}:\stories15M.bin" -Force
Write-Host "  [OK] stories15M.bin" -ForegroundColor Green

# Copier tokenizer.bin
Write-Host "  Copying tokenizer.bin..." -ForegroundColor Yellow
Copy-Item -Path "tokenizer.bin" -Destination "${DriveLetter}:\tokenizer.bin" -Force
Write-Host "  [OK] tokenizer.bin" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  USB BOOTABLE READY!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "USB Structure:" -ForegroundColor Cyan
Write-Host "  ${DriveLetter}:\" -ForegroundColor White
Write-Host "    EFI\" -ForegroundColor White
Write-Host "      BOOT\" -ForegroundColor White
Write-Host "        BOOTX64.EFI" -ForegroundColor Green
Write-Host "    stories15M.bin" -ForegroundColor Green
Write-Host "    tokenizer.bin" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Safely eject the USB drive" -ForegroundColor White
Write-Host "  2. Insert into target computer" -ForegroundColor White
Write-Host "  3. Boot from USB (F12/F2/DEL in BIOS)" -ForegroundColor White
Write-Host "  4. Select UEFI Boot from USB" -ForegroundColor White
Write-Host ""
Write-Host "Enjoy your LLM running on bare metal! Made in Dakar, Senegal" -ForegroundColor Magenta
