# Test llama2.efi in QEMU with output capture
# This script launches QEMU and captures the EFI output

Write-Host "🚀 Testing LLaMA2 (stories15M) in QEMU..." -ForegroundColor Cyan
Write-Host "Press Ctrl+C to stop QEMU" -ForegroundColor Yellow
Write-Host ""

# Build disk if needed
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && make llama2-disk"

Write-Host ""
Write-Host "Starting QEMU..." -ForegroundColor Green

# Launch QEMU with serial output
wsl bash -c @"
cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal
qemu-system-x86_64 \
  -bios /usr/share/ovmf/OVMF.fd \
  -drive format=raw,file=llama2-disk.img \
  -m 512M \
  -nographic \
  -serial mon:stdio
"@
