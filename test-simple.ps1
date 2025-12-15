#!/usr/bin/env powershell
# Simple QEMU test - direct output

Write-Host "=== DRC v6.0 Direct Test ==="
Write-Host "Binary: llama2.efi (682KB optimized)"
Write-Host ""

# Start QEMU in background
$qemuPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$args = @(
    "-bios", "OVMF.fd",
    "-drive", "format=raw,file=qemu-test.img",
    "-m", "1024M",
    "-nographic"
)

Write-Host "Starting QEMU..."
$proc = Start-Process -FilePath $qemuPath -ArgumentList $args -PassThru -NoNewWindow

Write-Host "QEMU PID: $($proc.Id)"
Write-Host "Waiting 90 seconds for test..."
Write-Host ""

Start-Sleep -Seconds 90

if (!$proc.HasExited) {
    Write-Host "Stopping QEMU..."
    Stop-Process -Id $proc.Id -Force
} else {
    Write-Host "QEMU exited with code: $($proc.ExitCode)"
}

Write-Host ""
Write-Host "=== Test Complete ==="
