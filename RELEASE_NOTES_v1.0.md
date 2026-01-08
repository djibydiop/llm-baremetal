# LLM Bare-Metal v1.0 - UEFI x86_64 Inference Kernel

**A 110M-parameter language model running directly on bare-metal x86_64 hardware without any operating system.**

Made in Senegal 🇸🇳 by Djiby Diop - January 2026

---

## 🎯 What is this?

This project boots a **TinyStories 110M model** directly from USB on UEFI x86_64 systems and provides an interactive chat REPL with **~26-32 tokens/second** generation on modern hardware (AVX2+FMA).

No OS. No kernel. Just UEFI firmware, a language model, and you.

---

## ✨ Features

### Core
- **Bare-metal inference**: Runs directly on UEFI firmware (GNU-EFI)
- **DjibMark tracing**: Omnipresent execution profiling (0xD31B2026 signature) 🇸🇳
- **Optimized SIMD**: Runtime dispatch (SSE2 baseline, AVX2+FMA when available)
  - DjibLAS SGEMM with AVX2+FMA (matrix multiply)
  - AVX2 attention helpers with FMA fused ops
- **Interactive REPL**: Full chat loop with 20+ commands
- **UTF-8 streaming**: Proper Unicode output + mojibake repair
- **LLM-Kernel primitives**:
  - Zone allocator (WEIGHTS/KV/SCRATCH/ACTS/ZONEC)
  - Sentinel with per-phase cycle budgets (prefill/decode)
  - Post-mortem ring log (128 entries)

### Diagnostics & Persistence (NEW in v1.0)
- **Persistent dumps**: Write diagnostics to FAT volume
  - `/save_log [n]` → `llmk-log.txt` (ring log entries)
  - `/save_dump` → `llmk-dump.txt` (full system state)
  - `llmk-failsafe.txt` (auto-written on budget overrun)
- **File persistence**: Explicit `Flush()` ensures writes survive hard reset
- **Optional config**: `repl.cfg` for boot-time defaults (sampling, budgets, SIMD mode)

### REPL Commands
**Sampling:** `/temp`, `/min_p`, `/top_p`, `/top_k`, `/norepeat`, `/repeat`, `/max_tokens`, `/seed`  
**Diagnostics:** `/model`, `/cpu`, `/zones`, `/ctx`, `/log [n]`, `/save_log [n]`, `/save_dump`, `/djibmarks`, `/djibperf`  
**Performance:** `/budget [p] [d]`, `/attn [auto|sse2|avx2]`, `/test_failsafe`  
**Context:** `/clear` (reset KV cache), `/reset` (clear budgets/log)  
**Other:** `/stats`, `/stop_you`, `/stop_nl`, `/version`, `/help`

---

## 📦 What's Included

### Download
- **`llm-baremetal-boot.img`** (499MB) - Ready-to-flash bootable image
  - TinyStories 110M model (stories110M.bin)
  - Tokenizer (32k vocab)
  - Pre-configured for immediate boot

### Source
- Full UEFI application source (`llama2_efi_final.c`)
- LLM-Kernel primitives (zones, sentinel, log)
- DjibLAS optimized BLAS (AVX2+FMA)
- Build scripts (WSL + PowerShell)

---

## 🚀 Quick Start

### Flash to USB (Windows + Rufus)
1. Download `llm-baremetal-boot.img`
2. Open **Rufus**
3. Select your USB drive
4. Click **SELECT** → choose `llm-baremetal-boot.img`
5. Settings:
   - **Partition scheme:** GPT
   - **Target system:** UEFI (non-CSM)
   - **Write mode:** DD Image
6. Click **START**

### Boot
1. Insert USB and reboot
2. Enter UEFI boot menu (F12/F11/Del depending on system)
3. Select USB drive (UEFI boot entry)
4. Wait ~30s for model load
5. Chat at the `You:` prompt!

