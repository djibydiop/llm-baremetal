#!/usr/bin/env pwsh
# Recréation d'image USB compatible Rufus

$ErrorActionPreference = "Stop"

Write-Host "`n╔════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║    CRÉATION IMAGE USB COMPATIBLE RUFUS                ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Supprimer ancienne image
if (Test-Path llm-baremetal-usb.img) {
    Write-Host "[1/5] Suppression ancienne image..." -ForegroundColor Yellow
    Remove-Item llm-baremetal-usb.img -Force
}

# Créer nouvelle image
Write-Host "[2/5] Création image 256 MB..." -ForegroundColor Yellow
wsl dd if=/dev/zero of=llm-baremetal-usb.img bs=1M count=256 2>&1 | Out-Null
Write-Host "      ✓ Fichier créé" -ForegroundColor Green

# GPT
Write-Host "[3/5] Configuration partition GPT..." -ForegroundColor Yellow
wsl sgdisk -Z llm-baremetal-usb.img 2>&1 | Out-Null
wsl sgdisk -n 1:2048:0 -t 1:EF00 -c 1:'EFI System' llm-baremetal-usb.img 2>&1 | Out-Null
Write-Host "      ✓ Partition GPT créée" -ForegroundColor Green

# Format FAT32
Write-Host "[4/5] Formatage FAT32 et copie fichiers..." -ForegroundColor Yellow
$offset = 2048 * 512

wsl mformat -i llm-baremetal-usb.img@$offset -F -v LLMBOOT :: 2>&1 | Out-Null
wsl mmd -i llm-baremetal-usb.img@$offset ::/EFI 2>&1 | Out-Null
wsl mmd -i llm-baremetal-usb.img@$offset ::/EFI/BOOT 2>&1 | Out-Null
wsl mcopy -i llm-baremetal-usb.img@$offset llama2.efi ::/EFI/BOOT/BOOTX64.EFI 2>&1 | Out-Null
wsl mcopy -i llm-baremetal-usb.img@$offset iwlwifi-cc-a0-72.ucode ::/ 2>&1 | Out-Null
Write-Host "      ✓ Fichiers copiés" -ForegroundColor Green

# Vérification
Write-Host "[5/5] Vérification..." -ForegroundColor Yellow
$img = Get-Item llm-baremetal-usb.img
Write-Host "      ✓ Taille: $([math]::Round($img.Length/1MB, 2)) MB" -ForegroundColor Green

Write-Host "`nContenu de l'image:" -ForegroundColor Cyan
wsl mdir -i llm-baremetal-usb.img@$offset ::/

Write-Host "`n╔════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║              ✓ IMAGE USB PRÊTE!                       ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "RÉESSAYEZ AVEC RUFUS:" -ForegroundColor Yellow
Write-Host "  1. SELECT → llm-baremetal-usb.img" -ForegroundColor White
Write-Host "  2. Partition: GPT" -ForegroundColor White
Write-Host "  3. Target: UEFI (non CSM)" -ForegroundColor White
Write-Host "  4. START`n" -ForegroundColor White

Write-Host "Si Rufus refuse toujours, alternative:" -ForegroundColor Yellow
Write-Host "  - Utiliser balenaEtcher: https://etcher.balena.io/" -ForegroundColor Cyan
Write-Host "  - Ou Win32DiskImager" -ForegroundColor Cyan
Write-Host "  - Ou dd sous WSL directement sur USB`n" -ForegroundColor Cyan
