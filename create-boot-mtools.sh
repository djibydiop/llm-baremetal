#!/bin/bash
# Create bootable USB image with mtools (no sudo required)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "════════════════════════════════════════════════"
echo "🚀 Creating Bootable USB Image (mtools method)"
echo "════════════════════════════════════════════════"
echo ""

# Model selection (default: stories15M.bin)
# Usage:
#   MODEL_BIN=stories110M.bin ./create-boot-mtools.sh
MODEL_BIN="${MODEL_BIN:-stories15M.bin}"

# Check required files
echo "[1/4] Checking required files..."
for file in llama2.efi tokenizer.bin; do
    if [ ! -f "$file" ]; then
        echo "❌ Missing: $file"
        exit 1
    fi
done

MODEL_SRC="$MODEL_BIN"
if [ ! -f "$MODEL_SRC" ] && [ -f "../$MODEL_BIN" ]; then
    MODEL_SRC="../$MODEL_BIN"
fi
if [ ! -f "$MODEL_SRC" ]; then
    echo "❌ Missing model: $MODEL_BIN (looked in current dir and parent dir)"
    exit 1
fi
echo "✅ All files present"

# Create image file (auto-sized)
echo ""
MODEL_BYTES=$(stat -c %s "$MODEL_SRC")
MODEL_MIB=$(( (MODEL_BYTES + 1024*1024 - 1) / (1024*1024) ))
# Slack for FAT + GPT + EFI + tokenizer + alignment
SLACK_MIB=80
IMAGE_MIB=$(( MODEL_MIB + SLACK_MIB ))
if [ $IMAGE_MIB -lt 100 ]; then IMAGE_MIB=100; fi

echo "[2/4] Creating ${IMAGE_MIB}MB FAT32 image..."
IMAGE="llm-baremetal-boot.img"
rm -f "$IMAGE"
dd if=/dev/zero of="$IMAGE" bs=1M count=$IMAGE_MIB status=progress
echo "✅ Image created"

# Format as FAT32 with partition table
echo ""
echo "[3/4] Formatting as GPT + FAT32..."
# Use parted for GPT (doesn't need mount)
parted "$IMAGE" --script mklabel gpt
parted "$IMAGE" --script mkpart primary fat32 1MiB 100%
parted "$IMAGE" --script set 1 boot on
parted "$IMAGE" --script set 1 esp on

# Add hybrid MBR for maximum compatibility
echo "  Adding hybrid MBR bootcode..."
# Install GRUB MBR bootcode (helps some BIOS detect the disk)
dd if=/usr/lib/grub/i386-pc/boot.img of="$IMAGE" bs=440 count=1 conv=notrunc 2>/dev/null || echo "  (grub bootcode not found, skipping)"

# Format partition with mformat (no sudo!)
# Calculate offset: 1MiB = 2048 sectors * 512 bytes
OFFSET_BYTES=$((1024 * 1024))
mtoolsrc_tmp="$(mktemp)"
echo "mtools_skip_check=1" > "$mtoolsrc_tmp"
echo "drive z: file=\"${PWD}/${IMAGE}\" offset=${OFFSET_BYTES}" >> "$mtoolsrc_tmp"
export MTOOLSRC="$mtoolsrc_tmp"
mformat -F -v LLMBOOT z:
echo "✅ Formatted"

# Copy files with mcopy
echo ""
echo "[4/4] Copying files with mtools..."
mmd z:/EFI
mmd z:/EFI/BOOT
mcopy llama2.efi z:/EFI/BOOT/BOOTX64.EFI
echo "  ✅ Copied BOOTX64.EFI"

# Also keep a convenient copy at the root for manual launch in the UEFI shell
mcopy llama2.efi z:/LLAMA2.EFI
echo "  ✅ Copied LLAMA2.EFI"

mcopy "$MODEL_SRC" z:/"$(basename "$MODEL_BIN")"
echo "  ✅ Copied $(basename "$MODEL_BIN") (${MODEL_MIB} MB)"

mcopy tokenizer.bin z:/
echo "  ✅ Copied tokenizer.bin"

# Create startup.nsh for auto-boot
echo "\\EFI\\BOOT\\BOOTX64.EFI" > startup.nsh
mcopy startup.nsh z:/
rm -f startup.nsh
echo "  ✅ Copied startup.nsh"

# Cleanup
rm -f "$mtoolsrc_tmp"
echo "✅ Image finalized"

# Show result
echo ""
echo "════════════════════════════════════════════════"
echo "✅ BOOTABLE IMAGE CREATED!"
echo "════════════════════════════════════════════════"
echo ""
echo "📁 File: $(pwd)/$IMAGE"
echo "📊 Size: $(ls -lh $IMAGE | awk '{print $5}')"
echo ""
echo "🔥 Next steps:"
echo "  1. Open Rufus on Windows"
echo "  2. Select your USB drive"
echo "  3. Click SELECT and choose: $IMAGE"
echo "  4. Partition scheme: GPT"
echo "  5. Target system: UEFI (non CSM)"
echo "  6. Click START"
echo ""
echo "🚀 Boot will show optimized matmul message!"
