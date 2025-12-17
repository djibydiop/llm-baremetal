# Script pour créer une image USB bootable UEFI avec llama2.efi et firmware WiFi
# Résout le problème: partition FAT32 correctement formatée avec fichiers EFI

Write-Host "=== Création d'une image USB bootable UEFI ===" -ForegroundColor Cyan

# Paramètres
$imageName = "llm-baremetal-usb.img"
$imageSize = 128 # MB
$label = "LLMBOOT"

# Fichiers à copier
$efiFile = "llama2.efi"
$fwFile = "iwlwifi-cc-a0-72.ucode"

# Vérifier que les fichiers existent
if (-not (Test-Path $efiFile)) {
    Write-Host "ERREUR: $efiFile non trouvé!" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $fwFile)) {
    Write-Host "ERREUR: $fwFile non trouvé!" -ForegroundColor Red
    exit 1
}

Write-Host "Fichiers trouvés:"
Write-Host "  - $efiFile ($(((Get-Item $efiFile).Length / 1KB).ToString('0.00')) KB)"
Write-Host "  - $fwFile ($(((Get-Item $fwFile).Length / 1MB).ToString('0.00')) MB)"
Write-Host ""

# Supprimer l'ancienne image si elle existe
if (Test-Path $imageName) {
    Write-Host "Suppression de l'ancienne image..." -ForegroundColor Yellow
    Remove-Item $imageName -Force
}

Write-Host "Étape 1: Création de l'image vide (${imageSize} MB)..." -ForegroundColor Green
wsl bash -c "dd if=/dev/zero of=/mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal/$imageName bs=1M count=$imageSize status=progress"

Write-Host ""
Write-Host "Étape 2: Création de la table de partition GPT..." -ForegroundColor Green
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && sgdisk -Z $imageName"
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && sgdisk -n 1:2048:0 -t 1:EF00 -c 1:'EFI System' $imageName"

Write-Host ""
Write-Host "Étape 3: Formatage FAT32 de la partition EFI..." -ForegroundColor Green
# Offset: 2048 secteurs * 512 bytes = 1048576 bytes
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mformat -i $imageName@@1M -F -v $label ::"

Write-Host ""
Write-Host "Étape 4: Création de la structure EFI..." -ForegroundColor Green
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mmd -i $imageName@@1M ::/EFI"
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mmd -i $imageName@@1M ::/EFI/BOOT"

Write-Host ""
Write-Host "Étape 5: Copie de llama2.efi vers EFI/BOOT/BOOTX64.EFI..." -ForegroundColor Green
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mcopy -i $imageName@@1M $efiFile ::/EFI/BOOT/BOOTX64.EFI"

Write-Host ""
Write-Host "Étape 6: Copie du firmware WiFi..." -ForegroundColor Green
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mcopy -i $imageName@@1M $fwFile ::/"

Write-Host ""
Write-Host "Étape 7: Vérification du contenu..." -ForegroundColor Green
Write-Host "Contenu de la partition EFI:" -ForegroundColor Cyan
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mdir -i $imageName@@1M ::/ -/"

Write-Host ""
Write-Host "=== Image USB créée avec succès! ===" -ForegroundColor Green
Write-Host ""
Write-Host "Fichier: $imageName ($(((Get-Item $imageName).Length / 1MB).ToString('0.00')) MB)" -ForegroundColor Cyan
Write-Host ""
Write-Host "Pour flasher sur USB:" -ForegroundColor Yellow
Write-Host "  1. balenaEtcher: https://etcher.balena.io/"
Write-Host "     - Flash from file -> $imageName -> Select target -> Flash!"
Write-Host ""
Write-Host "  2. dd (WSL/Linux):"
Write-Host "     wsl sudo dd if=$imageName of=/dev/sdX bs=4M status=progress && sudo sync"
Write-Host "     (remplacer /dev/sdX par votre clé USB, ex: /dev/sdb)"
Write-Host ""
Write-Host "  3. Rufus (Windows):"
Write-Host "     - Mode: DD Image"
Write-Host "     - Partition scheme: GPT"
Write-Host "     - Target: UEFI"
Write-Host ""
Write-Host "IMPORTANT pour le boot:" -ForegroundColor Red
Write-Host "  - Désactiver Secure Boot dans le BIOS"
Write-Host "  - Boot mode: UEFI (pas Legacy/CSM)"
Write-Host "  - Boot order: USB en premier ou utiliser F12"
Write-Host ""
