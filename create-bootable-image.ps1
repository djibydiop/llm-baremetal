# Script pour créer une image bootable llama2_efi.img
# Cette image contient : BOOTX64.EFI + stories15M.bin + stories110M.bin + tokenizer.bin

$ErrorActionPreference = "Stop"

Write-Host "=== Création de l'image bootable llama2_efi.img ===" -ForegroundColor Cyan

# Paramètres
$ImageName = "llama2_efi.img"
$ImageSize = 550MB  # Assez pour stories110M (419MB) + stories15M (58MB) + marge
$QemuDir = "..\qemu-mini"

# Vérifier que les fichiers nécessaires existent
if (-not (Test-Path "llama2.efi")) {
    Write-Host "ERREUR: llama2.efi introuvable. Compilez d'abord avec 'wsl make'" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path "$QemuDir\stories15M.bin")) {
    Write-Host "ERREUR: stories15M.bin introuvable dans $QemuDir" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path "$QemuDir\tokenizer.bin")) {
    Write-Host "ERREUR: tokenizer.bin introuvable dans $QemuDir" -ForegroundColor Red
    exit 1
}

Write-Host "Étape 1: Création de l'image disque vide (550 MB)..." -ForegroundColor Yellow
wsl bash -c "dd if=/dev/zero of=llama2_efi.img bs=1M count=550 status=progress 2>&1"

Write-Host "Étape 2: Formatage FAT32..." -ForegroundColor Yellow
wsl bash -c "mkfs.vfat -F 32 -n LLAMA2EFI llama2_efi.img"

Write-Host "Étape 3: Montage de l'image..." -ForegroundColor Yellow
wsl bash -c "mkdir -p /tmp/llama2_mount"
wsl bash -c "sudo umount /tmp/llama2_mount 2>/dev/null || true"
wsl bash -c "sudo mount -o loop llama2_efi.img /tmp/llama2_mount"

Write-Host "Étape 4: Création de la structure EFI..." -ForegroundColor Yellow
wsl bash -c "sudo mkdir -p /tmp/llama2_mount/EFI/BOOT"

Write-Host "Étape 5: Copie de BOOTX64.EFI..." -ForegroundColor Yellow
wsl bash -c "sudo cp llama2.efi /tmp/llama2_mount/EFI/BOOT/BOOTX64.EFI"

Write-Host "Étape 6: Copie de stories15M.bin..." -ForegroundColor Yellow
wsl bash -c "sudo cp ../qemu-mini/stories15M.bin /tmp/llama2_mount/"

Write-Host "Étape 7: Copie de tokenizer.bin..." -ForegroundColor Yellow
wsl bash -c "sudo cp ../qemu-mini/tokenizer.bin /tmp/llama2_mount/"

# Copier stories110M.bin si disponible
if (Test-Path "$QemuDir\stories110M.bin") {
    Write-Host "Étape 8: Copie de stories110M.bin (419 MB)..." -ForegroundColor Yellow
    wsl bash -c "sudo cp ../qemu-mini/stories110M.bin /tmp/llama2_mount/"
} else {
    Write-Host "Note: stories110M.bin non trouvé, sauté" -ForegroundColor Gray
}

Write-Host "Étape 9: Vérification du contenu..." -ForegroundColor Yellow
wsl bash -c "sudo ls -lh /tmp/llama2_mount/"
wsl bash -c "sudo ls -lh /tmp/llama2_mount/EFI/BOOT/"

Write-Host "Étape 10: Synchronisation et démontage..." -ForegroundColor Yellow
wsl bash -c "sudo sync"
wsl bash -c "sudo umount /tmp/llama2_mount"

Write-Host ""
Write-Host "=== IMAGE BOOTABLE CRÉÉE AVEC SUCCÈS ===" -ForegroundColor Green
Write-Host ""
Write-Host "Fichier: llama2_efi.img (550 MB)" -ForegroundColor Green
Write-Host "Emplacement: $(Get-Location)\llama2_efi.img" -ForegroundColor Green
Write-Host ""
Write-Host "Pour déployer sur USB (remplacez X: par votre clé USB):" -ForegroundColor Cyan
Write-Host "  Windows (Admin): dd if=llama2_efi.img of=\\.\PhysicalDriveX bs=4M" -ForegroundColor White
Write-Host "  Linux:           sudo dd if=llama2_efi.img of=/dev/sdX bs=4M status=progress" -ForegroundColor White
Write-Host ""
Write-Host "ATTENTION: Vérifiez bien le numéro du disque USB avant de lancer dd !" -ForegroundColor Red
