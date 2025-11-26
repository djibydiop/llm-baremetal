#!/bin/bash
# Build script for LLM Bare-Metal v2.0 with Hardware Auto-Detection

set -e

echo "============================================="
echo "Building LLM Bare-Metal v2.0"
echo "============================================="
echo ""

# Check if we're in the right directory
if [ ! -f "llama2_efi.c" ]; then
    echo "Error: llama2_efi.c not found!"
    echo "Please run this script from llm-baremetal directory"
    exit 1
fi

# Check for required tools
command -v gcc >/dev/null 2>&1 || { echo "Error: gcc not found!"; exit 1; }
command -v ld >/dev/null 2>&1 || { echo "Error: ld not found!"; exit 1; }
command -v objcopy >/dev/null 2>&1 || { echo "Error: objcopy not found!"; exit 1; }

echo "✓ Tools check passed"
echo ""

# Clean previous build
echo "Cleaning previous build..."
make clean 2>/dev/null || true
echo ""

# Compile
echo "Compiling llama2_efi.c with AVX2/FMA optimizations..."
make llama2.efi

if [ $? -eq 0 ]; then
    echo ""
    echo "============================================="
    echo "✓ Build successful!"
    echo "============================================="
    echo ""
    echo "Output: llama2.efi"
    ls -lh llama2.efi
    echo ""
    
    # Check for models
    echo "Checking for model files..."
    MODELS_FOUND=0
    
    for model in stories15M.bin stories42M.bin stories110M.bin stories260M.bin tinyllama_1b.bin llama2_7b.bin nanogpt.bin; do
        if [ -f "$model" ]; then
            SIZE=$(du -h "$model" | cut -f1)
            echo "  ✓ $model ($SIZE)"
            MODELS_FOUND=$((MODELS_FOUND + 1))
        fi
    done
    
    if [ $MODELS_FOUND -eq 0 ]; then
        echo "  ⚠️  No model files found"
        echo ""
        echo "Download at least one model:"
        echo "  wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin"
        echo "  wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin"
    else
        echo ""
        echo "Found $MODELS_FOUND model(s)"
    fi
    
    echo ""
    echo "Next steps:"
    echo "  1. Download models (if not done): See above URLs"
    echo "  2. Download tokenizer: wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin"
    echo "  3. Create disk image: make disk"
    echo "  4. Test in QEMU: make run"
    echo ""
else
    echo ""
    echo "============================================="
    echo "✗ Build failed!"
    echo "============================================="
    exit 1
fi
