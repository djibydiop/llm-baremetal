#!/bin/bash
# Boot direct depuis la clé USB physique dans QEMU

echo "🚀 LlamaUltimate v7.0 - Boot USB dans QEMU"
echo ""

# Détection OVMF
OVMF_CODE=""
if [ -f /usr/share/OVMF/OVMF_CODE.fd ]; then
    OVMF_CODE="/usr/share/OVMF/OVMF_CODE.fd"
elif [ -f /usr/share/OVMF/OVMF_CODE_4M.fd ]; then
    OVMF_CODE="/usr/share/OVMF/OVMF_CODE_4M.fd"
else
    echo "❌ OVMF introuvable!"
    exit 1
fi

echo "📦 OVMF: $OVMF_CODE"
echo "💾 USB: /dev/sdb (ou similaire)"
echo ""

# Option 1: Boot depuis l'image montée (plus fiable)
USB_DEVICE="/mnt/d"
if [ -d "$USB_DEVICE" ]; then
    echo "✅ USB accessible via $USB_DEVICE"
    echo "🚀 Lancement QEMU (30s timeout)..."
    echo ""
    
    timeout 30s qemu-system-x86_64 \
        -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
        -drive file=fat:rw:/mnt/d,format=raw \
        -m 1024M \
        -cpu qemu64 \
        -smp 2 \
        -nographic \
        -serial mon:stdio 2>&1 | tee boot-usb.log
else
    echo "❌ USB non accessible"
    echo "Tentative avec device physique..."
    
    # Option 2: Accès direct au device (nécessite root)
    USB_DEV="/dev/sdb"
    if [ -b "$USB_DEV" ]; then
        echo "✅ Device USB trouvé: $USB_DEV"
        echo "⚠️  Nécessite sudo pour accès direct"
        
        sudo qemu-system-x86_64 \
            -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
            -drive file="$USB_DEV",format=raw \
            -m 1024M \
            -cpu qemu64 \
            -smp 2 \
            -nographic \
            -serial mon:stdio 2>&1 | tee boot-usb.log
    else
        echo "❌ Device USB non trouvé"
        echo "Utilisez: lsblk pour voir les devices"
        exit 1
    fi
fi

echo ""
echo "📊 Résultat:"
[ -f boot-usb.log ] && tail -30 boot-usb.log
echo ""
echo "💾 Log: $(pwd)/boot-usb.log"
