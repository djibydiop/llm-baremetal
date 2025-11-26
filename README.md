# LLM Bare-Metal UEFI Bootloader

Run Large Language Models directly on bare-metal hardware without an operating system.

A lightweight UEFI bootloader that boots directly into a transformer inference engine. Features automatic hardware detection, multi-model support, and AVX2 SIMD optimization.

## Features

- OS-free LLM inference - boot directly into AI without Windows/Linux
- UEFI bootloader - works on modern x86-64 hardware
- USB bootable - deploy to USB drives for portable AI
- AVX2/FMA SIMD optimization - hardware-accelerated matrix operations (2-3x speedup)
- Multi-model support - automatically detects and selects optimal model (15M-7B parameters)
- Hardware auto-detection - RAM capacity and CPU feature detection
- Interactive prompt library - 53 prompts across 8 categories
- BPE tokenization - full text encoding/decoding support
- Precise performance measurement - tokens/second, first token latency, timing with EFI Runtime Services

---

## 📦 Quick Start

### Prerequisites

- **Hardware**: x86-64 CPU with AVX2 support, 4GB+ RAM
- **Build Environment**: Linux/WSL with GCC and GNU-EFI
- **Tools**: `make`, `gcc`, `objcopy`

### 1. Clone Repository

```bash
git clone https://github.com/djibydiop/llm-baremetal.git
cd llm-baremetal
```

### 2. Download Model Files

Models are not included in the repository due to size. Download from HuggingFace:

```bash
# Recommended: stories15M (60MB) - fast, reliable for USB boot
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin

# Alternative: stories42M (165MB) - better quality
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories42M.bin

# Tokenizer (required, 434KB)
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/tokenizer.bin
```

For automated setup, use the provided script:
```bash
./download_models.sh
```

### 3. Build UEFI Bootloader

```bash
make clean && make
```

This produces:
- `llama2.efi` - UEFI bootloader with LLM inference

### 4. Deploy to USB (Windows)

```powershell
# Format USB as FAT32 (drive letter E: in this example)
# Then run deployment script:
.\deploy-usb.ps1 -DriveLetter E
```

### 5. Deploy to USB (Linux)

```bash
sudo ./deploy-usb.sh /dev/sdX  # Replace sdX with your USB device
```

See `USB_BOOT_GUIDE.md` for detailed USB deployment instructions.

---

## 📂 Repository Structure

```
llm-baremetal/
├── llama2_efi.c          # Main UEFI bootloader source (2200 lines)
├── Makefile              # Build configuration
├── deploy-usb.ps1        # USB deployment script (Windows)
├── deploy-usb.sh         # USB deployment script (Linux)
├── download_stories110m.sh  # Model download helper
├── .gitignore            # Git ignore rules
├── LICENSE               # MIT License
├── README.md             # This file
├── ROADMAP.md            # Development roadmap
├── USB_BOOT_GUIDE.md     # USB boot instructions
├── HARDWARE_BOOT.md      # Hardware compatibility guide
│
└── Files to download separately:
    ├── stories15M.bin       # 15M param model (60MB) - RECOMMENDED
    ├── stories110M.bin      # 110M param model (420MB)
    └── tokenizer.bin        # BPE tokenizer (434KB)
```

---

## Supported Models

All models available at: https://huggingface.co/karpathy/tinyllamas

| Model | Size | Parameters | RAM Required | Notes |
|-------|------|------------|--------------|-------|
| stories15M | 60MB | 15M | 512MB+ | Recommended for USB boot |
| stories42M | 165MB | 42M | 1GB+ | Better quality, longer load time |
| stories110M | 420MB | 110M | 2GB+ | Best quality for TinyStories |

The bootloader automatically detects available models on the boot disk and selects the largest one that fits in available RAM (60% threshold).

## Performance Metrics

The system provides detailed real-time performance measurement:

- **Tokens per second** - Actual generation speed measured with EFI Runtime Services
- **First token latency** - Time from prompt submission to first output token
- **Total generation time** - Complete inference duration in seconds.milliseconds
- **Forward pass count** - Number of transformer passes for analysis

Expected performance (real hardware with AVX2):
- stories15M: 20-40 tokens/second
- stories42M: 10-20 tokens/second
- stories110M: 5-10 tokens/second

Note: QEMU emulation is 10-20x slower than real hardware.

