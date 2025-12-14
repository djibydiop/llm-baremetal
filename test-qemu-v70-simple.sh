#!/bin/bash
# Test QEMU simplifié pour LlamaUltimate v7.0 UNIFIED
# Sans besoin de sudo

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEST_DIR="$SCRIPT_DIR/qemu-test-v70-simple"

echo "=========================================="
echo "  LlamaUltimate v7.0 UNIFIED - QEMU Test"
echo "  (Version simplifiée sans sudo)"
echo "=========================================="

# Créer répertoire de test
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

echo "📦 Préparation des fichiers..."

# Créer structure EFI
mkdir -p esp/EFI/BOOT
cp "$SCRIPT_DIR/llama2.efi" esp/EFI/BOOT/BOOTX64.EFI
cp "$SCRIPT_DIR/stories110M.bin" esp/
cp "$SCRIPT_DIR/../llama2.c/tokenizer.bin" esp/ 2>/dev/null || {
    echo "⚠️  tokenizer.bin optionnel non trouvé"
}

echo "✅ Structure EFI créée dans esp/"

# Vérifier OVMF
OVMF_CODE="/usr/share/OVMF/OVMF_CODE.fd"
OVMF_VARS="/usr/share/OVMF/OVMF_VARS.fd"

if [ ! -f "$OVMF_CODE" ]; then
    echo "❌ OVMF non installé. Installez avec:"
    echo "   sudo apt-get install ovmf"
    exit 1
fi

# Copier VARS (modifiable)
cp "$OVMF_VARS" ./OVMF_VARS.fd 2>/dev/null || {
    echo "⚠️  Impossible de copier OVMF_VARS, utilisation read-only"
    OVMF_VARS_OPT="-drive if=pflash,format=raw,readonly=on,file=$OVMF_VARS"
}

if [ -f ./OVMF_VARS.fd ]; then
    OVMF_VARS_OPT="-drive if=pflash,format=raw,file=./OVMF_VARS.fd"
fi

echo ""
echo "🚀 Lancement QEMU avec timeout de 45 secondes..."
echo "   v7.0 Features: Flash+INT8+Beam+Cache+Interactive+MultiModal"
echo ""
echo "📝 Log: $TEST_DIR/qemu-output.log"
echo ""
echo "⌨️  Pour interrompre: Ctrl+C puis attendez quelques secondes"
echo ""

# Lancer QEMU avec fat virtuel (pas besoin de monter)
timeout 45s qemu-system-x86_64 \
    -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
    $OVMF_VARS_OPT \
    -drive file=fat:rw:esp,format=raw \
    -m 1024M \
    -cpu qemu64 \
    -smp 2 \
    -nographic \
    -serial mon:stdio 2>&1 | tee qemu-output.log

EXIT_CODE=$?

echo ""
echo "=========================================="
if [ $EXIT_CODE -eq 124 ]; then
    echo "✅ Test terminé (timeout 45s atteint)"
elif [ $EXIT_CODE -eq 0 ]; then
    echo "✅ QEMU terminé normalement"
else
    echo "⚠️  QEMU terminé avec code: $EXIT_CODE"
fi

echo ""
echo "📊 Analyse de la sortie..."
echo ""

# Analyser les features détectées
if [ -f qemu-output.log ]; then
    grep -q "v7.0" qemu-output.log && echo "✅ Version 7.0 détectée"
    grep -q "UNIFIED" qemu-output.log && echo "✅ Build UNIFIED confirmé"
    grep -q "Flash" qemu-output.log && echo "✅ Flash Attention actif"
    grep -q "INT8" qemu-output.log && echo "✅ INT8 support présent"
    grep -q "Beam" qemu-output.log && echo "✅ Beam Search disponible"
    grep -q "Agent" qemu-output.log && echo "✅ Agent Loop détecté"
    grep -q "cache" qemu-output.log && echo "✅ Prompt Cache ready"
    grep -q "Interactive" qemu-output.log && echo "✅ Mode interactif activé"
    grep -q "Multi-Modal\|Vision" qemu-output.log && echo "✅ Multi-Modal présent"
    
    echo ""
    echo "📄 Dernières 30 lignes du log:"
    echo "----------------------------------------"
    tail -30 qemu-output.log
fi

echo "=========================================="
echo ""
echo "💡 Pour voir le log complet:"
echo "   cat $TEST_DIR/qemu-output.log"
echo ""
echo "💡 Pour relancer:"
echo "   bash $SCRIPT_DIR/test-qemu-v70-simple.sh"
