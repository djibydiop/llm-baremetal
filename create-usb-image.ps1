#!/usr/bin/env pwsh
# Script de création d'image USB bootable pour LLM Baremetal
# Avec firmware Intel AX200

$ErrorActionPreference = "Stop"

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  CRÉATION IMAGE USB BOOTABLE - LLM BAREMETAL + WIFI       ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$imgPath = "llm-baremetal-usb.img"
$imgSizeMB = 256

# Vérifier les fichiers requis
Write-Host "[1/6] Vérification des fichiers..." -ForegroundColor Yellow
$required = @(
    "llama2.efi",
    "iwlwifi-cc-a0-72.ucode"
)

foreach ($file in $required) {
    if (Test-Path $file) {
        $size = (Get-Item $file).Length
        Write-Host "  ✓ $file ($([math]::Round($size/1KB, 2)) KB)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $file MANQUANT!" -ForegroundColor Red
        exit 1
    }
}

# Fichiers optionnels
$optional = @(
    "..\llama2.c\stories15M.bin",
    "..\llama2.c\tokenizer.bin"
)

foreach ($file in $optional) {
    if (Test-Path $file) {
        Write-Host "  ✓ $file (optionnel)" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ $file non trouvé (optionnel)" -ForegroundColor Yellow
    }
}

# Créer l'image
Write-Host "`n[2/6] Création de l'image ($imgSizeMB MB)..." -ForegroundColor Yellow
wsl bash -c "dd if=/dev/zero of=$imgPath bs=1M count=$imgSizeMB 2>&1 | tail -3"

# Créer partition GPT
Write-Host "`n[3/6] Création de la partition GPT..." -ForegroundColor Yellow
wsl bash -c "sgdisk -Z $imgPath 2>&1 | grep -v 'Warning'"
wsl bash -c "sgdisk -n 1:2048:0 -t 1:EF00 -c 1:'EFI System' $imgPath 2>&1 | grep -v 'Warning'"

# Formater (sans sudo via mtools)
Write-Host "`n[4/6] Installation de mtools..." -ForegroundColor Yellow
wsl bash -c "sudo apt-get update >/dev/null 2>&1 && sudo apt-get install -y mtools >/dev/null 2>&1"

Write-Host "`n[5/6] Copie des fichiers vers l'image..." -ForegroundColor Yellow

# Extraire la partition
wsl bash -c @"
# Calculer l'offset (2048 secteurs * 512 bytes)
OFFSET=`$((2048 * 512))

# Formater avec mtools (sans sudo)
mformat -i $imgPath@@`$OFFSET -F -v LLMBOOT ::

# Créer structure EFI
mmd -i $imgPath@@`$OFFSET ::/EFI
mmd -i $imgPath@@`$OFFSET ::/EFI/BOOT

# Copier les fichiers
mcopy -i $imgPath@@`$OFFSET llama2.efi ::/EFI/BOOT/BOOTX64.EFI
mcopy -i $imgPath@@`$OFFSET iwlwifi-cc-a0-72.ucode ::/

# Copier modèle et tokenizer si disponibles
if [ -f ../llama2.c/stories15M.bin ]; then
    mcopy -i $imgPath@@`$OFFSET ../llama2.c/stories15M.bin ::/
fi
if [ -f ../llama2.c/tokenizer.bin ]; then
    mcopy -i $imgPath@@`$OFFSET ../llama2.c/tokenizer.bin ::/
fi

# Lister le contenu
echo -e '\n=== Contenu de l''image USB ==='
mdir -i $imgPath@@`$OFFSET ::/ 
mdir -i $imgPath@@`$OFFSET ::/EFI/BOOT
"@

# Vérification finale
Write-Host "`n[6/6] Vérification finale..." -ForegroundColor Yellow
$img = Get-Item $imgPath
Write-Host "  ✓ Image créée: $($img.Name)" -ForegroundColor Green
Write-Host "  ✓ Taille: $([math]::Round($img.Length/1MB, 2)) MB" -ForegroundColor Green
Write-Host "  ✓ Date: $($img.LastWriteTime)" -ForegroundColor Green

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║              ✓ IMAGE USB PRÊTE!                           ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "PROCHAINES ÉTAPES:" -ForegroundColor Yellow
Write-Host "  1. Insérer une clé USB (≥256 MB)" -ForegroundColor Cyan
Write-Host "  2. Ouvrir Rufus (https://rufus.ie)" -ForegroundColor Cyan
Write-Host "  3. Sélectionner la clé USB" -ForegroundColor Cyan
Write-Host "  4. Cliquer 'SELECT' → choisir $imgPath" -ForegroundColor Cyan
Write-Host "  5. Partition scheme: GPT" -ForegroundColor Cyan
Write-Host "  6. Target system: UEFI (non CSM)" -ForegroundColor Cyan
Write-Host "  7. Cliquer START" -ForegroundColor Cyan
Write-Host "  8. Booter sur la clé USB (désactiver Secure Boot si nécessaire)`n" -ForegroundColor Cyan

Write-Host "Ou tester dans QEMU:" -ForegroundColor Yellow
Write-Host "  qemu-system-x86_64 -bios OVMF.fd -drive format=raw,file=$imgPath -m 512M -serial stdio`n" -ForegroundColor Cyan
