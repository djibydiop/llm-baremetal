# Script pour télécharger et installer QEMU Windows portable

Write-Host "=== INSTALLATION QEMU PORTABLE ===" -ForegroundColor Cyan
Write-Host ""

$qemuDir = "$PSScriptRoot\qemu-portable"
$qemuUrl = "https://qemu.weilnetz.de/w64/2024/qemu-w64-setup-20240930.exe"

if (Test-Path "$qemuDir\qemu-system-x86_64.exe") {
    Write-Host "✓ QEMU déjà installé dans $qemuDir" -ForegroundColor Green
} else {
    Write-Host "Téléchargement depuis: https://qemu.weilnetz.de/w64/" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "MANUEL:" -ForegroundColor Cyan
    Write-Host "1. Ouvrez https://qemu.weilnetz.de/w64/ dans votre navigateur" -ForegroundColor White
    Write-Host "2. Téléchargez la dernière version (exe ou zip)" -ForegroundColor White
    Write-Host "3. Installez dans C:\Program Files\qemu" -ForegroundColor White
    Write-Host "   OU extrayez dans: $qemuDir" -ForegroundColor White
    Write-Host ""
    Write-Host "Une fois installé, relancez: .\test-qemu.ps1" -ForegroundColor Green
    
    Start-Process "https://qemu.weilnetz.de/w64/"
}
