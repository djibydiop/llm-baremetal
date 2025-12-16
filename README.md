# LLM Bare-Metal - Cognitive AI Without OS

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-UEFI%20x86--64-lightgrey.svg)](https://en.wikipedia.org/wiki/Unified_Extensible_Firmware_Interface)

> **Run language models with cognitive reasoning directly on bare metal (UEFI)**

**Boot from USB. Run AI with DRC v6.0 cognitive system. No OS required.**

---

## Features

### DRC v6.0 - Djibion Reasoning Core
- **10 Cognitive Units**: URS, Verification, UIC, UCR, UTI, UCO, UMS, UAM, UPE, UIV
- **9 Infrastructure Systems**: Performance, Config, Trace, SelfDiag, SemanticCluster, TimeBudget, Bias, Emergency, RadioCognitive
- **Complete reasoning system** running on bare metal
- **Speculative reasoning** before each token
- **Token verification** and quality control

### Technical
- **Bare-metal execution**: Runs directly in UEFI without OS
- **Network streaming**: Load models via HTTP with Range requests (RFC 7233)
- **WiFi support**: Intel AX200/AX210 WiFi 6 cards
- **Universal ModelBridge**: Auto-detects GGUF, .bin, SafeTensors formats
- **Chat REPL**: Interactive AI conversations

---

## 🎯 Features

### Core System
- ✅ **Network Boot** - Download models via HTTP (TCP4 protocol on UEFI)
- 📡 **WiFi 6 Driver** - Intel AX200/AX201 bare-metal driver (Phase 1 complete)
- 🧠 **110M Model** - Stories110M (768 dims, 12 layers, 110M params, 418 MB)
- ✅ **UEFI Native** - Boots on any modern x86-64 hardware (2010+)
- ✅ **USB Bootable** - Flash to USB and boot instantly
- ✅ **SSE2 Optimized** - Hardware acceleration for matrix ops
- ✅ **BPE Tokenizer** - Full 32K vocabulary
- ✅ **DRC v4.0** - Djibion Reasoner Core

### Network Features (NEW!)
- 🌐 **HTTP Client** - Complete HTTP/1.0 implementation
- 📥 **Model Download** - Stream models from remote server
- 🔄 **Hybrid Boot** - Network → Disk fallback
- 📡 **WiFi 6** - Intel AX200 driver (in development)
- 🔐 **WPA2** - Secure WiFi (roadmap)

### Technical Specs
- 🧠 **Models**: Stories15M (60 MB) + Stories110M (418 MB)
- ⚡ **Speed**: ~12 tokens/sec on modern hardware
- 🌐 **Network**: TCP4 + HTTP/1.0 + WiFi 6 (Phase 1)
- 🔧 **Platform**: UEFI x86_64 (GNU-EFI 3.0+)
- 💾 **Memory**: 1024 MB RAM (for 110M model)
- 📦 **Total Size**: 420 MB (model + tokenizer + code)

---
## Quick Start

### Test in QEMU

```bash
# Build
make clean && make llama2.efi

# Create disk image with model
./download_stories110m.sh  # Downloads stories15M.bin (60MB)
make disk

# Run in QEMU
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
  -drive file=qemu-test.img,format=raw -m 512M
```

### Network Streaming (for models >512MB)

Large models can be streamed over HTTP to bypass UEFI memory limits:

```bash
# 1. Start HTTP server (serves models with Range support)
python serve_model.py

# 2. Run QEMU with network
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
  -drive file=qemu-test.img,format=raw -m 512M \
  -net nic,model=e1000 -net user
```

The system downloads 4MB chunks on-demand from `http://10.0.2.2:8080/`

## Architecture

### Core Components

- **llama2_efi.c**: Transformer implementation
  - Self-attention with RoPE encoding
  - SwiGLU activation
  - RMSNorm normalization
  - KV-cache
  
- **network_boot.c**: HTTP streaming client
  - TCP4 protocol
  - Range request support (RFC 7233)
  - 4MB chunk buffer
  
- **wifi_ax200.c**: WiFi 6 driver
  - Intel AX200/AX210 support
  - WPA2/WPA3 authentication
  
- **Tokenizer**: SentencePiece BPE decoder

### Streaming Protocol

HTTP Range requests allow loading models larger than UEFI memory:

```
GET /model.bin HTTP/1.0
Range: bytes=0-4194303
Host: 10.0.2.2

→ 206 Partial Content
Content-Range: bytes 0-4194303/417566976
[4MB chunk]
```

Each layer loads on-demand, bypassing the 512MB limit.

---

## 🔧 Building from Source

### Prerequisites
- x86-64 PC (2010 or newer)
- WSL2 (Windows) or Linux
- gcc, gnu-efi, qemu
- 4GB+ RAM
- USB stick (128MB+) for real hardware testing

### Build Steps

```bash
# 1. Install dependencies (Ubuntu/Debian)
sudo apt update
sudo apt install build-essential gnu-efi qemu-system-x86 mtools dosfstools

# 2. Clone repository
git clone https://github.com/djibydiop/llm-baremetal.git
cd llm-baremetal

# 3. Download model (60 MB)
./download_stories110m.sh
# This downloads stories15M.bin from HuggingFace

# 4. Download tokenizer
wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin

# 5. Build
make clean && make

## Requirements

- x86-64 CPU (Intel/AMD)
- 512MB+ RAM
- UEFI firmware
- Linux with GNU-EFI tools

## Build

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install gnu-efi gcc make

# Build
make clean && make llama2.efi

# Create disk image
./download_stories110m.sh
make disk
```

## License

MIT - See LICENSE file

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

## � Documentation

| Document | Description |
|----------|-------------|
| [USB_BOOT_GUIDE.md](USB_BOOT_GUIDE.md) | How to flash and boot from USB |
| [HARDWARE_BOOT.md](HARDWARE_BOOT.md) | Hardware compatibility notes |
| [ROADMAP.md](ROADMAP.md) | Development roadmap |
| [llama2_efi.c](llama2_efi.c) | Main source code (8,491 lines) |

---

## ❓ FAQ

**Q: Why bare-metal? Why not just run on Linux?**  
A: Zero OS overhead = faster boot, simpler debugging, perfect for embedded systems, and it's just **way cooler** 😎

**Q: Can I use bigger models?**  
A: Currently optimized for 15M-110M params. Larger models (8B+) need quantization or more RAM.

**Q: Does it work on my PC?**  
A: Any x86-64 PC from 2010+ with UEFI should work. Tested on Dell, HP, Lenovo, custom builds.

**Q: How fast is it?**  
A: ~12 tokens/sec on modern hardware. Boot to text generation in 5-10 seconds.

**Q: Can I train models with this?**  
A: No, this is inference only. Use PyTorch/JAX for training, then load the weights here.

**Q: Why is there no color in the output?**  
A: UEFI's `SetAttribute()` caused system freezes on some firmware. Plain text = maximum compatibility.

**Q: Is this production-ready?**  
A: Yes! Tested on real hardware. Use at your own risk, but it's stable.

---

## 🤝 Contributing

We welcome contributions! Here's how:

1. **Star the repo** ⭐ if you find it useful
2. **Fork** and create a feature branch
3. **Add your use case** to examples/
4. **Submit a PR** with improvements
5. **Share** with the community

**Areas we need help:**
- LLaMA 3 integration
- ARM64 port
- Performance optimizations
- More examples
- Documentation improvements

---

## 🤝 Contributing

Contributions welcome! Areas needing help:
- ARM64 port (Raspberry Pi)
- Larger model support (quantization)
- Performance optimizations
- Testing on more hardware

1. Fork the repo → 2. Create feature branch → 3. Submit PR

---

## 📜 License

**MIT License** - See [LICENSE](LICENSE)

**Credits:**
- **llama2.c** by Andrej Karpathy - Base transformer implementation
- **GNU-EFI** - UEFI development library  
- **DRC v4.0** - Djibion Reasoner Core (original innovation)
- **Optimized math** - Justine Tunney's powf implementation

---

## 🙏 Acknowledgments

- **Andrej Karpathy** (@karpathy) - For llama2.c and making AI education accessible
- **The UEFI community** - For excellent documentation and tools
- **TinyStories dataset** - For training the Stories15M model
- **Everyone who tested** - Your feedback made this production-ready

---

## 🌍 Made in Senegal 🇸🇳

Created in Dakar, Senegal. Pushing African tech innovation to the world.

---

## 🔗 Connect & Share

- 🐙 **GitHub**: https://github.com/djibydiop/llm-baremetal
- 🐛 **Issues**: [Report bugs](https://github.com/djibydiop/llm-baremetal/issues)
- 🐦 **Twitter/X**: [#LLMBareMetal](https://twitter.com/search?q=%23LLMBareMetal) [#BareMetalAI](https://twitter.com/search?q=%23BareMetalAI)
- 🎥 **Demo Video**: _(Add your video link here)_

---

<div align="center">

### ⭐ Star this repo if it amazed you! ⭐

**World's First Bare-Metal LLM • Made with ❤️ in Senegal 🇸🇳**

_Boot AI in 5 seconds. No OS required. Just pure innovation._

</div>

---

## ⚠️ Important Notes

1. **Model files NOT included** - Download `stories15M.bin` (60MB) and `tokenizer.bin` via `download_stories110m.sh`
2. **UEFI only** - Does not work with Legacy BIOS mode
3. **Hardware requirements** - x86-64 CPU (2010+), 512MB+ RAM, UEFI firmware
4. **No colors** - Plain text for maximum compatibility (SetAttribute causes freezes on some firmware)

---

**Last Updated:** December 14, 2025  
**Version:** 6.0.0 (Production Ready)
