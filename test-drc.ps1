# Test DRC/URS in QEMU
wsl bash -c @"
cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal
timeout 15 qemu-system-x86_64 \
  -drive format=raw,file=qemu-test.img \
  -m 256M \
  -cpu qemu64 \
  -bios /usr/share/ovmf/OVMF.fd \
  -nographic 2>&1 | tee qemu-output.txt
"@

Write-Host "`n=== DRC/URS Output ===" -ForegroundColor Cyan
wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && cat qemu-output.txt | grep -A 20 "DRC"'
