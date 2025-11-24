# Hardware Boot Guide - LLM Bare-Metal UEFI System

## Overview

This guide explains how to boot the LLM Bare-Metal system on **real UEFI hardware** (physical computers) instead of QEMU emulation.

## System Features

- **110M Parameter Language Model** running on bare metal (no OS)
- **BPE Tokenization** for accurate prompt understanding
- **6 Categories**: Stories, Science, Adventure, Philosophy, History, Technology
- **41 Pre-configured Prompts** for diverse interactions
- **AVX2 Optimized** inference for maximum performance
- **Temperature-controlled** text generation

## Hardware Requirements

### Minimum Requirements

| Component | Requirement |
|-----------|-------------|
| **CPU** | x86-64 with AVX2 support (Intel Haswell+ or AMD Excavator+) |
| **RAM** | 4GB minimum (model + KV cache) |
| **Firmware** | UEFI (not Legacy BIOS) |
| **Storage** | USB drive (1GB+) or internal drive |
| **Display** | Any monitor (VGA/HDMI/DisplayPort) |

### Recommended Hardware

- **Intel**: Core i5 4th gen or newer (2013+)
- **AMD**: Ryzen or FX-9000 series
- **RAM**: 8GB for comfortable operation
- **USB**: USB 3.0 for faster model loading

### Check CPU Compatibility

#### Linux:
```bash
grep avx2 /proc/cpuinfo
```

#### Windows:
```powershell
Get-WmiObject -Class Win32_Processor | Select-Object -ExpandProperty Name
```

Look for processors from:
- Intel: Haswell (2013) or newer
- AMD: Excavator (2015) or newer

## Deployment Methods

### Method 1: USB Drive (Recommended)

#### Windows:
```powershell
# Build and deploy to USB drive E:
.\deploy-usb.ps1 -DriveLetter E
```

#### Linux:
```bash
# Build and deploy to /dev/sdb (WARNING: formats drive!)
sudo ./deploy-usb.sh /dev/sdb
```

### Method 2: Manual Deployment

1. **Format USB drive as FAT32** (UEFI requirement)
2. **Create directory structure**:
   ```
   USB:\
   ├── EFI\
   │   └── BOOT\
   │       └── BOOTX64.EFI    (renamed from llama2_efi.efi)
   ├── stories110M.bin         (420MB)
   └── tokenizer.bin           (434KB)
   ```
3. **Copy files** from build directory
4. **Safely eject** USB drive

### Method 3: Internal Drive Installation

For permanent installation (e.g., dual-boot):

1. Create a **FAT32 EFI System Partition** (ESP)
2. Mount ESP (usually at `/boot/efi` or `/efi`)
3. Copy files:
   ```bash
   sudo cp llama2_efi.efi /boot/efi/EFI/BOOT/BOOTX64.EFI
   sudo cp stories110M.bin /boot/efi/
   sudo cp tokenizer.bin /boot/efi/
   ```
4. Add boot entry with `efibootmgr` (Linux)

## Booting Instructions

### Step 1: Enter BIOS/UEFI Settings

Power on the machine and press:
- **Dell/Lenovo**: F12
- **HP**: F10 or Esc
- **Asus/MSI**: F2 or Delete
- **Apple**: Hold Option/Alt key

### Step 2: Configure UEFI Settings

#### Disable Secure Boot
1. Navigate to **Security** or **Boot** tab
2. Find **Secure Boot** option
3. Set to **Disabled**
4. Save and exit

*Why?* Secure Boot requires signed bootloaders. Our custom UEFI application is unsigned.

#### Set Boot Mode to UEFI (Not Legacy)
1. Find **Boot Mode** or **Boot List Option**
2. Select **UEFI** (not CSM, Legacy, or Both)

#### Set Boot Order
1. Find **Boot Priority** or **Boot Order**
2. Move USB drive to **first position**
3. Save changes (usually F10)

### Step 3: Boot

1. System will restart automatically
2. UEFI firmware will load `BOOTX64.EFI`
3. Model loading begins (~30 seconds)
4. Interactive menu appears

## What to Expect

### Boot Sequence

```
1. UEFI Logo (2-5 seconds)
2. "LLaMA2 Bare-Metal REPL" banner
3. Model loading: stories110M.bin (30-60 seconds)
   Progress: "... 128000 KB read ... 256000 KB read ..."
4. Tokenizer loading: tokenizer.bin (1 second)
   "Tokenizer loaded: 32000 tokens"
5. Menu display with 6 categories
6. Auto-demo starts generating text
```

### Performance Expectations

| Metric | Expected Value |
|--------|----------------|
| **Boot Time** | 40-90 seconds |
| **Model Load** | 30-60 seconds (depends on USB speed) |
| **Generation Speed** | 1-3 tokens/second (depends on CPU) |
| **Memory Usage** | ~500MB (model + activations) |

