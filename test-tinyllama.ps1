#!/usr/bin/env powershell
# TinyLlama 1.1B Test - DRC v6.0

Write-Host "=== DRC v6.0 - TinyLlama 1.1B Test ==="
Write-Host "Binary: llama2.efi (682KB optimized)"
Write-Host "Model: tinyllama-1.1b-chat.bin (1.1GB)"
Write-Host ""

# QEMU settings - need more RAM for 1.1B model
$qemuPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$args = @(
    "-bios", "OVMF.fd",
    "-drive", "format=raw,file=qemu-tinyllama.img",
    "-m", "4096M",  # 4GB RAM for 1.1GB model
    "-nographic"
)

Write-Host "Starting QEMU with 4GB RAM..."
$proc = Start-Process -FilePath $qemuPath -ArgumentList $args -PassThru -NoNewWindow

Write-Host "QEMU PID: $($proc.Id)"
Write-Host "Waiting 120 seconds for test (large model)..."
Write-Host ""

Start-Sleep -Seconds 120

if (!$proc.HasExited) {
    Write-Host "Stopping QEMU..."
    Stop-Process -Id $proc.Id -Force
} else {
    Write-Host "QEMU exited with code: $($proc.ExitCode)"
}

Write-Host ""
Write-Host "=== Test Complete ==="
