#!/usr/bin/env powershell
# Test Network Streaming with QEMU

Write-Host "=== Network Streaming Test ===" -ForegroundColor Cyan
Write-Host ""

# Check if model server is running
Write-Host "Step 1: Checking model server..." -ForegroundColor Yellow
$serverRunning = Test-NetConnection -ComputerName localhost -Port 8080 -InformationLevel Quiet -WarningAction SilentlyContinue

if (!$serverRunning) {
    Write-Host "⚠ Model server not running!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Start server in another terminal:" -ForegroundColor White
    Write-Host "  python serve_model.py" -ForegroundColor Green
    Write-Host ""
    Write-Host "Then run this test again." -ForegroundColor White
    exit 1
}

Write-Host "✓ Model server running on port 8080" -ForegroundColor Green
Write-Host ""

# Check QEMU
Write-Host "Step 2: Checking QEMU..." -ForegroundColor Yellow
$qemuPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"

if (!(Test-Path $qemuPath)) {
    Write-Host "✗ QEMU not found at: $qemuPath" -ForegroundColor Red
    exit 1
}

Write-Host "✓ QEMU found" -ForegroundColor Green
Write-Host ""

# Check test image
Write-Host "Step 3: Checking test image..." -ForegroundColor Yellow
if (!(Test-Path "qemu-test.img")) {
    Write-Host "✗ qemu-test.img not found" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Test image ready" -ForegroundColor Green
Write-Host ""

# Start QEMU with network
Write-Host "Step 4: Starting QEMU with network..." -ForegroundColor Yellow
Write-Host "Network mode: user (host accessible as 10.0.2.2)" -ForegroundColor Gray
Write-Host ""

$qemuArgs = @(
    "-bios", "OVMF.fd",
    "-drive", "format=raw,file=qemu-test.img",
    "-m", "1024M",
    "-net", "nic,model=e1000",
    "-net", "user,hostfwd=tcp::8080-:8080",
    "-nographic"
)

Write-Host "Starting QEMU..." -ForegroundColor Cyan
Write-Host "Model URL: http://10.0.2.2:8080/stories110M.bin" -ForegroundColor White
Write-Host ""
Write-Host "Test will run for 120 seconds..." -ForegroundColor Gray
Write-Host "Press Ctrl+C to stop early" -ForegroundColor Gray
Write-Host ""

$proc = Start-Process -FilePath $qemuPath -ArgumentList $qemuArgs -PassThru -NoNewWindow

Write-Host "QEMU PID: $($proc.Id)" -ForegroundColor White
Start-Sleep -Seconds 120

if (!$proc.HasExited) {
    Write-Host ""
    Write-Host "Stopping QEMU..." -ForegroundColor Yellow
    Stop-Process -Id $proc.Id -Force
}

Write-Host ""
Write-Host "=== Test Complete ===" -ForegroundColor Cyan
