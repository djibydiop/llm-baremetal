#!/bin/bash
# Setup script for llm-baremetal
# Downloads model weights and prepares the build environment

set -e

echo "Setting up llm-baremetal..."
echo ""

# Check for required tools
echo "Checking dependencies..."
command -v gcc >/dev/null 2>&1 || { echo "Error: gcc not found. Install build-essential or equivalent."; exit 1; }
command -v ld >/dev/null 2>&1 || { echo "Error: ld not found. Install binutils."; exit 1; }
command -v objcopy >/dev/null 2>&1 || { echo "Error: objcopy not found. Install binutils."; exit 1; }
command -v qemu-system-x86_64 >/dev/null 2>&1 || { echo "Error: qemu-system-x86_64 not found. Install qemu."; exit 1; }

# Check for EFI headers
if [ ! -f /usr/include/efi/efi.h ]; then
    echo "Error: EFI development headers not found."
    echo "Install gnu-efi package:"
    echo "  Ubuntu/Debian: sudo apt install gnu-efi"
    echo "  Fedora: sudo dnf install gnu-efi gnu-efi-devel"
    echo "  Arch: sudo pacman -S gnu-efi"
    exit 1
fi

# Check for OVMF firmware
if [ ! -f /usr/share/ovmf/OVMF.fd ] && [ ! -f /usr/share/edk2-ovmf/OVMF.fd ]; then
    echo "Error: OVMF firmware not found."
    echo "Install ovmf package:"
    echo "  Ubuntu/Debian: sudo apt install ovmf"
    echo "  Fedora: sudo dnf install edk2-ovmf"
    echo "  Arch: sudo pacman -S edk2-ovmf"
    exit 1
fi

echo "All dependencies found."
echo ""

# Download model weights if not present
if [ ! -f stories15M.bin ]; then
    echo "Downloading stories15M.bin model (60MB)..."
    echo "Source: https://huggingface.co/karpathy/tinyllamas"
    
    if command -v curl >/dev/null 2>&1; then
        curl -L -o stories15M.bin https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin
    elif command -v wget >/dev/null 2>&1; then
        wget -O stories15M.bin https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin
    else
        echo "Error: Neither curl nor wget found. Cannot download model."
        echo "Please download manually from:"
        echo "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin"
        exit 1
    fi
    
    echo "Model downloaded successfully."
else
    echo "Model file stories15M.bin already exists."
fi

echo ""
echo "Setup complete!"
echo ""
echo "Next steps:"
echo "  1. Build: make"
echo "  2. Create disk: make llama2-disk"
echo "  3. Run: make test-llama2"
echo ""
