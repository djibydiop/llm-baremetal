#!/usr/bin/env pwsh
# Lancer QEMU Windows (plus rapide que WSL)
# Made in Senegal by Djiby Diop

Write-Host "`n=== Testing Bare-Metal Neural LLM (QEMU Windows) ===" -ForegroundColor Cyan
Write-Host "Lancement de QEMU natif Windows..." -ForegroundColor Yellow
Write-Host ""

# Chemins possibles pour QEMU Windows
$qemuPaths = @(
    "C:\Program Files\qemu\qemu-system-x86_64.exe",
    "C:\Program Files (x86)\qemu\qemu-system-x86_64.exe",
    "C:\qemu\qemu-system-x86_64.exe",
    "$env:ProgramFiles\qemu\qemu-system-x86_64.exe",
    "$env:LOCALAPPDATA\Programs\QEMU\qemu-system-x86_64.exe"
)

$qemuExe = $null
foreach ($path in $qemuPaths) {
    if (Test-Path $path) {
        $qemuExe = $path
        Write-Host "QEMU trouvé: $path" -ForegroundColor Green
        break
    }
}

if (-not $qemuExe) {
    Write-Host "QEMU Windows non trouvé!" -ForegroundColor Red
    Write-Host "Recherche dans PATH..." -ForegroundColor Yellow
    $qemuExe = (Get-Command qemu-system-x86_64.exe -ErrorAction SilentlyContinue).Source
}

if (-not $qemuExe) {
    Write-Host "`nQEMU Windows n'est pas installé!" -ForegroundColor Red
    Write-Host "Téléchargez depuis: https://qemu.weilnetz.de/w64/" -ForegroundColor Yellow
    Write-Host "`nUtilisation de QEMU WSL à la place..." -ForegroundColor Yellow
    wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=qemu-test.img,format=raw -m 512'
    exit
}

# Chemins pour OVMF (UEFI firmware)
$ovmfLocal = "C:\Users\djibi\Desktop\baremetal\llm-baremetal\OVMF.fd"
$ovmfPaths = @(
    $ovmfLocal,
    "C:\Program Files\qemu\share\edk2-x86_64-code.fd",
    "C:\qemu\share\edk2-x86_64-code.fd",
    "$env:ProgramFiles\qemu\share\edk2-x86_64-code.fd"
)

$ovmfBios = $null
foreach ($path in $ovmfPaths) {
    if (Test-Path $path) {
        $ovmfBios = $path
        Write-Host "OVMF trouvé: $path" -ForegroundColor Green
        break
    }
}

if (-not $ovmfBios) {
    Write-Host "OVMF BIOS non trouvé!" -ForegroundColor Red
    exit
}

# Lancer QEMU
$imgPath = "C:\Users\djibi\Desktop\baremetal\llm-baremetal\qemu-test.img"

Write-Host "Lancement QEMU avec $imgPath..." -ForegroundColor Cyan
& $qemuExe -bios $ovmfBios -drive "file=$imgPath,format=raw" -m 512

Write-Host "`nQEMU terminé." -ForegroundColor Green
