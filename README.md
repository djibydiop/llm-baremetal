# LLM Bare-Metal UEFI Bootloader

🚀 **Run Large Language Models directly on bare-metal hardware without an operating system**

A lightweight UEFI bootloader that loads and runs transformer models (LLaMA2, TinyLlama) with optimized inference on x86-64 hardware.

---

## 🎯 Features

- ✅ **OS-Free LLM Inference** - Boot directly into AI without Windows/Linux
- ✅ **UEFI Boot** - Works on modern PC hardware (2010+)
- ✅ **USB Bootable** - Create bootable USB drives
- ✅ **AVX2/FMA Optimized** - Hardware SIMD acceleration
- ✅ **Multiple Models** - Support for 15M to 110M parameter models
- ✅ **Interactive Prompts** - 41 pre-configured prompts across 6 categories
- ✅ **BPE Tokenization** - Full text encoding/decoding

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

Download the model and tokenizer (not included in repo due to size):

```bash
# Option 1: stories15M (60MB) - RECOMMENDED for USB boot
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin

# Option 2: stories110M (420MB) - Larger, may timeout on some UEFI
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin

# Tokenizer (required)
wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin
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

## 🔧 Supported Models

| Model | Size | Parameters | Recommended Use |
|-------|------|------------|-----------------|
| **stories15M** | 60MB | 15M | ✅ USB boot (fast, reliable) |
| stories110M | 420MB | 110M | Desktop/VM (larger context) |
| llama2_7b | 13GB | 7B | High-memory systems only |

**Note:** `stories15M.bin` is recommended for USB boot as it loads completely without timeout issues.

---

## 🎮 Interactive Menu

After booting, you'll see an interactive menu with 6 categories:

1. **Stories** (7 prompts) - Fairy tales, dragons, princesses
2. **Science** (7 prompts) - Water cycle, gravity, solar system
3. **Adventure** (7 prompts) - Knights, explorers, pirates
4. **Philosophy** (5 prompts) - Meaning of life, happiness
5. **History** (5 prompts) - Ancient civilizations, inventions
6. **Technology** (5 prompts) - Computers, internet, AI

Select a category, then choose a prompt to generate text.

---

## 🚀 Hardware Requirements

### Minimum
- x86-64 CPU with AVX2 support (Intel Haswell 2013+, AMD Excavator 2015+)
- 4GB RAM
- UEFI firmware
- USB 2.0+ (for USB boot)

### Recommended
- Intel Core i5/i7 (4th gen+) or AMD Ryzen
- 8GB RAM
- USB 3.0+

### BIOS Settings
- **UEFI boot mode** (not Legacy/CSM)
- **Secure Boot disabled**
- **Fast Boot disabled** (optional, helps with debugging)

---

## 🐛 Troubleshooting

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

---

## 📚 Documentation

- **[USB_BOOT_GUIDE.md](USB_BOOT_GUIDE.md)** - Detailed USB deployment guide
- **[HARDWARE_BOOT.md](HARDWARE_BOOT.md)** - Hardware compatibility and testing
- **[ROADMAP.md](ROADMAP.md)** - Development roadmap and future features

---

## 🤝 Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## 📜 License

MIT License - see [LICENSE](LICENSE) file for details.

Based on [llama2.c](https://github.com/karpathy/llama2.c) by Andrej Karpathy.

---

## 🙏 Credits

- **Andrej Karpathy** - Original llama2.c implementation
- **Meta AI** - LLaMA2 model architecture
- **TinyStories Dataset** - Training data for stories models
- **GNU-EFI** - UEFI development libraries

---

## 📞 Contact

- **GitHub Issues**: [Report bugs or request features](https://github.com/djibydiop/llm-baremetal/issues)
- **Repository**: https://github.com/djibydiop/llm-baremetal

---

## ⚠️ Important Notes

1. **Model files not included** - Download `stories15M.bin` and `tokenizer.bin` separately (see Quick Start)
2. **USB boot timeout fix** - Use `stories15M.bin` (60MB) instead of `stories110M.bin` (420MB) for reliable USB boot
3. **UEFI only** - Does not work with Legacy BIOS
4. **AVX2 required** - CPU must support AVX2 instructions

---

**Last Updated:** November 24, 2025  
**Version:** 1.0.0
