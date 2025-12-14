# Workflow principal : Compile + Met à jour llama2_efi.img
# Ensuite utilisez Rufus pour écrire l'image sur USB

$ErrorActionPreference = "Stop"

Write-Host "=== COMPILATION & UPDATE IMAGE ===" -ForegroundColor Cyan

# Étape 1: Compilation
Write-Host "`n[1/2] Compilation..." -ForegroundColor Yellow
wsl make clean
wsl make

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERREUR de compilation!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Compilation réussie" -ForegroundColor Green

# Étape 2: Mise à jour de l'image
Write-Host "`n[2/2] Mise à jour de llama2_efi.img..." -ForegroundColor Yellow

if (-not (Test-Path "llama2_efi.img")) {
    Write-Host "Image non trouvée, création..." -ForegroundColor Yellow
    .\create-bootable-image.ps1
} else {
    wsl bash -c "mkdir -p /tmp/llama2_mount"
    wsl bash -c "sudo umount /tmp/llama2_mount 2>/dev/null || true"
    wsl bash -c "sudo mount -o loop llama2_efi.img /tmp/llama2_mount"
    wsl bash -c "sudo cp llama2.efi /tmp/llama2_mount/EFI/BOOT/BOOTX64.EFI"
    wsl bash -c "sudo sync"
    wsl bash -c "sudo umount /tmp/llama2_mount"
}

Write-Host "`n=== TERMINÉ ===" -ForegroundColor Green
Write-Host "✓ llama2_efi.img mis à jour (550 MB)" -ForegroundColor Green
Write-Host ""
Write-Host "📌 Utilisez Rufus pour écrire l'image sur USB" -ForegroundColor Cyan
Write-Host "   Fichier: llama2_efi.img" -ForegroundColor White
