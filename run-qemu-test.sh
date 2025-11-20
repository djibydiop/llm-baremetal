#!/bin/bash
# Test LLaMA2 EFI in QEMU with detailed output

cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal

echo "========================================="
echo "  LLaMA2 QEMU Test"
echo "========================================="
echo ""
echo "Model: stories15M.bin (15M params)"
echo "Binary: llama2.efi (18.8 MB)"
echo "Disk: llama2-disk.img (128 MB)"
echo ""
echo "Starting QEMU (will timeout after 45 seconds)..."
echo ""

# Run QEMU with output capture
timeout 45 qemu-system-x86_64 \
  -bios /usr/share/ovmf/OVMF.fd \
  -drive format=raw,file=llama2-disk.img \
  -m 512M \
  -nographic \
  -serial stdio \
  2>&1 | tee qemu-full-output.log

EXIT_CODE=$?

echo ""
echo "========================================="
echo "Test completed with exit code: $EXIT_CODE"
echo "========================================="

if [ -f qemu-full-output.log ]; then
    echo ""
    echo "Output saved to: qemu-full-output.log"
    echo "Output size: $(wc -l < qemu-full-output.log) lines"
    echo ""
    echo "=== First 30 lines ==="
    head -30 qemu-full-output.log
    echo ""
    echo "=== Last 30 lines ==="
    tail -30 qemu-full-output.log
fi
