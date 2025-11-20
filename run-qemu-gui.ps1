# Test LLaMA2 in QEMU with graphical window (from Windows)
# This will launch QEMU with a visible window to see EFI output

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "  LLaMA2 QEMU Test (Graphical Mode)" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Model: stories15M.bin (15M params)" -ForegroundColor Green
Write-Host "Binary: llama2.efi (18.8 MB)" -ForegroundColor Green
Write-Host "Disk: llama2-disk.img (128 MB)" -ForegroundColor Green
Write-Host ""
Write-Host "⚠️  QEMU will open in a separate window" -ForegroundColor Yellow
Write-Host "📝 Watch the QEMU window for EFI output" -ForegroundColor Yellow
Write-Host "🛑 Press Ctrl+C in this window to stop QEMU" -ForegroundColor Yellow
Write-Host ""

$qemuCmd = @"
cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal
qemu-system-x86_64 \
  -bios /usr/share/ovmf/OVMF.fd \
  -drive format=raw,file=llama2-disk.img \
  -m 512M \
  -serial mon:stdio
"@

Write-Host "Starting QEMU..." -ForegroundColor Green
Write-Host ""

# Launch QEMU (it will show output in its own window)
wsl bash -c $qemuCmd
