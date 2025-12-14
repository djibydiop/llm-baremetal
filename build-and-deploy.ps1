# Workflow de développement complet
# Compile, met à jour l'image bootable ET copie sur USB

$ErrorActionPreference = "Stop"

Write-Host "=== BUILD & DEPLOY COMPLET ===" -ForegroundColor Cyan

# Étape 1: Compilation
Write-Host "`n[1/3] Compilation..." -ForegroundColor Yellow
wsl make clean
wsl make

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERREUR de compilation!" -ForegroundColor Red
    exit 1
}

# Étape 2: Mise à jour de l'image
Write-Host "`n[2/3] Mise à jour de llama2_efi.img..." -ForegroundColor Yellow
.\update-bootable-image.ps1

# Étape 3: Copie sur USB (D:)
Write-Host "`n[3/3] Copie directe sur USB (D:\EFI\BOOT\BOOTX64.EFI)..." -ForegroundColor Yellow
Copy-Item "llama2.efi" "D:\EFI\BOOT\BOOTX64.EFI" -Force

Write-Host "`n=== DÉPLOIEMENT TERMINÉ ===" -ForegroundColor Green
Write-Host "✓ Image bootable: llama2_efi.img (à copier avec dd si besoin)" -ForegroundColor Green
Write-Host "✓ USB direct: D:\EFI\BOOT\BOOTX64.EFI mis à jour" -ForegroundColor Green
