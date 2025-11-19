# Build script for Windows using WSL

Write-Host "Building llm-baremetal EFI application..." -ForegroundColor Cyan
Write-Host ""

# Check if WSL is available
try {
    wsl --list --quiet | Out-Null
} catch {
    Write-Host "ERROR: WSL not found. Please install WSL first." -ForegroundColor Red
    exit 1
}

Write-Host "[1/3] Installing EFI development tools in WSL..." -ForegroundColor Yellow
wsl bash -c "sudo apt-get update && sudo apt-get install -y gnu-efi mtools ovmf 2>&1 | grep -E '(Installing|Unpacking|Setting up|already)'"

Write-Host ""
Write-Host "[2/3] Compiling EFI application..." -ForegroundColor Yellow

$wslPath = (wsl wslpath "'$PSScriptRoot'")
wsl bash -c "cd '$wslPath' && make clean && make"

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "[3/3] Creating bootable disk image..." -ForegroundColor Yellow
wsl bash -c "cd '$wslPath' && make disk"

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Disk creation failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host "  ✓ Build complete!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
Write-Host "To run in QEMU:" -ForegroundColor Cyan
Write-Host "  .\run-qemu.ps1" -ForegroundColor White
Write-Host ""
Write-Host "Files created:" -ForegroundColor Cyan
Write-Host "  - llm.efi (EFI application)" -ForegroundColor White
Write-Host "  - llm-disk.img (Bootable disk)" -ForegroundColor White
Write-Host ""
