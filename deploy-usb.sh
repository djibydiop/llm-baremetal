#!/bin/bash
# USB Deployment Script for LLM-Baremetal with 9 Revolutionary Systems
# Made in Senegal by Djiby Diop - December 2025
#
# WARNING: This will ERASE all data on the target USB drive!
# Usage: sudo ./deploy-usb.sh /dev/sdX

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}❌ Error: This script must be run as root${NC}"
    echo "Usage: sudo $0 /dev/sdX"
    exit 1
fi

# Check argument
if [ -z "$1" ]; then
    echo -e "${RED}❌ Error: No USB device specified${NC}"
    echo "Usage: sudo $0 /dev/sdX"
    echo ""
    echo "Available devices:"
    lsblk -d -o NAME,SIZE,TYPE,TRAN | grep -E '(NAME|usb)'
    exit 1
fi

USB_DEVICE="$1"

# Validate device exists
if [ ! -b "$USB_DEVICE" ]; then
    echo -e "${RED}❌ Error: Device $USB_DEVICE does not exist${NC}"
    exit 1
fi

# Safety check: don't allow /dev/sda (usually main disk)
if [ "$USB_DEVICE" == "/dev/sda" ]; then
    echo -e "${RED}❌ DANGER: Refusing to use /dev/sda (likely your main disk!)${NC}"
    echo "Please specify a USB device like /dev/sdb or /dev/sdc"
    exit 1
fi

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  USB DEPLOYMENT - 9 REVOLUTIONARY SYSTEMS${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

# Show device info
echo -e "${YELLOW}Target device:${NC} $USB_DEVICE"
lsblk -o NAME,SIZE,TYPE,MOUNTPOINT "$USB_DEVICE" || true
echo ""

# Final warning
echo -e "${RED}⚠️  WARNING: This will ERASE ALL DATA on $USB_DEVICE!${NC}"
echo -n "Type 'YES' to continue: "
read -r CONFIRM

if [ "$CONFIRM" != "YES" ]; then
    echo -e "${YELLOW}Deployment cancelled.${NC}"
    exit 0
fi

echo ""
echo -e "${GREEN}[1/6]${NC} Unmounting partitions..."
umount "${USB_DEVICE}"* 2>/dev/null || true

echo -e "${GREEN}[2/6]${NC} Creating GPT partition table..."
parted -s "$USB_DEVICE" mklabel gpt

echo -e "${GREEN}[3/6]${NC} Creating EFI System Partition (ESP)..."
parted -s "$USB_DEVICE" mkpart primary fat32 1MiB 513MiB
parted -s "$USB_DEVICE" set 1 esp on

# Wait for partition to appear
sleep 2
PARTITION="${USB_DEVICE}1"

echo -e "${GREEN}[4/6]${NC} Formatting as FAT32..."
mkfs.vfat -F 32 -n "LLM_BOOT" "$PARTITION"

echo -e "${GREEN}[5/6]${NC} Mounting and copying files..."
MOUNT_POINT="/tmp/llm-boot-$$"
mkdir -p "$MOUNT_POINT"
mount "$PARTITION" "$MOUNT_POINT"

# Create EFI directory structure
mkdir -p "$MOUNT_POINT/EFI/BOOT"

# Copy kernel
if [ ! -f "llama2.efi" ]; then
    echo -e "${RED}❌ Error: llama2.efi not found${NC}"
    umount "$MOUNT_POINT"
    rmdir "$MOUNT_POINT"
    exit 1
fi

echo "   Copying llama2.efi ($(du -h llama2.efi | cut -f1))..."
cp llama2.efi "$MOUNT_POINT/EFI/BOOT/BOOTX64.EFI"

# Copy model if it exists
if [ -f "stories15M.bin" ]; then
    echo "   Copying stories15M.bin ($(du -h stories15M.bin | cut -f1))..."
    cp stories15M.bin "$MOUNT_POINT/"
else
    echo -e "${YELLOW}   ⚠️  stories15M.bin not found - you'll need to add it manually${NC}"
fi

# Create README on USB
cat > "$MOUNT_POINT/README.txt" << 'EOF'
================================================================================
LLM-BAREMETAL - 9 Revolutionary Memory Systems
Made in Senegal by Djiby Diop - December 2025
================================================================================

This USB drive contains a bare-metal LLM inference kernel that boots directly
on UEFI firmware without any operating system.

REVOLUTIONARY FEATURES:
- Memory Consciousness (ML-like pattern prediction)
- Self-Healing Memory (auto-repair)
- Time-Travel Debugging (snapshots)
- Quantum Memory Coherence
- Speculative Execution Engine
- Neural Memory Allocator
- Adversarial Attack Testing
- Blockchain Memory Verification
- Distributed Memory Cluster
- Optimized Matrix Multiplication (blocked algorithm, CPU feature detection)

BOOT INSTRUCTIONS:
1. Insert this USB drive into a x86_64 UEFI computer
2. Enter BIOS/UEFI settings (usually F2, F12, DEL, or ESC during boot)
3. Disable Secure Boot (if enabled)
4. Set boot order to boot from USB first
5. Save and exit BIOS
6. System will boot into LLM inference kernel

REQUIREMENTS:
- x86_64 architecture (Intel/AMD 64-bit)
- UEFI firmware (not legacy BIOS)
- At least 512 MB RAM
- AVX2 support recommended for optimal performance

TROUBLESHOOTING:
- If boot fails, try disabling Secure Boot
- If inference is slow, check CPU supports AVX2 (2013+ Intel/AMD)
- If model not found, ensure stories15M.bin is in root directory

FILES ON THIS USB:
- EFI/BOOT/BOOTX64.EFI : UEFI bootloader (llama2.efi kernel, 1.2 MB)
- stories15M.bin       : Language model (57 MB, 6 layers, 288 dim)
- README.txt           : This file

PROJECT INFO:
- GitHub: (add your repo link)
- Made in Dakar, Senegal
- December 2025

"From Senegal to the world, one kernel at a time."
================================================================================
EOF

sync

echo -e "${GREEN}[6/6]${NC} Unmounting and finalizing..."
umount "$MOUNT_POINT"
rmdir "$MOUNT_POINT"

echo ""
echo -e "${BLUE}================================================${NC}"
echo -e "${GREEN}✅ DEPLOYMENT COMPLETE!${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""
echo "USB drive is ready to boot. Next steps:"
echo ""
echo "1. Insert USB into a UEFI x86_64 computer"
echo "2. Enter BIOS/UEFI settings (F2, F12, DEL, or ESC)"
echo "3. Disable Secure Boot"
echo "4. Set USB as first boot device"
echo "5. Save and reboot"
echo ""
echo "The system will boot directly into LLM inference."
echo ""
echo -e "${YELLOW}Device:${NC} $USB_DEVICE"
echo -e "${YELLOW}Label:${NC}  LLM_BOOT"
echo -e "${YELLOW}Size:${NC}   $(lsblk -d -o SIZE "$USB_DEVICE" | tail -1)"
echo ""
echo -e "${GREEN}Made in Senegal 🇸🇳 - Djiby Diop${NC}"
