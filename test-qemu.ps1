# Test QEMU Boot
Write-Host "Test QEMU UEFI Boot" -ForegroundColor Cyan

$qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$img = "llm-baremetal-usb.img"
$uefi = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"

if (-not (Test-Path $qemu)) { Write-Host "QEMU non trouve"; exit 1 }
if (-not (Test-Path $img)) { Write-Host "Image non trouvee"; exit 1 }
if (-not (Test-Path $uefi)) { Write-Host "UEFI non trouve"; exit 1 }

Write-Host "Demarrage QEMU..." -ForegroundColor Green
Write-Host "NOTE: WiFi ne fonctionnera pas (normal, pas d'Intel AX200 dans QEMU)" -ForegroundColor Yellow
Write-Host ""

$uefiFull = $uefi -replace '\\', '/'
$imgFull = $img -replace '\\', '/'

& $qemu `
    -drive "if=pflash,format=raw,readonly=on,file=$uefi" `
    -drive "file=$img,format=raw,if=ide" `
    -m 512 `
    -cpu qemu64 `
    -smp 1 `
    -vga std `
    -boot order=c `
    -serial stdio

Write-Host ""
Write-Host "Test termine" -ForegroundColor Green
