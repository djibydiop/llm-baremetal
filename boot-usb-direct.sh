#!/bin/bash
# Boot QEMU depuis image de la clé USB

echo "🚀 LlamaUltimate v7.0 - Boot depuis image USB"
echo ""

WORK_DIR="/mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal/qemu-usb-boot"
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

echo "📦 Copie du contenu de la clé USB..."
rm -rf esp
mkdir -p esp

# Copier depuis le montage Windows de la clé D:\
if [ -f /mnt/d/EFI/BOOT/BOOTX64.EFI ]; then
    echo "✅ Clé USB accessible via /mnt/d"
    cp -r /mnt/d/* esp/ 2>/dev/null || true
    cp -r /mnt/d/EFI esp/ 2>/dev/null || true
else
    echo "⚠️  Clé non accessible, copie depuis workspace..."
    mkdir -p esp/EFI/BOOT
    cp /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal/llama2.efi esp/EFI/BOOT/BOOTX64.EFI
    cp /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal/stories110M.bin esp/
fi

echo "📊 Contenu de l'image ESP:"
ls -lh esp/
[ -d esp/EFI ] && ls -lh esp/EFI/BOOT/

# Trouver OVMF
OVMF_CODE=""
if [ -f /usr/share/OVMF/OVMF_CODE.fd ]; then
    OVMF_CODE="/usr/share/OVMF/OVMF_CODE.fd"
elif [ -f /usr/share/OVMF/OVMF_CODE_4M.fd ]; then
    OVMF_CODE="/usr/share/OVMF/OVMF_CODE_4M.fd"
else
    echo "❌ OVMF introuvable!"
    exit 1
fi

echo ""
echo "🚀 Lancement QEMU (40s)..."
echo "   OVMF: $OVMF_CODE"
echo "   Image: fat:rw:esp"
echo ""

timeout 40s qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive file=fat:rw:esp,format=raw \
    -m 1024M \
    -cpu qemu64 \
    -smp 2 \
    -nographic \
    -serial mon:stdio 2>&1 | tee qemu-usb-boot.log

EXIT_CODE=$?

echo ""
echo "=========================================="
if [ $EXIT_CODE -eq 124 ]; then
    echo "✅ Timeout atteint (40s)"
elif [ $EXIT_CODE -eq 0 ]; then
    echo "✅ Terminé"
else
    echo "⚠️  Code: $EXIT_CODE"
fi

echo ""
echo "📄 Dernières lignes:"
[ -f qemu-usb-boot.log ] && tail -40 qemu-usb-boot.log

echo ""
echo "💾 Log complet: $WORK_DIR/qemu-usb-boot.log"
