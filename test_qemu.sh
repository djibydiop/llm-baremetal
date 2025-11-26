#!/bin/bash
# Cross-platform QEMU test script for llm-baremetal
# Works on Linux, macOS, and WSL

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Detect platform
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    PLATFORM="windows"
else
    PLATFORM="unknown"
fi

echo "=== LLM Bare-Metal QEMU Test ==="
echo "Platform: $PLATFORM"
echo ""

# Check for QEMU
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo -e "${RED}Error: qemu-system-x86_64 not found${NC}"
    echo ""
    echo "Install QEMU:"
    case $PLATFORM in
        linux)
            echo "  Ubuntu/Debian: sudo apt install qemu-system-x86 ovmf"
            echo "  Fedora/RHEL:   sudo dnf install qemu-system-x86 edk2-ovmf"
            echo "  Arch:          sudo pacman -S qemu-system-x86 edk2-ovmf"
            ;;
        macos)
            echo "  Homebrew: brew install qemu"
            ;;
        windows)
            echo "  Download from: https://www.qemu.org/download/#windows"
            ;;
    esac
    exit 1
fi

# Find OVMF firmware
OVMF_PATH=""
OVMF_SEARCH_PATHS=(
    "/usr/share/qemu/OVMF.fd"
    "/usr/share/OVMF/OVMF_CODE.fd"
    "/usr/share/edk2/ovmf/OVMF_CODE.fd"
    "/usr/local/share/qemu/edk2-x86_64-code.fd"
    "/opt/homebrew/share/qemu/edk2-x86_64-code.fd"
    "./OVMF.fd"
)

for path in "${OVMF_SEARCH_PATHS[@]}"; do
    if [ -f "$path" ]; then
        OVMF_PATH="$path"
        break
    fi
done

if [ -z "$OVMF_PATH" ]; then
    echo -e "${RED}Error: OVMF firmware not found${NC}"
    echo "Searched paths:"
    printf '%s\n' "${OVMF_SEARCH_PATHS[@]}"
    exit 1
fi

echo -e "${GREEN}Found OVMF:${NC} $OVMF_PATH"

# Check for disk image
if [ ! -f "usb_512mb.img" ]; then
    echo -e "${RED}Error: usb_512mb.img not found${NC}"
    echo "Create it first with: make disk"
    exit 1
fi

echo -e "${GREEN}Found disk image:${NC} usb_512mb.img"

# Check if models are in the image
echo ""
echo "Checking disk image contents..."
if command -v mdir &> /dev/null; then
    mdir -i usb_512mb.img ::/ | grep -E '\.bin$|\.efi$' || true
else
    echo -e "${YELLOW}Note: Install mtools to verify disk contents${NC}"
fi

# Run QEMU
echo ""
echo "=== Starting QEMU ==="
echo "CPU: Haswell (AVX2 enabled)"
echo "RAM: 1024MB"
echo "Timeout: 120 seconds"
echo ""
echo "Press Ctrl+C to exit early"
echo ""

timeout 120 qemu-system-x86_64 \
    -bios "$OVMF_PATH" \
    -drive format=raw,file=usb_512mb.img \
    -cpu Haswell \
    -m 1024 \
    -nographic \
    -serial mon:stdio \
    2>&1 || {
    exit_code=$?
    if [ $exit_code -eq 124 ]; then
        echo ""
        echo -e "${YELLOW}Test completed (timeout reached)${NC}"
        exit 0
    else
        echo ""
        echo -e "${RED}QEMU exited with code $exit_code${NC}"
        exit $exit_code
    fi
}

echo ""
echo -e "${GREEN}Test completed successfully${NC}"
