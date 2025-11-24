# 🚀 LLM Bare-Metal Inference Engine

**Run large language models directly on UEFI firmware - no operating system required.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: UEFI](https://img.shields.io/badge/Platform-UEFI-blue.svg)](https://uefi.org/)
[![Language: C](https://img.shields.io/badge/Language-C-green.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

## 🎯 What is this?

A fully functional **110M parameter transformer model** running on **bare UEFI firmware** without any operating system. Boot from USB, generate text instantly.

**Key Innovation**: BPE tokenization + Transformer inference + UEFI APIs = OS-free LLM

## ✨ Features

### Core Capabilities
- 🧠 **stories110M**: 110M parameters, 12 layers, 768 dimensions, 12 attention heads
- 🔤 **BPE Tokenizer**: Greedy longest-match encoding, 32K vocabulary
- ⚡ **AVX2 Optimized**: SIMD instructions, 4x/8x loop unrolling, FMA
- 🎯 **41 Prompts**: Across 6 categories (Stories, Science, Adventure, Philosophy, History, Tech)
- 💾 **Persistent Storage**: Auto-save generations to disk
- 🔄 **Multi-Model Support**: Detect and select stories15M/110M/llama2_7b
- 🚀 **Bare-Metal**: Zero OS overhead, pure UEFI firmware

### Performance

| Model | Size | Architecture | QEMU (WSL) | Hardware (est.) |
|-------|------|--------------|------------|-----------------|
| **stories15M** | 60MB | 6L/288D/6H | ~15 tok/s | ~30 tok/s |
| **stories110M** | 420MB | 12L/768D/12H | ~5 tok/s | ~15 tok/s |
| **llama2_7b** | 13GB | 32L/4096D/32H | ~0.5 tok/s | ~2 tok/s |

**Optimizations**: KV cache, loop unrolling, AVX2 SIMD (-mavx2 -mfma)

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

---

## 📚 Documentation

- **[ROADMAP.md](ROADMAP.md)** - Future features and development plan
- **[PHASE_8_9_REPORT.md](PHASE_8_9_REPORT.md)** - Multi-model + Persistent storage implementation
- **[HARDWARE_BOOT.md](HARDWARE_BOOT.md)** - Complete USB boot guide

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