## Troubleshooting

### Problem: Black Screen After Boot

**Causes:**
- Secure Boot still enabled
- Legacy BIOS mode active
- Corrupted bootloader

**Solutions:**
1. Re-enter BIOS and verify Secure Boot is OFF
2. Ensure boot mode is UEFI (not Legacy/CSM)
3. Reformat USB and redeploy

### Problem: "No Bootable Device" Error

**Causes:**
- USB drive not in boot order
- Files in wrong directory
- Drive not formatted as FAT32

**Solutions:**
1. Check BIOS boot order (USB should be #1)
2. Verify `EFI\BOOT\BOOTX64.EFI` exists (not `llama2_efi.efi`)
3. Reformat USB as FAT32 (UEFI requirement)

### Problem: System Hangs During Model Loading

**Causes:**
- Insufficient RAM (< 4GB)
- Slow USB 2.0 drive
- Corrupted model file

**Solutions:**
1. Verify 4GB+ RAM available
2. Use USB 3.0 drive if possible
3. Re-download `stories110M.bin` and verify checksum

### Problem: "CPU doesn't support AVX2" Error

**Causes:**
- Older CPU without AVX2 (pre-2013 Intel, pre-2015 AMD)

**Solutions:**
1. Boot on newer hardware with AVX2 support
2. Check `/proc/cpuinfo` (Linux) or CPU specs online
3. Consider rebuilding with SSE2-only code (slower)

### Problem: Garbage Text Generation

**Causes:**
- Tokenizer not loaded
- Corrupted tokenizer.bin
- Model mismatch

**Solutions:**
1. Verify `tokenizer.bin` is present on USB
2. Check "Tokenizer loaded: 32000 tokens" message appears
3. Ensure using correct model (stories110M, not stories15M)

## Advanced Usage

### Testing Specific Prompts

The system boots into auto-demo mode. To modify prompts:

1. Edit `llama2_efi.c` before building
2. Modify `story_prompts[]`, `science_prompts[]`, etc.
3. Rebuild with `make clean && make`
4. Redeploy to USB

### Performance Tuning

Edit `llama2_efi.c` constants:

```c
float temperature = 1.0f;    // Lower = more deterministic (0.7-0.9)
int steps = 100;             // Increase for longer generations (100-256)
int max_response_tokens = 80; // Tokens per response
```

### Using Different Models

The system supports any Llama2-format model:

1. Download model (e.g., `stories15M.bin`, `llama2_7b.bin`)
2. Rename model file or update `model_path` in code
3. Ensure sufficient RAM (stories15M=80MB, llama2_7b=13GB)
4. Rebuild and redeploy

## Hardware Compatibility List

### Tested Systems

| Manufacturer | Model | Status | Notes |
|--------------|-------|--------|-------|
| *Your tests here* | - | ✅/❌ | Add after testing |

### Known Compatible CPUs

- **Intel**: Haswell (i3/i5/i7 4xxx), Broadwell, Skylake, Kaby Lake, Coffee Lake, Comet Lake, Ice Lake, Tiger Lake, Alder Lake
- **AMD**: Excavator (FX-9xxx, A10-8xxx), Zen 1/2/3/4 (Ryzen 1000-7000 series), Threadripper, EPYC

## Safety & Warnings

⚠️ **Important:**
- Deployment scripts **format USB drives** - backup data first!
- Disabling Secure Boot reduces security - re-enable after testing
- System has no file save - output is display-only
- USB wear: Frequent writes may reduce lifespan

## Technical Details

### Memory Layout

| Component | Size | Address Range |
|-----------|------|---------------|
| UEFI Firmware | Varies | < 4GB |
| Model Weights | 420MB | Heap allocated |
| Tokenizer | 434KB | Heap allocated |
| KV Cache | 72MB | Heap allocated |
| Activations | ~5MB | Stack/Heap |

### File System Requirements

- **Partition Type**: GPT (not MBR)
- **Partition Label**: ESP (EFI System Partition)
- **File System**: FAT32 (FAT16 also works, exFAT NOT supported)
- **Bootloader Path**: `\EFI\BOOT\BOOTX64.EFI` (case-insensitive)

## Further Reading

- **UEFI Specification**: https://uefi.org/specifications
- **GNU-EFI Documentation**: https://sourceforge.net/projects/gnu-efi/
- **LLaMA Architecture**: https://arxiv.org/abs/2302.13971
- **GitHub Repository**: https://github.com/djibydiop/llm-baremetal

## Support

- **Issues**: https://github.com/djibydiop/llm-baremetal/issues
- **Discussions**: GitHub Discussions
- **Email**: djibirooney@gmail.com

## Success Stories

Share your hardware boot experiences! Open a PR to add your system to the compatibility list.

---

**Ready to boot on hardware?** Follow the deployment steps and enjoy bare-metal LLM inference! 🚀
