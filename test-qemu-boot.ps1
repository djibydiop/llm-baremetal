# Test QEMU UEFI Boot - llm-baremetal
# Teste l'image USB en mode UEFI avant boot hardware

Write-Host "=== Test QEMU UEFI Boot ===" -ForegroundColor Cyan
Write-Host ""

$qemuExe = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$usbImage = "llm-baremetal-usb.img"
$ovmfBios = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"

# Vérifications
if (-not (Test-Path $qemuExe)) {
    Write-Host "ERREUR: QEMU non installé" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $usbImage)) {
    Write-Host "ERREUR: $usbImage non trouvée" -ForegroundColor Red
    Write-Host "Exécutez: .\create-bootable-usb.ps1" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $ovmfBios)) {
    Write-Host "ERREUR: OVMF BIOS non trouvé" -ForegroundColor Red
    exit 1
}

Write-Host "✓ QEMU: $qemuExe" -ForegroundColor Green
Write-Host "✓ Image: $usbImage ($(((Get-Item $usbImage).Length / 1MB).ToString('0.00')) MB)" -ForegroundColor Green
Write-Host "✓ UEFI: $ovmfBios" -ForegroundColor Green
Write-Host ""

Write-Host "IMPORTANT:" -ForegroundColor Yellow
Write-Host "  - QEMU n'émule pas l'Intel AX200 WiFi" -ForegroundColor Yellow
Write-Host "  - Ce test vérifie UNIQUEMENT le boot UEFI" -ForegroundColor Yellow
Write-Host "  - Attendu: 'WiFi device not found' (NORMAL)" -ForegroundColor Yellow
Write-Host ""

Write-Host "Lancement de QEMU en mode UEFI..." -ForegroundColor Green
Write-Host "Appuyez sur Ctrl+Alt+Q pour quitter QEMU" -ForegroundColor Cyan
Write-Host ""
Start-Sleep -Seconds 2

# Lancer QEMU avec UEFI
& $qemuExe `
    -drive "if=pflash,format=raw,readonly=on,file=$ovmfBios" `
    -drive "file=$usbImage,format=raw,if=ide" `
    -m 512 `
    -cpu qemu64 `
    -smp 1 `
    -vga std `
    -boot order=c `
    -serial stdio

Write-Host ""
Write-Host "Test terminé!" -ForegroundColor Green
Write-Host ""
Write-Host "Si 'WiFi device not found' est apparu: SUCCÈS!" -ForegroundColor Green
Write-Host "L'image boote correctement en UEFI." -ForegroundColor Green
Write-Host ""
Write-Host "Prochaine étape: Boot sur hardware réel" -ForegroundColor Cyan
Write-Host "  1. Flasher USB avec balenaEtcher" -ForegroundColor Gray
Write-Host "  2. BIOS: Désactiver Secure Boot" -ForegroundColor Gray
Write-Host "  3. BIOS: Mode UEFI (pas Legacy)" -ForegroundColor Gray
Write-Host "  4. Boot sur USB (F12)" -ForegroundColor Gray
Write-Host ""
