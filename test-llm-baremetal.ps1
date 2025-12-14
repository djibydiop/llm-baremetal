# Test LLM Baremetal with stories15M model

Write-Host "`n=== YAMAOO LLM BAREMETAL TEST ===" -ForegroundColor Cyan
Write-Host "Model: stories15M.bin (58MB)" -ForegroundColor Green
Write-Host "Bootloader: llama2.efi (88KB)" -ForegroundColor Green
Write-Host "`nLaunching QEMU with UEFI...`n" -ForegroundColor Yellow

# Check files
Write-Host "[CHECK] Verifying files..." -ForegroundColor Cyan
if (Test-Path "qemu-test.img") { Write-Host "  OK: qemu-test.img" -ForegroundColor Green }
if (Test-Path "stories15M.bin") { Write-Host "  OK: stories15M.bin" -ForegroundColor Green }
if (Test-Path "tokenizer.bin") { Write-Host "  OK: tokenizer.bin" -ForegroundColor Green }
if (Test-Path "llama2.efi") { Write-Host "  OK: llama2.efi" -ForegroundColor Green }

Write-Host "`n[INFO] QEMU window will open..." -ForegroundColor Cyan
Write-Host "[INFO] The baremetal LLM will:" -ForegroundColor Cyan
Write-Host "  1. Load stories15M.bin model" -ForegroundColor White
Write-Host "  2. Show interactive prompt menu" -ForegroundColor White
Write-Host "  3. Generate text with LLM" -ForegroundColor White
Write-Host "`n[INFO] Press Ctrl+C to stop QEMU`n" -ForegroundColor Yellow

# Launch QEMU via WSL
wsl bash -c "qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=/mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal/qemu-test.img,format=raw -m 1024M -cpu qemu64 -smp 2 -display gtk 2>&1 | tee /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal/qemu-session.log"

Write-Host "`nQEMU session ended" -ForegroundColor Green
Write-Host "Log saved: qemu-session.log`n" -ForegroundColor Cyan
