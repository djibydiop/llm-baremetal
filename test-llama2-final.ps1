# Final LLaMA2 Test Script with proper debug output

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  LLaMA2 QEMU Test (Debug Mode)" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

Write-Host "Files:" -ForegroundColor Yellow
Write-Host "  - llama2.efi: $(if (Test-Path llama2.efi) {'✅'} else {'❌'})" -ForegroundColor Green
Write-Host "  - stories15M.bin: $(if (Test-Path stories15M.bin) {'✅'} else {'❌'})" -ForegroundColor Green  
Write-Host "  - llama2-disk.img: $(if (Test-Path llama2-disk.img) {'✅'} else {'❌'})" -ForegroundColor Green

Write-Host "`n⚠️  QEMU will open in a separate window" -ForegroundColor Yellow
Write-Host "📺 Watch the QEMU window for output:" -ForegroundColor Yellow
Write-Host "    - [DEBUG] messages show progress" -ForegroundColor Gray
Write-Host "    - [ERROR] messages show problems" -ForegroundColor Gray
Write-Host "    - Forward pass may take 10-30 seconds" -ForegroundColor Gray

Write-Host "`n🚀 Launching QEMU..." -ForegroundColor Green
Write-Host "(Press Ctrl+C here to stop)`n" -ForegroundColor Gray

# Launch QEMU - output goes to QEMU window
wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=llama2-disk.img -m 512M'

Write-Host "`n✅ QEMU exited" -ForegroundColor Green
