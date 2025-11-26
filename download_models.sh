#!/bin/bash
# Automatic model download script for llm-baremetal
# Downloads recommended models from HuggingFace

set -e

MODELS_URL="https://huggingface.co/karpathy/tinyllamas/resolve/main"
TOKENIZER_URL="${MODELS_URL}/tokenizer.bin"

echo "=== LLM Bare-Metal Model Downloader ==="
echo ""
echo "This script will download transformer models from HuggingFace."
echo "Source: https://huggingface.co/karpathy/tinyllamas"
echo ""

# Check for wget or curl
if command -v wget &> /dev/null; then
    DOWNLOADER="wget"
elif command -v curl &> /dev/null; then
    DOWNLOADER="curl -LO"
else
    echo "Error: Neither wget nor curl found. Please install one of them."
    exit 1
fi

# Function to download file
download_file() {
    local url=$1
    local filename=$2
    local size=$3
    
    if [ -f "$filename" ]; then
        echo "[SKIP] $filename already exists"
        return 0
    fi
    
    echo "[DOWNLOAD] $filename ($size)..."
    if [ "$DOWNLOADER" = "wget" ]; then
        wget -q --show-progress "$url" -O "$filename"
    else
        curl -# -L "$url" -o "$filename"
    fi
    echo "[OK] $filename downloaded"
}

# Download tokenizer (required)
echo ""
echo "Downloading tokenizer (required)..."
download_file "$TOKENIZER_URL" "tokenizer.bin" "434KB"

# Download models (prompt user)
echo ""
echo "Select models to download:"
echo "  1. stories15M.bin (60MB) - Recommended for USB boot"
echo "  2. stories42M.bin (165MB) - Better quality"
echo "  3. stories110M.bin (420MB) - Best quality"
echo "  4. All models"
echo ""
read -p "Enter choice (1-4, or 0 to skip): " choice

case $choice in
    1)
        download_file "${MODELS_URL}/stories15M.bin" "stories15M.bin" "60MB"
        ;;
    2)
        download_file "${MODELS_URL}/stories42M.bin" "stories42M.bin" "165MB"
        ;;
    3)
        download_file "${MODELS_URL}/stories110M.bin" "stories110M.bin" "420MB"
        ;;
    4)
        download_file "${MODELS_URL}/stories15M.bin" "stories15M.bin" "60MB"
        download_file "${MODELS_URL}/stories42M.bin" "stories42M.bin" "165MB"
        download_file "${MODELS_URL}/stories110M.bin" "stories110M.bin" "420MB"
        ;;
    0)
        echo "Skipping model download"
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac

echo ""
echo "=== Download Complete ==="
echo ""
echo "Next steps:"
echo "  1. Build: make clean && make"
echo "  2. Deploy: ./deploy-usb.sh /dev/sdX  (replace X with your USB drive)"
echo "  3. Boot from USB and test"
echo ""
