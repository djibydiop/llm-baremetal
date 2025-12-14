# Script pour METTRE À JOUR rapidement l'image bootable
# Utilisez ce script après chaque compilation pour mettre à jour BOOTX64.EFI

$ErrorActionPreference = "Stop"

Write-Host "=== Mise à jour de llama2_efi.img ===" -ForegroundColor Cyan

# Vérifier que l'image existe
if (-not (Test-Path "llama2_efi.img")) {
    Write-Host "ERREUR: llama2_efi.img introuvable. Lancez d'abord create-bootable-image.ps1" -ForegroundColor Red
    exit 1
}

# Vérifier que le binaire existe
if (-not (Test-Path "llama2.efi")) {
    Write-Host "ERREUR: llama2.efi introuvable. Compilez d'abord avec 'wsl make'" -ForegroundColor Red
    exit 1
}

Write-Host "Montage de l'image..." -ForegroundColor Yellow
wsl bash -c "mkdir -p /tmp/llama2_mount"
wsl bash -c "sudo umount /tmp/llama2_mount 2>/dev/null || true"
wsl bash -c "sudo mount -o loop llama2_efi.img /tmp/llama2_mount"

Write-Host "Mise à jour de BOOTX64.EFI..." -ForegroundColor Yellow
wsl bash -c "sudo cp llama2.efi /tmp/llama2_mount/EFI/BOOT/BOOTX64.EFI"

Write-Host "Synchronisation et démontage..." -ForegroundColor Yellow
wsl bash -c "sudo sync"
wsl bash -c "sudo umount /tmp/llama2_mount"

Write-Host ""
Write-Host "=== IMAGE MISE À JOUR ===" -ForegroundColor Green
Write-Host "Vous pouvez maintenant copier llama2_efi.img sur USB" -ForegroundColor Green
