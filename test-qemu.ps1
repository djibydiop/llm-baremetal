# Script de test QEMU rapide
# Téléchargez QEMU pour Windows depuis: https://qemu.weilnetz.de/w64/

Write-Host "=== TEST QEMU ===" -ForegroundColor Cyan
Write-Host ""

# Vérifier si QEMU est installé
if (Test-Path "C:\Program Files\qemu\qemu-system-x86_64.exe") {
    $qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
} elseif (Get-Command qemu-system-x86_64.exe -ErrorAction SilentlyContinue) {
    $qemu = "qemu-system-x86_64.exe"
} else {
    Write-Host "QEMU Windows non trouvé !" -ForegroundColor Red
    Write-Host ""
    Write-Host "Téléchargez depuis: https://qemu.weilnetz.de/w64/" -ForegroundColor Yellow
    Write-Host "Ou installez avec: choco install qemu" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "En attendant, utilisez WSL QEMU avec VNC:" -ForegroundColor Cyan
    Write-Host "  wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/baremetal/qemu-mini && qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -hda fat:rw:. -m 2048M -cpu qemu64,+sse2 -smp 2 -vnc :0'" -ForegroundColor White
    Write-Host "  Puis connectez-vous avec un client VNC sur localhost:5900" -ForegroundColor White
    exit 1
}

Write-Host "Lancement de QEMU..." -ForegroundColor Green
Write-Host "OVMF: Firmware UEFI" -ForegroundColor Gray
Write-Host "Mémoire: 2048 MB" -ForegroundColor Gray
Write-Host "CPU: qemu64 +sse2" -ForegroundColor Gray
Write-Host ""

# Chemin vers OVMF (téléchargez depuis EDK2 si nécessaire)
$ovmf = "$PSScriptRoot\OVMF.fd"
if (-not (Test-Path $ovmf)) {
    Write-Host "OVMF.fd non trouvé, utilisation de WSL OVMF via chemin réseau..." -ForegroundColor Yellow
    # Utiliser le fichier OVMF de WSL
    $ovmf = "\\wsl.localhost\Ubuntu\usr\share\ovmf\OVMF.fd"
}

# Lancer QEMU
Set-Location "$PSScriptRoot\..\qemu-mini"
& $qemu -bios $ovmf -hda fat:rw:. -m 2048M -cpu qemu64,+sse2 -smp 2
