#!/usr/bin/env pwsh
# Quick test script for LLaMA2 bare-metal

Write-Host "`n==================================" -ForegroundColor Cyan
Write-Host "  LLaMA2 Quick Test" -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan

# Check files
Write-Host "`n📋 Checking files..." -ForegroundColor Yellow
$files = @(
    @{Path="llama2-disk.img"; MinSize=100MB},
    @{Path="llama2.efi"; MinSize=10MB}
)

foreach ($file in $files) {
    if (Test-Path $file.Path) {
        $size = (Get-Item $file.Path).Length
        $sizeMB = [math]::Round($size / 1MB, 2)
        if ($size -ge $file.MinSize) {
            Write-Host "  ✅ $($file.Path): $sizeMB MB" -ForegroundColor Green
        } else {
            Write-Host "  ⚠️  $($file.Path): $sizeMB MB (too small)" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  ❌ $($file.Path): Not found" -ForegroundColor Red
    }
}

# Check disk contents
Write-Host "`n📦 Checking disk contents..." -ForegroundColor Yellow
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && mdir -i llama2-disk.img ::/ 2>&1"

Write-Host "`n🚀 Launching QEMU..." -ForegroundColor Cyan
Write-Host "   (Window will open - check for output there)" -ForegroundColor Yellow
Write-Host "`n⌛ Running for 15 seconds...`n" -ForegroundColor Yellow

# Launch QEMU with timeout
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && timeout 15 qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=llama2-disk.img -m 512M 2>&1"

Write-Host "`n✅ Test complete!" -ForegroundColor Green
Write-Host "   Did you see output in the QEMU window?" -ForegroundColor Yellow
