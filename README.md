# LLM Bare-Metal Kernel

**A UEFI-based bare-metal LLM inference kernel running directly on x86_64 hardware**

Made in Senegal 🇸🇳 by Djiby Diop - December 2025

## 🎯 What is this?

This project runs **TinyStories language models** (15M-300M parameters) directly on bare-metal hardware without any operating system. It boots from USB and features:
- **Multi-model support**: Auto-detects stories300M/260M/200M/110M/15M/model.bin
- **Persistent KV cache**: Context retained across prompts for conversation flow
- **Multi-line prompts**: Use `\` to continue lines, `;;` to submit
- **SIMD acceleration**: AVX2+FMA on compatible CPUs (~26-32 tok/s on i5-8250U)
- **LLM-Kernel diagnostics**: Zone allocator, cycle budgets, sentinel fail-safe

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

Optional:
- `repl.cfg` (key=value config for REPL defaults). If present in `llm-baremetal/`, the image builder copies it to the image root.

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

Optional:
├── repl.cfg.example        # Template config (copy to repl.cfg and tune)
```

## ⚙️ Configuration (optional)

Copy `repl.cfg.example` to `repl.cfg` and adjust values:
```bash
cp repl.cfg.example repl.cfg
# Edit repl.cfg with your preferred sampling/budgets/attn settings
```

The build script auto-includes `repl.cfg` in the boot image if present. Settings are loaded at boot before the REPL prompt.

## 🎮 REPL Commands

Once booted, the interactive REPL supports:

**Sampling:**
- `/temp <val>` - Temperature (0.0=greedy, 1.0=creative)
- `/min_p <val>` - Min-p threshold (0.0-1.0)
- `/top_p <val>` - Nucleus sampling (0.0-1.0)
- `/top_k <int>` - Top-k sampling (0=off)
- `/norepeat <n>` - No-repeat ngram size (0=off, typical 3-6)
- `/repeat <val>` - Repetition penalty (1.0=none, 1.5=strong)
- `/max_tokens <n>` - Max generation tokens (1-256)
- `/seed <n>` - Set RNG seed

**Diagnostics:**
- `/model` - Show loaded model config
- `/cpu` - Show CPU SIMD status (djiblas_sgemm + attn_simd)
- `/zones` - Dump allocator zones + sentinel status
- `/ctx` - Show model + sampling + budgets
- `/log [n]` - Dump last n log entries (default 16, max 128)
- `/save_log [n]` - Write last n log entries to `llmk-log.txt`
- `/save_dump` - Write ctx+zones+sentinel+log to `llmk-dump.txt`
- `/reset` - Clear budgets/log + untrip sentinel

**Performance:**
- `/budget [p] [d]` - Set budgets in cycles (p=prefill, d=decode)
- `/attn [auto|sse2|avx2]` - Force attention SIMD path
- `/test_failsafe [prefill|decode|both] [cycles]` - One-shot strict budget trip test

**Context & UI:**
- `/stats <0|1>` - Toggle generation stats
- `/stop_you <0|1>` - Toggle stop on `\nYou:` pattern
- `/stop_nl <0|1>` - Toggle stop on double newline
- `/clear` - Clear KV cache (reset conversation context)
- `/version` - Show build info and features
- `/help` - Show help

**Multi-line prompts:**
- End line with `\` to continue on next line
- Type `;;` on a line by itself to submit multi-line prompt

**Files written to USB (after commands):**
- `llmk-log.txt` - Ring log entries (`/save_log`)
- `llmk-dump.txt` - Full diagnostic dump (`/save_dump`)
- `llmk-failsafe.txt` - Auto-written on sentinel trip (budget overrun)

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
