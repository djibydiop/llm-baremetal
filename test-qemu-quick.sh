#!/bin/bash
# Test rapide QEMU

cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal/qemu-test || exit 1

echo "=== QEMU QUICK TEST ==="
echo "Fichiers:"
ls -lh *.efi *.bin 2>/dev/null | awk '{print $9, $5}'

echo ""
echo "Lancement QEMU (10 secondes)..."
echo "Le menu devrait s'afficher..."

# Lance QEMU avec timeout
timeout 10 qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=fat:rw:.,format=raw \
    -m 2048 \
    -serial mon:stdio \
    -nographic \
    2>&1 | head -50

echo ""
echo "✓ Test terminé!"
