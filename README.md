# LLaMA2 Bare-Metal

**15M parameter LLaMA2 transformer running directly on UEFI firmware - no OS required.**

## Features

- 🚀 Bare-metal UEFI implementation (no operating system)
- 🧠 15M parameters (6 layers, 288 dim, 32K vocab)
- 🎯 Temperature sampling for quality text generation
- 📦 ~70KB binary, ~62MB runtime memory
- ⚡ ~2 tokens/second inference speed

## Quick Start

```bash
# Download model (60MB from HuggingFace)
./setup.sh

# Build and test
make
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
**Binary Size**: ~70KB  
**Runtime Memory**: ~20MB  
**Generation**: 200 tokens with temperature=0.9

## Code Structure

- `llama2_efi.c` - Main implementation (932 lines)
  - Transformer inference engine
  - BPE tokenizer loader
  - Temperature sampling implementation
- `Makefile` - Build configuration
- `setup.sh` - Setup and model download script
- `startup.nsh` - UEFI auto-boot script

## How It Works

1. UEFI firmware loads the EFI application
2. Application reads model weights (60MB) and tokenizer (424KB) from disk
3. Allocates buffers for activations and KV cache
4. Runs transformer forward pass for each token
5. Applies temperature sampling for next token selection
6. Decodes token IDs to readable text using BPE vocabulary
7. Generates 200-token sequences

## Example Output

```text
Once upon a time, there was a little girl named Lily. She loved to play in the park
with her friends. One day, Lily saw a big red ball bouncing down the hill...
```

The model generates coherent short stories using temperature sampling (temp=0.9) for natural, varied text.

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
