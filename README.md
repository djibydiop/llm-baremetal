# 🚀 LLM Bare-Metal Bootloader

**stories110M transformer running directly on UEFI firmware - no OS required.**

## Features

- 🔥 **Powerful**: 110M parameter GPT model (420MB)
- 🚀 Bare-metal UEFI implementation (no operating system)
- 🧠 stories110M: 12 layers, 768 dimensions, 12 attention heads
- 🎯 **Interactive menu** with categorized prompts (Stories, Science, Adventure)
- ⚡ **Optimized**: Loop unrolling + AVX2 (~1.5-2x speedup)
- 🔤 **Complete BPE tokenizer** (32000 vocab)
- 📊 Auto-demo mode for QEMU testing

## Performance

| Model | Size | Architecture | QEMU | Real Hardware (est.) |
|-------|------|--------------|------|---------------------|
| **stories110M** | 420MB | 12L/768D/12H | 4-7 tok/s | 10-20 tok/s |

**Optimizations**: 4x/8x loop unrolling, AVX2 SIMD, KV cache

## Quick Start

```bash
# 1. Download model (420MB)
bash download_stories110m.sh

# 2. Build
make clean && make && make disk

# 3. Test in QEMU
./test-qemu.sh          # Linux/macOS/WSL
.\test-qemu.ps1         # Windows

# 4. Boot on real hardware (USB)
sudo dd if=qemu-test.img of=/dev/sdX bs=4M
```

## 🎬 Demo

Record your own demo or watch it in action:

```bash
# Record demo (Windows)
.\record-demo.ps1

# Record demo (Linux/macOS)
./record-demo.sh

# Quick 30-second demo
.\record-demo.ps1 -Duration 30
```

See [DEMO_GUIDE.md](DEMO_GUIDE.md) for recording tips and sharing options.

## Interactive Menu

Auto-demo cycles through **6 prompt categories** with **41 prompts**:

- **Stories** (7 prompts): Fairy tales, dragons, princesses
- **Science** (7 prompts): Physics, biology, astronomy  
- **Adventure** (7 prompts): Knights, explorers, pirates
- **Philosophy** (5 prompts): Life, wisdom, happiness
- **History** (5 prompts): Civilizations, inventions
- **Technology** (5 prompts): Computers, AI, robots

**Features**:

- ✅ BPE prompt encoding (understands context!)
- ✅ Temperature-controlled generation
- ✅ 80 tokens per response
- ✅ Auto-progression through categories

**Note**: Keyboard input works only on real UEFI hardware (not in QEMU).

## 🖥️ Hardware Boot (Real Machines!)

Deploy to USB drive and boot on real UEFI hardware:

**Windows**:
```powershell
.\deploy-usb.ps1 -DriveLetter E
```

**Linux**:
```bash
sudo ./deploy-usb.sh /dev/sdX
```

**Requirements**: UEFI firmware, x86-64 CPU with AVX2, 4GB+ RAM, Secure Boot disabled

See [HARDWARE_BOOT.md](HARDWARE_BOOT.md) for complete guide, BIOS settings, and troubleshooting.

## Prerequisites

```bash
# Ubuntu/Debian
sudo apt install build-essential gnu-efi qemu-system-x86 ovmf mtools

# Fedora/RHEL
sudo dnf install gcc binutils gnu-efi-devel qemu-system-x86 edk2-ovmf mtools

# macOS
brew install qemu  # gnu-efi may need manual install
```

## Architecture

**Model**: stories110M (HuggingFace)  
**Layers**: 12 transformer layers, 768 dimensions, 12 attention heads  
**Tokenizer**: BPE with 32K vocabulary  
**Implementation**: Based on [llama2.c](https://github.com/karpathy/llama2.c) (MIT license)  
**Environment**: Bare-metal UEFI via gnu-efi  
**Optimizations**: Loop unrolling (4x/8x), AVX2 SIMD (-mavx2 -mfma)  
**CPU**: Requires Haswell or newer (AVX2 support)

## Files

- `llama2_efi.c` - Main implementation (~1800 lines)
- `Makefile` - Build with AVX2 optimizations
- `download_stories110m.sh` - Model downloader
- `test-qemu.sh` / `test-qemu.ps1` - QEMU test scripts
- `train_stories101m.*` / `train_stories260m.*` - Training scripts

## Training Your Own Model

```bash
# Train stories101M on GPU (~350MB model)
bash train_stories101m.sh

# Train stories260M (requires powerful GPU)
bash train_stories260m.sh
```

Training scripts use TinyStories dataset and export to `.bin` format compatible with the bootloader.

## License

MIT License
