# Script de déploiement flexible - détecte automatiquement la clé USB

param(
    [string]$UsbDrive = ""
)

$ErrorActionPreference = "Stop"

Write-Host "=== COMPILATION & DÉPLOIEMENT ===" -ForegroundColor Cyan

# Compilation
Write-Host "`n[1/2] Compilation..." -ForegroundColor Yellow
wsl make clean
wsl make

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERREUR de compilation!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Compilation réussie (llama2.efi)" -ForegroundColor Green

# Déploiement
Write-Host "`n[2/2] Déploiement..." -ForegroundColor Yellow

# Si pas de lecteur spécifié, tenter de le détecter
if ($UsbDrive -eq "") {
    $possibleDrives = @("D:", "E:", "F:", "G:")
    foreach ($drive in $possibleDrives) {
        if (Test-Path "$drive\EFI\BOOT") {
            $UsbDrive = $drive
            Write-Host "✓ Clé USB détectée: $UsbDrive" -ForegroundColor Cyan
            break
        }
    }
}

# Déployer si trouvé
if ($UsbDrive -ne "" -and (Test-Path "$UsbDrive\EFI\BOOT")) {
    Copy-Item "llama2.efi" "$UsbDrive\EFI\BOOT\BOOTX64.EFI" -Force
    Write-Host "✓ Déployé sur USB: $UsbDrive\EFI\BOOT\BOOTX64.EFI" -ForegroundColor Green
} else {
    Write-Host "⚠ Clé USB non trouvée - binaire compilé mais non copié" -ForegroundColor Yellow
    Write-Host "  Utilisez: .\deploy.ps1 -UsbDrive X:" -ForegroundColor Gray
    Write-Host "  Ou copiez manuellement llama2.efi vers votre USB" -ForegroundColor Gray
}

Write-Host "`n=== TERMINÉ ===" -ForegroundColor Green