## Interactive Prompt Library

The system includes 53 pre-configured prompts across 8 categories:

- Stories (7 prompts) - Fairy tales, dragons, princesses
- Science (7 prompts) - Water cycle, gravity, solar system
- Adventure (7 prompts) - Knights, explorers, pirates
- Philosophy (5 prompts) - Meaning of life, happiness
- History (5 prompts) - Ancient civilizations, inventions
- Technology (5 prompts) - Computers, internet, AI
- Jokes (6 prompts) - Humor and funny scenarios
- Coding (6 prompts) - Programming concepts

Select a category and prompt to generate text. The system tracks conversation context and automatically resets when the context window fills.

---

## Hardware Requirements

### CPU
- x86-64 architecture
- AVX2 support (Intel Haswell 2013+, AMD Excavator 2015+)
- FMA3 support (recommended for optimal performance)

### Memory
- Minimum: 512MB RAM (stories15M model)
- Recommended: 1GB+ RAM (stories42M model)
- 2GB+ for stories110M model

### Firmware
- UEFI boot support (not Legacy BIOS)
- Secure Boot must be disabled
- Fast Boot disabled (recommended for debugging)

### Storage
- USB 2.0+ drive (for USB boot)
- 200MB+ free space for bootloader and models

---

## Testing with QEMU

You can test the bootloader in QEMU without physical hardware:

```bash
# Linux/WSL
qemu-system-x86_64 -bios /usr/share/qemu/OVMF.fd \
  -drive format=raw,file=usb.img \
  -cpu Haswell -m 1024 -nographic

# macOS (with QEMU installed via Homebrew)
qemu-system-x86_64 -bios /usr/local/share/qemu/edk2-x86_64-code.fd \
  -drive format=raw,file=usb.img \
  -cpu Haswell -m 1024 -nographic
```

Note: QEMU emulation is significantly slower than real hardware. Expect 10-20x slower inference speed.

## Troubleshooting

### Boot stops at "127580KB read"
**Solution:** Model file too large for your UEFI firmware. Use `stories15M.bin` (60MB) instead of `stories110M.bin` (420MB).

### "No bootable device" error
**Solution:** 
1. Ensure USB is FAT32 formatted
2. Check UEFI boot mode (not Legacy)
3. Disable Secure Boot in BIOS

### "AVX2 not supported" error
**Solution:** Your CPU doesn't support AVX2. Minimum requirement is Intel Haswell (2013) or AMD Excavator (2015).

### Slow inference (< 10 tokens/sec)
**Solution:**
1. Enable AVX2 in BIOS (if available)
2. Use smaller model (stories15M vs stories110M)
3. Increase RAM allocation in QEMU/VM

## Documentation

- [USB_BOOT_GUIDE.md](USB_BOOT_GUIDE.md) - Detailed USB deployment instructions
- [HARDWARE_BOOT.md](HARDWARE_BOOT.md) - Hardware compatibility and testing
- [ROADMAP.md](ROADMAP.md) - Development roadmap and future features
- [HTTP_IMPLEMENTATION_GUIDE.md](HTTP_IMPLEMENTATION_GUIDE.md) - Network download implementation
- [MULTI_MODEL_SUPPORT.md](MULTI_MODEL_SUPPORT.md) - Multi-model architecture details

## Contributing

Contributions are welcome. Please follow standard GitHub workflow:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

MIT License - see LICENSE file for details.

Based on llama2.c by Andrej Karpathy (https://github.com/karpathy/llama2.c)

## 🙏 Credits

- **Andrej Karpathy** - Original llama2.c implementation
- **Meta AI** - LLaMA2 model architecture
- **TinyStories Dataset** - Training data for stories models
- **GNU-EFI** - UEFI development libraries

## Important Notes

- Model files are not included in the repository. Use download_models.sh or manually download from HuggingFace.
- For reliable USB boot, use stories15M.bin (60MB). Larger models may timeout on some UEFI firmware.
- Requires UEFI boot mode. Legacy BIOS is not supported.
- CPU must support AVX2 instructions (Intel Haswell 2013+ or AMD Excavator 2015+).

## Project Information

GitHub: https://github.com/djibydiop/llm-baremetal
Issues: https://github.com/djibydiop/llm-baremetal/issues
Version: 2.0.0
