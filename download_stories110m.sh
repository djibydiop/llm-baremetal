#!/bin/bash
# Download stories110M pre-trained model from Karpathy
# This is a ready-to-use model, no training needed!

set -e

echo "╔════════════════════════════════════════════╗"
echo "║  📥 DOWNLOADING STORIES110M                ║"
echo "╚════════════════════════════════════════════╝"
echo ""

# Check if already downloaded
if [ -f "stories110M.bin" ]; then
    SIZE=$(ls -lh stories110M.bin | awk '{print $5}')
    echo "✅ stories110M.bin already exists ($SIZE)"
    read -p "   Re-download? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Using existing file"
        exit 0
    fi
fi

echo "📦 Downloading stories110M.bin..."
echo "   Size: ~420 MB"
echo "   Source: Karpathy's TinyLlamas"
echo ""

# Download with wget (shows progress)
if command -v wget &> /dev/null; then
    wget -c https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin
elif command -v curl &> /dev/null; then
    curl -L -C - -o stories110M.bin https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin
else
    echo "❌ Error: Neither wget nor curl found"
    echo "   Install with: sudo apt install wget"
    exit 1
fi

# Verify download
if [ -f "stories110M.bin" ]; then
    SIZE=$(ls -lh stories110M.bin | awk '{print $5}')
    echo ""
    echo "✅ Download complete! ($SIZE)"
    
    # Show model info
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║  📊 STORIES110M INFO                      ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    echo "Architecture:"
    echo "  • Parameters: ~110 million"
    echo "  • Layers: 12"
    echo "  • Heads: 12"
    echo "  • Embedding: 768"
    echo "  • Trained on: Full TinyStories dataset"
    echo ""
    echo "Performance:"
    echo "  • Much better coherence than stories15M"
    echo "  • Rich vocabulary and grammar"
    echo "  • Creative story generation"
    echo "  • File size: ~420 MB"
    echo ""
    echo "🔧 Next steps:"
    echo "  1. Update llama2_efi.c to use stories110M"
    echo "  2. Recompile: make clean && make"
    echo "  3. Create larger test image (512 MB)"
    echo "  4. Test: ./test-qemu.ps1"
    echo ""
else
    echo "❌ Download failed!"
    exit 1
fi
