#!/bin/bash
# Deploy LLM Bare-Metal System to USB Drive for Hardware Boot
# Usage: ./deploy-usb.sh /dev/sdX (e.g., /dev/sdb)

set -e

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <usb_device>"
    echo "Example: $0 /dev/sdb"
    echo ""
    echo "WARNING: This will FORMAT the USB drive!"
    echo "List available devices: lsblk"
    exit 1
fi

USB_DEVICE=$1
USB_MOUNT="/mnt/llm-usb"

echo "=== LLM Bare-Metal USB Deployment ==="
echo ""

# Safety check
if [ ! -b "$USB_DEVICE" ]; then
    echo "[ERROR] Device $USB_DEVICE not found!"
    exit 1
fi

# Confirm
echo "[WARNING] This will FORMAT $USB_DEVICE and ERASE ALL DATA!"
echo -n "Type 'yes' to continue: "
read CONFIRM
if [ "$CONFIRM" != "yes" ]; then
    echo "Aborted."
    exit 0
fi

echo ""
echo "[INFO] Unmounting any existing partitions..."
sudo umount ${USB_DEVICE}* 2>/dev/null || true

echo "[INFO] Creating GPT partition table..."
sudo parted -s $USB_DEVICE mklabel gpt

echo "[INFO] Creating EFI System Partition (ESP)..."
sudo parted -s $USB_DEVICE mkpart primary fat32 1MiB 100%
sudo parted -s $USB_DEVICE set 1 esp on

# Wait for device to be ready
sleep 2

PARTITION="${USB_DEVICE}1"
if [ ! -b "$PARTITION" ]; then
    # Handle device naming (e.g., /dev/mmcblk0p1 vs /dev/sdb1)
    if [[ $USB_DEVICE == *"mmcblk"* ]] || [[ $USB_DEVICE == *"nvme"* ]]; then
        PARTITION="${USB_DEVICE}p1"
    fi
fi

echo "[INFO] Formatting partition as FAT32..."
sudo mkfs.vfat -F 32 -n "LLM_BOOT" $PARTITION

echo "[INFO] Mounting USB drive..."
sudo mkdir -p $USB_MOUNT
sudo mount $PARTITION $USB_MOUNT

echo ""
echo "[INFO] Building LLM system..."
make clean
make

echo "[INFO] Creating EFI directory structure..."
sudo mkdir -p $USB_MOUNT/EFI/BOOT

echo "[INFO] Copying UEFI bootloader..."
sudo cp llama2_efi.efi $USB_MOUNT/EFI/BOOT/BOOTX64.EFI
echo "  ✓ BOOTX64.EFI copied"

echo "[INFO] Copying model (stories110M.bin - 420MB)..."
if [ -f "stories110M.bin" ]; then
    sudo cp stories110M.bin $USB_MOUNT/
    echo "  ✓ stories110M.bin copied"
else
    echo "  [WARNING] stories110M.bin not found - download it separately!"
fi

echo "[INFO] Copying tokenizer (tokenizer.bin - 434KB)..."
if [ -f "tokenizer.bin" ]; then
    sudo cp tokenizer.bin $USB_MOUNT/
    echo "  ✓ tokenizer.bin copied"
else
    echo "  [WARNING] tokenizer.bin not found!"
fi

echo "[INFO] Creating README..."
sudo tee $USB_MOUNT/README.txt > /dev/null << 'EOF'
LLM Bare-Metal UEFI System
===========================

This USB drive contains a complete LLM inference system that boots directly
on UEFI hardware without an operating system.

Contents:
- EFI/BOOT/BOOTX64.EFI - UEFI bootloader with LLM implementation
- stories110M.bin - 110M parameter language model (420MB)
- tokenizer.bin - BPE tokenizer for text encoding/decoding (434KB)

Boot Instructions:
1. Insert USB drive into target machine
2. Enter BIOS/UEFI settings (F2, F10, F12, or DEL key at boot)
3. Set USB drive as first boot device
4. Save and exit BIOS
5. System will boot and load LLM automatically

Hardware Requirements:
- UEFI firmware (most PCs since 2010)
- x86-64 CPU with AVX2 support
- 4GB+ RAM
- USB boot capability

Features:
- Interactive menu with 6 categories (Stories, Science, Adventure, Philosophy, History, Technology)
- 41 pre-configured prompts
- BPE tokenization for prompt understanding
- AVX2 optimized inference
- Temperature-controlled generation

Troubleshooting:
- If boot fails, check Secure Boot is disabled in BIOS
- Verify UEFI boot mode (not Legacy/CSM)
- Ensure USB drive is recognized in BIOS boot menu

GitHub: https://github.com/djibydiop/llm-baremetal
EOF

echo "  ✓ README.txt created"

echo ""
echo "[INFO] Syncing filesystem..."
sudo sync

echo "[INFO] Unmounting USB drive..."
sudo umount $USB_MOUNT

echo ""
echo "=== Deployment Complete ==="
echo "USB Device: $USB_DEVICE"
echo "Files deployed:"
echo "  - EFI/BOOT/BOOTX64.EFI"
echo "  - stories110M.bin (420MB)"
echo "  - tokenizer.bin (434KB)"
echo "  - README.txt"
echo ""
echo "You can now boot from this USB drive on any UEFI x86-64 machine!"
echo "Remember to disable Secure Boot in BIOS if boot fails."
echo ""
echo "Safe to remove USB drive now."
