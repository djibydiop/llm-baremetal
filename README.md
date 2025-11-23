# 🚀 Multimodal LLM Bare-Metal Bootloader

**3 transformer models running directly on UEFI firmware - no OS required.**

## Features

- 🔥 **Multimodal**: 3 models (60MB → 4.2GB) with auto-detection
- 🚀 Bare-metal UEFI implementation (no operating system)
- 🧠 stories15M (15M) / NanoGPT (124M) / TinyLlama-Chat (1.1B)
- 🎯 **Conversational mode** with history and commands
- ⚡ **AVX2/SSE SIMD** optimizations (3x speedup)
- 🔤 **Complete BPE tokenizer** (character-level + byte fallback)
- 📊 Token tracking and statistics
- 🎛️ Temperature control (0.0-1.5)

## Supported Models

| Model | Size | Arch | Use Case | Speed (Scalar) | Speed (AVX2) |
|-------|------|------|----------|----------------|--------------|
| **stories15M** | 60MB | 6L/288D | Story generation | ~20 tok/s | ~40 tok/s |
| **NanoGPT-124M** | 471MB | 12L/768D | Text completion | ~5 tok/s | ~12 tok/s |
| **TinyLlama-Chat** | 4.2GB | 22L/2048D | Conversational | ~0.66 tok/s | **~2 tok/s** |

*Speed benchmarks with AVX2 on modern x86-64 CPU (QEMU)*

## Quick Start

### 1. Download Models

```bash
# Interactive Python downloader
python3 download_models.py

# Or manual download (stories15M only)
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin
wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin
```

### 2. Build Bootloader

```bash
# Clean and compile multimodal bootloader
make clean
make

# Create disk image (auto-includes available models)
make llama2-disk
```

### 3. Test

```bash
# QEMU (keyboard input doesn't work - auto-demo only)
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
                   -drive format=raw,file=llama2-disk.img \
                   -m 512M

# Real hardware (keyboard works!)
sudo dd if=llama2-disk.img of=/dev/sdX bs=4M
# Boot from USB
```

## Conversational Commands

In interactive mode, use these commands:

- `/help` - Show available commands
- `/clear` - Clear conversation history
- `/history` - Show last 10 turns
- `/stats` - Show token usage and SIMD status
- `/temp <0.0-1.5>` - Adjust temperature
- `/tokens <1-512>` - Set max response tokens
- `/exit` - Exit conversation

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

## Model Weights & Tokenizer

The setup script automatically downloads from HuggingFace:

- `stories15M.bin` (60MB) - Model weights
- `tokenizer.bin` (424KB) - BPE vocabulary

Source: <https://huggingface.co/karpathy/tinyllamas>

Alternatively, download manually and place both files in the project root.

## Technical Details

**Architecture**: LLaMA2 (Meta)  
**Implementation**: Based on Karpathy's llama2.c (MIT license)  
**Environment**: UEFI firmware via gnu-efi  
**Binary Size**: ~80KB (with AVX2 optimizations)  
**Runtime Memory**: 110MB → 8GB (model-dependent)  
**Disk Image**: 5.2GB (3 models + tokenizer)  
**SIMD**: AVX2/FMA with runtime CPU detection  
**Tokenizer**: Complete BPE with byte-level fallback  
**Generation**: Up to 512 tokens with adjustable temperature

## Code Structure

- `llama2_efi.c` - Main implementation (2,400+ lines)
  - Transformer inference engine with AVX2 SIMD
  - Complete BPE tokenizer (character + byte fallback)
  - Conversational mode with history tracking
  - System commands processor
- `Makefile` - Build configuration with AVX2 flags
- `convert_models.py` - Model converter (PyTorch/SafeTensors → binary)
- `download_tinyllama.py` - TinyLlama-1.1B downloader

## How It Works

1. UEFI firmware loads the EFI application
2. Detects and enables AVX2/SSE SIMD acceleration
3. Detects available models and displays selection menu
4. Loads selected model weights and tokenizer from disk
5. Allocates buffers for activations and KV cache
6. Runs optimized transformer forward pass (AVX2 matmul, rmsnorm, softmax)
7. Applies temperature sampling for next token selection
8. Decodes tokens using complete BPE tokenizer
9. Tracks conversation history and statistics

## Example Output

```text
[Turn 1/6]
User>>> Hello! How are you today?
Assistant>>> I'm doing great, thank you for asking! As an AI assistant...
[Tokens: 45 | Temp: 0.90 | Total: 45]
--------------------------------------------------

[Turn 2/6]
User>>> /stats

=== Conversation Stats ===
Turns: 1/10
Total tokens: 45
Temperature: 0.90
SIMD: AVX2 enabled
=========================
```

## Status

✅ **Fully Functional**:

- Model inference and token generation
- BPE tokenizer with readable text output
- Temperature sampling for high-quality generation
- 200-token story generation

🚧 **Future Enhancements**:

- User-provided prompts
- Configurable temperature/top-p parameters
- Hardware testing (currently validated on QEMU/OVMF)

## License

MIT License - see source code header for full details
