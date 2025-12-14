# Déploiement USB Direct - Sans lettre de lecteur
# Usage: .\deploy-usb-direct.ps1

$ErrorActionPreference = "Stop"

Write-Host "`n=== DÉPLOIEMENT USB DIRECT ===" -ForegroundColor Cyan
Write-Host "Accès direct au volume sans lettre de lecteur`n" -ForegroundColor Yellow

# Trouver la clé USB
$usbDisks = Get-Disk | Where-Object { $_.BusType -eq 'USB' }
if ($usbDisks.Count -eq 0) {
    Write-Host "✗ Aucune clé USB!" -ForegroundColor Red
    exit 1
}

$disk = $usbDisks[0]
Write-Host "✓ Clé: $($disk.FriendlyName)" -ForegroundColor Green

# Trouver le volume
$volume = Get-Volume | Where-Object { $_.FileSystemLabel -eq 'LLM-BOOT' -or ($_.DriveType -eq 'Removable' -and $_.FileSystem -eq 'FAT32') }

if (!$volume) {
    Write-Host "✗ Volume introuvable!" -ForegroundColor Red
    Write-Host "`n📝 SOLUTION MANUELLE:" -ForegroundColor Yellow
    Write-Host "1. Ouvrir l'Explorateur Windows" -ForegroundColor White
    Write-Host "2. Clic droit sur 'Ce PC' → Gérer → Gestion des disques" -ForegroundColor White
    Write-Host "3. Trouver la clé USB (LLM-BOOT)" -ForegroundColor White
    Write-Host "4. Clic droit → 'Modifier la lettre de lecteur'" -ForegroundColor White
    Write-Host "5. Assigner E: (ou autre lettre libre)" -ForegroundColor White
    Write-Host "6. Relancer: .\deploy-usb-manual.ps1 E" -ForegroundColor White
    exit 1
}

# Si volume a une lettre, utiliser directement
if ($volume.DriveLetter) {
    $usbPath = "$($volume.DriveLetter):"
    Write-Host "✓ Lecteur: $usbPath`n" -ForegroundColor Green
} else {
    Write-Host "⚠️  Volume sans lettre de lecteur!" -ForegroundColor Yellow
    Write-Host "`n📝 UTILISER SCRIPT MANUEL:" -ForegroundColor Cyan
    Write-Host ".\deploy-usb-manual.ps1" -ForegroundColor White
    exit 1
}

# Vérifier fichiers
if (!(Test-Path "llama2.efi")) {
    Write-Host "✗ llama2.efi manquant!" -ForegroundColor Red
    exit 1
}
if (!(Test-Path "stories15M.bin")) {
    Write-Host "✗ stories15M.bin manquant!" -ForegroundColor Red
    exit 1
}
if (!(Test-Path "tokenizer.bin")) {
    Write-Host "✗ tokenizer.bin manquant!" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Tous les fichiers présents`n" -ForegroundColor Green

# Créer structure
$efiPath = "$usbPath\EFI\BOOT"
New-Item -Path $efiPath -ItemType Directory -Force | Out-Null
Write-Host "✓ Structure EFI créée" -ForegroundColor Green

# Copier
Write-Host "`nCopie en cours..." -ForegroundColor Cyan
Copy-Item "llama2.efi" -Destination "$efiPath\BOOTX64.EFI" -Force
Copy-Item "stories15M.bin" -Destination "$usbPath\stories15M.bin" -Force
Copy-Item "tokenizer.bin" -Destination "$usbPath\tokenizer.bin" -Force

Write-Host "✓ Fichiers copiés!" -ForegroundColor Green
Write-Host "`n🚀 CLÉ USB PRÊTE À BOOTER!" -ForegroundColor Green
