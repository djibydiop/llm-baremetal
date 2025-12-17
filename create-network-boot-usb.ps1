# Image USB minimale pour boot réseau pur (2 MB au lieu de 512 MB)
Write-Host "=== Image USB Minimale - Network Boot Only ===" -ForegroundColor Cyan

$imageName = "llm-network-boot.img"
$imageSize = 2  # MB seulement

# Supprimer ancienne
if (Test-Path $imageName) { Remove-Item $imageName -Force }

Write-Host "Creation image 2MB..." -ForegroundColor Yellow
wsl dd if=/dev/zero of=$imageName bs=1M count=$imageSize status=none

Write-Host "Partition GPT..." -ForegroundColor Yellow
wsl sgdisk -Z $imageName 2>&1 | Out-Null
wsl sgdisk -n 1:2048:0 -t 1:EF00 $imageName 2>&1 | Out-Null

Write-Host "Format FAT32..." -ForegroundColor Yellow
wsl mformat -i $imageName@@1M -F -v NETBOOT ::

Write-Host "Structure EFI..." -ForegroundColor Yellow
wsl mmd -i $imageName@@1M ::/EFI
wsl mmd -i $imageName@@1M ::/EFI/BOOT

Write-Host "Copie binaire..." -ForegroundColor Yellow
wsl mcopy -i $imageName@@1M llama2.efi ::/EFI/BOOT/BOOTX64.EFI

Write-Host ""
Write-Host "=== IMAGE MINIMALE CREEE ===" -ForegroundColor Green
$size = (Get-Item $imageName).Length / 1MB
Write-Host "Taille: $($size.ToString('0.00')) MB" -ForegroundColor Cyan
Write-Host "Contenu: BOOTX64.EFI uniquement" -ForegroundColor Yellow
Write-Host "Boot: 100% réseau HTTP" -ForegroundColor Green
