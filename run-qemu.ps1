# Run llm-baremetal in QEMU

Write-Host "Launching Conscious Process in QEMU..." -ForegroundColor Cyan
Write-Host ""

$imagePath = "$PSScriptRoot\llm-disk.img"

if (-not (Test-Path $imagePath)) {
    Write-Host "ERROR: Disk image not found!" -ForegroundColor Red
    Write-Host "Run .\build-windows.ps1 first" -ForegroundColor Yellow
    exit 1
}

$wslPath = wsl wslpath "'$imagePath'"

Write-Host "Starting QEMU..." -ForegroundColor Yellow
Write-Host "Image: $imagePath" -ForegroundColor Gray
Write-Host ""

wsl bash -c "qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive format=raw,file='$wslPath' \
    -m 512M \
    -serial mon:stdio \
    -display gtk"
