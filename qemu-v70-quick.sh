#!/bin/bash
# Test QEMU rapide pour v7.0

echo "🚀 LlamaUltimate v7.0 UNIFIED - Test QEMU rapide"
echo ""

# Installation si nécessaire
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "Installation QEMU..."
    sudo apt-get update && sudo apt-get install -y qemu-system-x86 ovmf
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEST_DIR="$SCRIPT_DIR/qemu-v70-quick"

mkdir -p "$TEST_DIR/esp/EFI/BOOT"
cd "$TEST_DIR"

cp "$SCRIPT_DIR/llama2.efi" esp/EFI/BOOT/BOOTX64.EFI
cp "$SCRIPT_DIR/stories110M.bin" esp/

echo "✅ Fichiers prêts"
echo "🚀 Démarrage QEMU (30s timeout)..."
echo ""

# Trouver le bon fichier OVMF
OVMF_CODE=""
if [ -f /usr/share/OVMF/OVMF_CODE.fd ]; then
    OVMF_CODE="/usr/share/OVMF/OVMF_CODE.fd"
elif [ -f /usr/share/OVMF/OVMF_CODE_4M.fd ]; then
    OVMF_CODE="/usr/share/OVMF/OVMF_CODE_4M.fd"
elif [ -f /usr/share/qemu/OVMF_CODE.fd ]; then
    OVMF_CODE="/usr/share/qemu/OVMF_CODE.fd"
else
    echo "❌ OVMF introuvable! Installez: sudo apt install ovmf"
    exit 1
fi

echo "📦 Utilisation OVMF: $OVMF_CODE"
echo ""

timeout 30s qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive file=fat:rw:esp,format=raw \
    -m 512M \
    -nographic \
    -serial mon:stdio 2>&1 | tee log.txt

echo ""
echo "📊 Résultats:"
[ -f log.txt ] && tail -30 log.txt
echo ""
echo "💾 Log: $TEST_DIR/log.txt"
