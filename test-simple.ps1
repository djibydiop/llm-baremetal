# Test with different QEMU configurations

Write-Host "Testing llm-baremetal boot..." -ForegroundColor Cyan
Write-Host ""

$imagePath = "/mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal/llm-disk.img"

Write-Host "[Test 1] OVMF UEFI boot..." -ForegroundColor Yellow
wsl bash -c "timeout 10 qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=$imagePath -m 512M -serial mon:stdio -nographic 2>&1 | head -50"

Write-Host ""
Write-Host "[Test 2] Check disk structure..." -ForegroundColor Yellow
wsl bash -c "mdir -i $imagePath ::/"
wsl bash -c "mdir -i $imagePath ::/EFI"
wsl bash -c "mdir -i $imagePath ::/EFI/BOOT"
