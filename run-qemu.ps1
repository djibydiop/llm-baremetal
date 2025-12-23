#!/usr/bin/env pwsh
# Launch QEMU for LLM-Baremetal testing

Write-Host "🚀 Launching QEMU with LLM-Baremetal image..." -ForegroundColor Cyan

$qemuPath = "qemu-system-x86_64"
$imagePath = "llama2_BOOTABLE.img"
$biosPath = "OVMF.fd"

# Check if files exist
if (-not (Test-Path $imagePath)) {
    Write-Host "❌ Error: $imagePath not found" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $biosPath)) {
    Write-Host "❌ Error: $biosPath not found" -ForegroundColor Red
    exit 1
}

# Try to run QEMU with WSL
Write-Host "📡 Starting QEMU (press Ctrl+A then X to exit)..." -ForegroundColor Yellow

wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && qemu-system-x86_64 -bios OVMF.fd -drive format=raw,file=llama2_BOOTABLE.img -m 512M -serial mon:stdio -nographic -no-reboot"
