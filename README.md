# LLaMA2 Bare-Metal

A 15M parameter LLaMA2 transformer running directly on UEFI firmware - no OS required.

## Overview

This is a complete LLaMA2 implementation that runs as a bare-metal UEFI application. It loads a 15M parameter model from disk and performs transformer inference directly on firmware, without any operating system.

## Features

- 15M parameters (6 layers, 288 dimensions, 32K vocabulary)
- Complete transformer: RMSnorm, RoPE, multi-head attention, SwiGLU
- Token generation with greedy sampling
- Dynamic memory allocation (~20MB total)
- Chunked file I/O (loads 60MB model in 512KB blocks)
- Compact 65KB executable

## Quick Start

```bash
# Setup (downloads model from HuggingFace)
./setup.sh

# Build
make

# Create bootable disk image
make llama2-disk

# Run in QEMU
make run
```

## Prerequisites

**Linux/macOS/WSL:**
- gcc, binutils (ld, objcopy)
- gnu-efi development headers
- QEMU with OVMF firmware
- mtools (for FAT filesystem)

**Installation:**
```bash
# Ubuntu/Debian
sudo apt install build-essential gnu-efi qemu-system-x86 ovmf mtools

# Fedora
sudo dnf install gcc binutils gnu-efi-devel qemu-system-x86 edk2-ovmf mtools

# macOS (via Homebrew)
brew install qemu
# Note: May need to install gnu-efi from source
```

## Model Weights

The setup script automatically downloads `stories15M.bin` (60MB) from HuggingFace:
https://huggingface.co/karpathy/tinyllamas

Alternatively, download manually and place in the project root.

## Technical Details

**Architecture**: LLaMA2 (Meta)  
**Implementation**: Based on Karpathy's llama2.c (MIT license)  
**Environment**: UEFI firmware via gnu-efi  
**Binary Size**: 65KB  
**Runtime Memory**: ~20MB

## Code Structure

- `llama2_efi.c` - Main implementation (760 lines)
- `Makefile` - Build configuration
- `setup.sh` - Setup and model download script
- `startup.nsh` - UEFI auto-boot script

## How It Works

1. UEFI firmware loads the EFI application
2. Application reads 60MB weights from disk (chunked)
3. Allocates buffers for activations and KV cache
4. Runs transformer forward pass
5. Generates token sequences

## Status

Working: Model loads, forward pass executes, token generation functional  
In Progress: Token quality optimization (better prompting strategies)

## License

MIT License - see source code header for full details
