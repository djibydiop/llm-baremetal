#!/bin/bash
# Test QEMU pour LlamaUltimate v7.0 UNIFIED
# Optimisé pour exécution rapide

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEST_DIR="$SCRIPT_DIR/qemu-test-v70"

echo "=========================================="
echo "  LlamaUltimate v7.0 UNIFIED - QEMU Test"
echo "=========================================="

# Créer répertoire de test
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

# Copier les fichiers nécessaires
echo "📦 Préparation des fichiers..."
cp "$SCRIPT_DIR/llama2.efi" ./BOOTX64.EFI
cp "$SCRIPT_DIR/../llama2.c/stories110M.bin" ./stories110M.bin 2>/dev/null || {
    echo "⚠️  stories110M.bin non trouvé dans llama2.c/"
    echo "   Utilisation du fichier du répertoire courant"
    cp "$SCRIPT_DIR/stories110M.bin" ./stories110M.bin 2>/dev/null || {
        echo "❌ stories110M.bin non trouvé!"
        exit 1
    }
}

# Copier tokenizer
cp "$SCRIPT_DIR/../llama2.c/tokenizer.bin" ./tokenizer.bin 2>/dev/null || {
    cp "$SCRIPT_DIR/tokenizer.bin" ./tokenizer.bin 2>/dev/null || {
        echo "⚠️  tokenizer.bin non trouvé, mais peut continuer sans"
    }
}

# Créer image disque EFI
echo "💾 Création de l'image disque EFI..."
dd if=/dev/zero of=fat.img bs=1M count=450 2>/dev/null
mkfs.vfat fat.img >/dev/null 2>&1

# Monter et copier fichiers
mkdir -p mnt
sudo mount -o loop fat.img mnt
sudo mkdir -p mnt/EFI/BOOT
sudo cp BOOTX64.EFI mnt/EFI/BOOT/
sudo cp stories110M.bin mnt/
[ -f tokenizer.bin ] && sudo cp tokenizer.bin mnt/
sudo umount mnt
rmdir mnt

# Vérifier OVMF
OVMF_CODE="/usr/share/OVMF/OVMF_CODE.fd"
OVMF_VARS="/usr/share/OVMF/OVMF_VARS.fd"

if [ ! -f "$OVMF_CODE" ]; then
    echo "❌ OVMF non trouvé. Installation..."
    sudo apt-get update && sudo apt-get install -y ovmf
fi

# Créer copie de VARS (modifiable)
cp "$OVMF_VARS" ./OVMF_VARS.fd 2>/dev/null || sudo cp "$OVMF_VARS" ./OVMF_VARS.fd
sudo chmod 666 ./OVMF_VARS.fd 2>/dev/null || true

echo ""
echo "🚀 Lancement QEMU avec timeout de 60 secondes..."
echo "   v7.0 Features: Flash+INT8+Beam+Cache+Interactive+MultiModal"
echo ""
echo "📝 Log: $TEST_DIR/qemu-output.log"
echo ""

# Lancer QEMU avec timeout et logging
timeout 60s qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    -drive if=pflash,format=raw,file=./OVMF_VARS.fd \
    -drive file=fat.img,format=raw \
    -m 1024M \
    -cpu qemu64 \
    -smp 2 \
    -nographic \
    -serial mon:stdio 2>&1 | tee qemu-output.log

EXIT_CODE=$?

echo ""
echo "=========================================="
if [ $EXIT_CODE -eq 124 ]; then
    echo "✅ Test terminé (timeout 60s atteint)"
    echo "📊 Analysons la sortie..."
    echo ""
    
    # Analyser la sortie
    if grep -q "LlamaUltimate v7.0 UNIFIED" qemu-output.log; then
        echo "✅ Header v7.0 détecté"
    fi
    
    if grep -q "Flash Attention" qemu-output.log; then
        echo "✅ Flash Attention actif"
    fi
    
    if grep -q "Agent" qemu-output.log; then
        echo "✅ Agent Loop détecté"
    fi
    
    if grep -q "Multi-Modal" qemu-output.log; then
        echo "✅ Multi-Modal ready"
    fi
    
    if grep -q "Interactive" qemu-output.log; then
        echo "✅ Mode interactif activé"
    fi
    
    echo ""
    echo "📄 Dernières lignes du log:"
    tail -20 qemu-output.log
    
elif [ $EXIT_CODE -eq 0 ]; then
    echo "✅ QEMU terminé normalement"
else
    echo "⚠️  QEMU terminé avec code: $EXIT_CODE"
fi

echo "=========================================="
echo ""
echo "💡 Pour voir le log complet:"
echo "   cat $TEST_DIR/qemu-output.log"
echo ""
echo "💡 Pour relancer le test:"
echo "   bash $SCRIPT_DIR/test-qemu-v70.sh"
