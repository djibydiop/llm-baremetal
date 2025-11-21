# LLaMA2 Bare-Metal

A 15M parameter LLaMA2 transformer running directly on UEFI firmware - no OS required.

## What This Is

This is a complete LLaMA2 implementation that runs as a bare-metal UEFI application. It loads a 15M parameter model from disk and performs transformer inference directly on firmware.

## Features

✅ **15M parameters** - 6 layers, 288 dimensions, 32K vocabulary  
✅ **Complete forward pass** - Full transformer with RMSnorm, RoPE, multi-head attention, SwiGLU  
✅ **Token generation** - Generates sequences using greedy sampling  
✅ **Dynamic allocation** - Efficient memory usage (~20MB total)  
✅ **Chunked file I/O** - Loads 60MB model in 512KB blocks  
✅ **Minimal binary** - 63KB executable

## Technical Details

**Architecture**: LLaMA2 (Meta)  
**Implementation**: Based on Karpathy's llama2.c reference (MIT license)  
**Environment**: UEFI firmware (gnu-efi)  
**Model**: stories15M.bin (60MB, pre-trained weights)

## Building

### Prerequisites
- WSL with gcc and gnu-efi
- QEMU with OVMF firmware

### Compile
```bash
wsl bash -c "cd /mnt/c/path/to/llm-baremetal && ./build-windows.ps1"
```

### Run
```bash
./run-qemu.ps1
```

## How It Works

1. **Boot**: UEFI firmware loads the EFI application
2. **Load Model**: Reads 60MB weights from disk in chunks
3. **Allocate Buffers**: Dynamic memory for activations and KV cache
4. **Forward Pass**: Complete transformer inference
5. **Generate**: Produces token sequences

## Code Structure

- `llama2_efi.c` - Main implementation (755 lines)
- `build-windows.ps1` - Build script
- `run-qemu.ps1` - QEMU launcher
- `startup.nsh` - UEFI auto-boot script

## Status

✅ **Working**: Model loads, forward pass executes, tokens generate  
⚠️ **Known Issue**: Token quality needs improvement (better prompting)

## Repository

https://github.com/djibydiop/llm-baremetal

## License

MIT (see code header for details)