### Example Session
```
You: /cpu
CPU features:
  sse2=1 avx=1 avx2=1 fma=1
  djiblas_sgemm=AVX2+FMA
  attn_simd=AVX2

You: once upon a time
AI: there was a little girl named Lily. She loved to play outside...
[stats] tokens=160 time_ms=5000 tok_s=32.000
```

---

## ⚙️ Configuration (Optional)

Copy `repl.cfg.example` to `repl.cfg` and rebuild the image to set boot-time defaults:

```bash
# Example: performance-tuned config
temperature=0.75
top_p=0.95
top_k=80
attn=auto
budget_prefill=80000000000
budget_decode=60000000000
strict_budget=0
```

Then rebuild:
```powershell
./build.ps1
```

The new image will include your config and load it at boot.

---

## 📊 Performance

**Tested on:** Intel i5-8250U (Kaby Lake R, 4C/8T, AVX2+FMA)

| Metric | Value |
|--------|-------|
| Generation speed | **26-32 tok/s** |
| Model size | 110M parameters (419MB weights) |
| Boot time | ~30s (model load) |
| Memory usage | 768MB total (WEIGHTS: 418MB, KV: 72MB, ACTS: 20MB) |
| SIMD path | AVX2+FMA (auto-detected) |

**On older hardware (SSE2 only):** expect ~5-10 tok/s.

---

## 🧪 Diagnostics & Troubleshooting

### Persist Diagnostics to USB
After boot, run:
```
/save_dump
```

Then reboot and mount the USB to read `llmk-dump.txt`:
```bash
# Windows WSL
sudo mount -o loop,offset=1048576 llm-baremetal-boot.img /mnt/usb
cat /mnt/usb/llmk-dump.txt
```

### Check CPU Features
```
/cpu
```
Verify `djiblas_sgemm=AVX2+FMA` and `attn_simd=AVX2` for optimal performance.

### Monitor Budgets
```
/ctx
```
If you see `overruns(p=X d=Y)` growing, increase budgets:
```
/budget 100000000000 80000000000
```

---

## 🛠️ Build from Source

### Requirements
- **Windows + WSL2** (or Linux)
- **In WSL:** gcc, make, gnu-efi, mtools, parted
- **On Windows:** QEMU (for testing)

### Build
```powershell
# Clone repo
git clone https://github.com/djibydiop/llm-baremetal.git
cd llm-baremetal

# Build + create bootable image
./build.ps1

# Test in QEMU
./run.ps1
```

Output: `llm-baremetal-boot.img` (ready to flash)

---

## 📚 Documentation

- **README.md** - Quick start + build instructions
- **repl.cfg.example** - Config template with comments
- Full REPL command reference in README

---

## 🐛 Known Limitations

- **Model size:** Max ~500MB (FAT partition size)
- **Sequence length:** Fixed at compile time (1024 tokens for stories110M)
- **No persistent KV cache:** Each prompt starts fresh
- **Budget tuning:** Cycle budgets are machine-specific; start with defaults and adjust via `/ctx`

---

## 🙏 Acknowledgments

- **llama2.c** by Andrej Karpathy - Inspiration and model architecture
- **TinyStories** dataset - Training corpus for small models
- **GNU-EFI** - UEFI development framework

---

## 📄 License

MIT License - See LICENSE file

---

## 🇸🇳 Made in Senegal

Built with passion in Dakar.

Questions? Open an issue on GitHub!

---

**Release Date:** January 8, 2026  
**Commit:** `aa59959`  
**Tag:** `v1.0.0`

---

## 🇸🇳 Technical Signature: DjibMark

Every execution path is marked with **DjibMark** (`0xD31B2026` = DJIB + 2026), a lightweight tracing system that provides:
- **Ring buffer**: 256 marks with TSC timestamps
- **Phase tracking**: BOOT → PREFILL → DECODE → REPL
- **Zero overhead**: Inline recording (24 bytes/mark)
- **Introspection**: `/djibmarks` shows trace, `/djibperf` analyzes performance

This omnipresent signature makes the codebase instantly recognizable and enables deep runtime analysis without external tools.
