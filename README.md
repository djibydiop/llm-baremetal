# LLM Bare-Metal Kernel

**A UEFI-based bare-metal LLM inference kernel running directly on x86_64 hardware**

Made in Senegal 🇸🇳 by Djiby Diop - December 2025

## 🎯 What is this?

This project runs a **15M parameter language model** (stories15M) directly on bare-metal hardware without any operating system. It boots from USB and generates text at ~1 token/second.

## 📦 Project Structure

```
llm-baremetal/
├── llama2_efi.c           # Main kernel (350KB) - UEFI entry point & inference loop
├── llama2.efi             # Compiled kernel (1MB) - bootable EFI executable
├── llama2_USB.img         # Bootable USB image (200MB)
├── stories15M.bin         # Model weights (58MB, 15M parameters)
├── tokenizer.bin          # Tokenizer vocabulary (424KB, 32K tokens)
├── Makefile               # Build system
├── OVMF.fd                # UEFI firmware for QEMU testing
├── run-qemu.ps1           # QEMU test script
└── deploy-usb.sh          # USB deployment script
```

### Core Components

- **Inference Engine**: `llama2_efi.c` - Complete Transformer implementation
- **Memory Management**: `heap_allocator.h` - 100MB bump allocator
- **Math Optimizations**: `matmul_optimized.h`, `tinyblas.c` - SSE2 matrix ops
- **DRC System**: `drc_v5.c` + modules - Cognitive reasoning layer (10 units)
- **Network (experimental)**: `network_boot.c`, WiFi drivers - For model streaming

### Memory Systems (Advanced Features - Currently Disabled)

19 revolutionary memory systems in `memory_*.h` files:
- Consciousness, Healing, Time-Travel, Quantum, Neural, etc.
- **Status**: Disabled for hardware compatibility
- Future work: Re-enable with proper testing

## 🚀 Quick Start

### 1. Test in QEMU (Recommended)

```powershell
.\run-qemu.ps1
```

Or manually:
```powershell
qemu-system-x86_64 -bios OVMF.fd -drive format=raw,file=llama2_USB.img -m 512M
```

### 2. Flash to USB

**Using Rufus (Windows):**
1. Open Rufus as Administrator
2. Select your USB drive
3. Click "SELECT" → Choose `llama2_USB.img`
4. **Set mode to "DD Image"** (important!)
5. Click "START"

**Using dd (Linux/WSL):**
```bash
sudo dd if=llama2_USB.img of=/dev/sdX bs=4M status=progress
```

### 3. Boot on Real Hardware

1. Insert USB drive
2. Enter BIOS/UEFI boot menu (F12, F11, or DEL)
3. Select USB drive
4. Watch the model generate text!

## 🔧 Build from Source

**Requirements:**
- WSL or Linux
- gcc, GNU-EFI libraries
- make

**Compile:**
```bash
make
```

**Create USB image:**
```bash
./deploy-usb.sh
```

## 📊 Current Status

### ✅ Working
- UEFI boot on real hardware
- Model loading (60MB)
- Tokenizer loading (433KB)
- Token generation (~1 tok/s)
- 100MB heap allocation
- SSE2 optimized matmul
- QEMU emulation

### ⚠️ Known Issues
- **Text display on hardware**: Token generation works but decoded text shows `<unk>`
  - **Debug enabled**: Displays logit values and selected tokens
  - **QEMU works fine**: Same code displays text correctly in emulator
  - **Likely cause**: Hardware-specific issue with forward pass or model loading

### 🔍 Debug Output
```
[DEBUG] Logits check: [0]=-15 [1]=-12 [2]=-18 [3]=-11
[DEBUG] Selected token=2501 maxval=19
[9038] Once upon a time...
```

On real hardware, if you see token 0 (`<unk>`) repeatedly, the debug shows what's wrong.

## 🎛️ Configuration

**Memory:**
- Heap: 100MB (line 7234 in llama2_efi.c)
- Model: 58MB
- KV Cache: ~10MB
- Scratch: ~5MB

**Model:**
- Architecture: Llama2-based Transformer
- Parameters: 15M (6 layers, 288 dims, 6 heads)
- Vocabulary: 32K tokens
- Context: 256 tokens
- Precision: FP32

**Compilation Flags:**
- `-O2`: Optimization level
- `-msse2`: SSE2 only (no AVX2)
- `-fno-asynchronous-unwind-tables`: Smaller binary

## 🧪 Troubleshooting

### Boot fails
- **Check BIOS**: Enable UEFI boot, disable Secure Boot
- **Check USB**: Use DD mode in Rufus, not ISO mode
- **Check drive**: USB must be FAT32 with `/EFI/BOOT/BOOTX64.EFI`

### Generates `<unk>` tokens
- **Check debug output**: Look for logit values
- **If all logits are 0**: Forward pass issue
- **If token always 0**: Sampling issue
- **If logits vary but token 0**: Check argmax logic

### Slow performance
- Normal: ~1 tok/s on 15M model
- Optimize: Enable AVX2 (requires CPU support)
- Future: Quantization to INT8/Q4

## 📖 Technical Details

### Boot Process
1. **UEFI loads** `BOOTX64.EFI` from `/EFI/BOOT/`
2. **Heap init**: 100MB bump allocator
3. **Model load**: Read `stories15M.bin` (60MB)
4. **Tokenizer load**: Read `tokenizer.bin` (433KB)
5. **Generation**: 150 tokens with temperature=0 (greedy)

### Inference Loop
```c
for (int pos = 1; pos < steps; pos++) {
    float* logits = forward(&transformer, token, pos);
    int next = argmax(logits, vocab_size);
    token = next;
    char* piece = decode(vocab, token);
    Print(L"%a", piece);
}
```

### Memory Layout
```
0x00000000 - UEFI firmware
0x15775000 - Heap base (100MB)
  ├─ Model weights (58MB)
  ├─ KV cache (10MB)
  ├─ Scratch buffers (5MB)
  └─ Run state (~5MB)
```

## 🔮 Future Work

1. **Fix hardware text display** - Current priority
2. **Quantization** - INT8/Q4 for faster inference
3. **Larger models** - GPT-2 (500MB), Llama-2 (13GB) via network boot
4. **Chat mode** - REPL with conversation history
5. **Re-enable advanced systems** - 19 memory systems (when stable)

## 📄 License

See LICENSE file

## 🙏 Acknowledgments

- Based on llama2.c by Andrej Karpathy
- GNU-EFI for UEFI development
- Stories15M model for testing

---

**Status**: Functional prototype with ongoing debugging for hardware compatibility
**Last updated**: December 22, 2025
