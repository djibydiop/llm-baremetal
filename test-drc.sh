#!/bin/bash
cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal

echo "=== Testing DRC/URS in QEMU ==="
timeout 15 qemu-system-x86_64 \
  -drive format=raw,file=qemu-test.img \
  -m 256M \
  -cpu qemu64 \
  -bios /usr/share/ovmf/OVMF.fd \
  -nographic 2>&1 | tee drc-test.log | head -200

echo ""
echo "=== Test Complete ==="
echo "Log saved to drc-test.log"
cat drc-test.log | grep -A 30 "DRC"
