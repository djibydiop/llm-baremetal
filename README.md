# LLM Bare-Metal Kernel

**A UEFI-based bare-metal LLM inference kernel running directly on x86_64 hardware**

Made in Senegal 🇸🇳 by Djiby Diop - December 2025

## 🎯 What is this?

This project runs a **15M parameter language model** (stories15M) directly on bare-metal hardware without any operating system. It boots from USB and generates text at ~1 token/second.

## 📦 Project Structure

```
llm-baremetal/
# LLM Bare-Metal (UEFI)

UEFI x86_64 bare-metal LLM inference + chat REPL (stories15M).

Made in Senegal 🇸🇳 by Djiby Diop — December 2025

## ✅ Blessed workflow (stable)

This repo contains many experiments; the following entrypoints are the maintained path.

### Windows (PowerShell)

1) Build + create boot image (uses WSL):

```powershell
./build.ps1
```

2) Run in QEMU (OVMF):

```powershell
./run.ps1
```

### WSL / Linux

```bash
./build-image-wsl.sh
```

This produces:
- `llama2.efi` (UEFI application)
- `llm-baremetal-boot.img` (GPT + FAT32 image with `/EFI/BOOT/BOOTX64.EFI`)

## 📦 Key files

```
llm-baremetal/
├── llama2_efi_final.c      # Main REPL/kernel source (default build)
├── Makefile                # Canonical GNU-EFI build (PE32+)
├── create-boot-mtools.sh   # Image builder (mtools; no sudo mounts)
├── build-image-wsl.sh      # One-command WSL build + image
├── build.ps1               # Windows entrypoint (calls WSL build)
├── run.ps1                 # Windows entrypoint (runs QEMU + OVMF)
├── stories15M.bin          # Model weights
└── tokenizer.bin           # Tokenizer vocab
```

## 🔧 Requirements (for the blessed path)

- Windows + WSL2
- In WSL: `gcc`, `make`, `gnu-efi`, `mtools`, `parted`
- On Windows: QEMU installed at `C:\Program Files\qemu\...` (see `run.ps1`)

## 🧪 USB boot

Flash `llm-baremetal-boot.img` with Rufus:
- Partition scheme: GPT
- Target system: UEFI (non-CSM)
- Write mode: DD Image

## 🧹 Legacy / experimental scripts

Older build/run/image scripts are still kept for reference but are not the recommended path.
See LEGACY.md for a quick map.
- make
