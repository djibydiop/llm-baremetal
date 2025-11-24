#!/bin/bash
# QEMU Test Script for stories15M bare-metal LLM
# Uses Haswell CPU for AVX support

set -e

echo "🚀 Testing stories15M on QEMU with AVX support..."

# Check if image exists
if [ ! -f qemu-test.img ]; then
    echo "❌ qemu-test.img not found. Creating it..."
    
    # Create 256MB disk image
    dd if=/dev/zero of=qemu-test.img bs=1M count=256 status=progress
    
    # Format as FAT32
    mkfs.vfat qemu-test.img
    
    # Mount and copy files
    mkdir -p /tmp/qemu-mount
    sudo mount -o loop qemu-test.img /tmp/qemu-mount
    
    # Create EFI directory structure
    sudo mkdir -p /tmp/qemu-mount/EFI/BOOT
    
    # Copy EFI bootloader
    sudo cp llama2.efi /tmp/qemu-mount/EFI/BOOT/BOOTX64.EFI
    
    # Copy model files
    sudo cp stories15M.bin /tmp/qemu-mount/
    sudo cp tokenizer.bin /tmp/qemu-mount/
    
    # Unmount
    sudo umount /tmp/qemu-mount
    rmdir /tmp/qemu-mount
    
    echo "✅ Image created successfully!"
fi

echo ""
echo "═══════════════════════════════════════════"
echo "  Starting QEMU with Haswell CPU (AVX)"
echo "═══════════════════════════════════════════"
echo ""
echo "Press Ctrl+A then X to exit QEMU"
echo ""

# Run QEMU with Haswell CPU for AVX support
qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=qemu-test.img,format=raw \
    -m 4G \
    -cpu Haswell \
    -nographic \
    -serial stdio
