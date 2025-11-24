# Quick Start Guide

Get stories110M running in 5 minutes!

## Prerequisites

### Linux/WSL/macOS
- GCC compiler
- GNU EFI development headers
- QEMU with OVMF firmware
- mtools (FAT filesystem utilities)

### Installation Commands

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y build-essential gnu-efi qemu-system-x86 ovmf mtools
```

**Fedora/RHEL:**
```bash
sudo dnf install -y gcc binutils gnu-efi-devel qemu-system-x86 edk2-ovmf mtools
```

**macOS:**
```bash
brew install qemu
# Note: GNU EFI may need manual installation
```

## Step-by-Step Setup

### 1. Clone Repository
```bash
git clone https://github.com/npdji/YamaOO.git
cd YamaOO/llm-baremetal
```

### 2. Download Model
```bash
# Download stories110M (~420MB) from HuggingFace
bash download_stories110m.sh

# Wait for download to complete
# You should see: stories110M.bin (418 MB)
```

### 3. Build
```bash
# Compile the EFI application
make clean
make

# Create bootable disk image
make disk
```

### 4. Test in QEMU

**Linux/WSL/macOS:**
```bash
bash test-qemu.sh
```

**Windows PowerShell:**
```powershell
.\test-qemu.ps1
```

**Manual QEMU command:**
```bash
qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive format=raw,file=qemu-test.img \
    -m 4G \
    -cpu Haswell \
    -nographic \
    -serial mon:stdio
```

### 5. Boot on Real Hardware (Optional)

⚠️ **WARNING**: This will ERASE the target USB drive!

```bash
# Find your USB drive
lsblk  # or 'sudo fdisk -l'

# Write image to USB (replace /dev/sdX with your drive)
sudo dd if=qemu-test.img of=/dev/sdX bs=4M status=progress
sync

# Boot from USB on UEFI system
# Press F12/F10/Del during boot to select USB
```

## What to Expect

### QEMU Output
```
Loading model: stories110M.bin
... 427520 KB read
Model loaded successfully!
Loading tokenizer...
Tokenizer loaded: 32000 tokens

=== Interactive Menu ===
Select Prompt Category:
  1. Stories (fairy tales, adventures)
  2. Science (facts, explanations)
  3. Adventure (quests, journeys)
  4. Custom (your own prompt)
  5. Auto-Demo (run all categories)

Choice [1-5]: (Auto-Demo mode active in QEMU)

=== Category: STORIES ===
>>> [Prompt 1/3]
Prompt: "Once upon a time, in a magical kingdom"

[Text generation starts here...]
```

### Performance
- **Load time**: ~5-10 seconds (loading 420MB model)
- **Generation speed**: 4-7 tokens/sec in QEMU
- **Real hardware**: Expected 10-20 tokens/sec

## Troubleshooting

### Issue: "OVMF.fd not found"
**Solution:**
```bash
# Ubuntu/Debian
sudo apt install ovmf
# OVMF path: /usr/share/ovmf/OVMF.fd

# Fedora
sudo dnf install edk2-ovmf
# OVMF path: /usr/share/edk2/ovmf/OVMF_CODE.fd
```

### Issue: "Invalid Opcode Exception"
**Solution:** Use Haswell CPU in QEMU (not qemu64)
```bash
qemu-system-x86_64 -cpu Haswell ...
```
The model requires AVX2 support which qemu64 lacks.

### Issue: "stories110M.bin not found"
**Solution:** 
```bash
# Download the model
bash download_stories110m.sh

# Or manual download:
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin
```

### Issue: Compilation errors
**Solution:**
```bash
# Install GNU EFI headers
sudo apt install gnu-efi

# Check paths in Makefile
grep "include/efi" Makefile
```

### Issue: Slow performance in QEMU
**Expected:** QEMU is slower than real hardware (virtualization overhead)
- QEMU: 4-7 tok/s
- Real HW: 10-20 tok/s (estimated)

Use `-cpu host` on Linux for better performance (requires KVM).

## Interactive Menu

The bootloader shows a menu with 5 options:

1. **Stories** - Fairy tales and narrative prompts
2. **Science** - Educational explanations
3. **Adventure** - Quest and exploration stories
4. **Custom** - Enter your own prompt (real hardware only)
5. **Auto-Demo** - Automatically cycles through all categories

**Note:** Keyboard input doesn't work in QEMU/OVMF. The auto-demo mode will run automatically. Full interactivity works on real UEFI hardware.

## Next Steps

After successful boot:

1. **Try real hardware**: Write to USB and boot on a real UEFI system
2. **Train custom model**: See `train_stories101m.sh` to train your own
3. **Explore optimizations**: Read `PERFORMANCE_OPTIMIZATIONS.md`
4. **Contribute**: Submit improvements via pull request

## Files Overview

- `llama2_efi.c` - Main EFI application (transformer inference)
- `Makefile` - Build configuration
- `test-qemu.sh` - QEMU test script (Linux/macOS)
- `test-qemu.ps1` - QEMU test script (Windows)
- `download_stories110m.sh` - Model download script
- `stories110M.bin` - 420MB model file (downloaded separately)
- `tokenizer.bin` - BPE tokenizer (32K vocab)
- `qemu-test.img` - Bootable disk image (created by `make disk`)

## Support

- **GitHub Issues**: https://github.com/npdji/YamaOO/issues
- **Documentation**: See README.md and OPTIMIZATION_GUIDE.md

## License

See LICENSE file in repository.
