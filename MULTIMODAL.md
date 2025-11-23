# 🚀 Multimodal LLM Bare-Metal Bootloader

## Architecture Overview

This bootloader supports **3 different transformer architectures** running directly on UEFI firmware without an operating system.

### Supported Models

| Model | Size | Architecture | Use Case |
|-------|------|--------------|----------|
| **stories15M** | 60MB | 6 layers, 288 dim, 32K vocab | Story generation, lightweight |
| **NanoGPT-124M** | 48MB | 12 layers, 768 dim, 50K vocab | GPT-2 text completion |
| **TinyLlama-1.1B-Chat** | 440MB | 22 layers, 2048 dim, 32K vocab | Conversational AI, chat |

## Model Detection & Selection

### Auto-Detection
On boot, the system scans for available models:
```
Scanning for models...

  ✓ [1] stories15M (60MB) - Story generation
  ✓ [2] NanoGPT-124M (48MB) - GPT-2 architecture  
  ✗ [3] TinyLlama-1.1B-Chat (440MB) - Conversational (not found)

Found 2 model(s).
Select model (1-3):
```

### Auto-Select
If only one model is present, it's automatically selected.

### Manual Selection
Multiple models → user chooses via keyboard (1-3).

## Architecture Flexibility

### Dynamic Configuration
```c
typedef enum {
    MODEL_STORIES15M = 1,
    MODEL_NANOGPT = 2,
    MODEL_TINYLLAMA_CHAT = 3
} ModelType;

typedef struct {
    int dim;              // 288 → 2048
    int n_layers;         // 6 → 22
    int n_heads;          // 6 → 32
    int vocab_size;       // 32000 → 50257
    int seq_len;          // 256 → 2048
    ModelType model_type; // Runtime model ID
} Config;
```

### Memory Allocation
Static buffers sized for **largest model** (TinyLlama):
```c
#define MAX_DIM 2048        // TinyLlama dimension
#define MAX_HIDDEN 5632     // TinyLlama FFN hidden
#define MAX_LAYERS 22       // TinyLlama depth
#define MAX_HEADS 32        // TinyLlama attention heads
#define MAX_SEQ_LEN 2048    // TinyLlama context
#define MAX_VOCAB 50257     // GPT-2 vocab (largest)
```

Smaller models use subset of allocated memory.

## Prompt Adaptation

### Story Model (stories15M)
```
Prompts:
  - "Once upon a time"
  - "The little girl"
  - "In the forest"
```

### GPT-2 Model (NanoGPT)
```
Prompts:
  - "The quick brown fox"
  - "In a distant galaxy"
  - "To be or not to be"
```

### Chat Model (TinyLlama-1.1B-Chat)
```
Prompts:
  - "Hello! How are you today?"
  - "What is the capital of France?"
  - "Tell me a joke"
```

## File Structure

```
llm-baremetal/
├── llama2_efi.c         # Main bootloader (multimodal support)
├── Makefile             # Build system
├── stories15M.bin       # 60MB story model (optional)
├── nanogpt.bin          # 48MB GPT-2 model (optional)
├── tinyllama_chat.bin   # 440MB chat model (optional)
├── tokenizer.bin        # Shared tokenizer
└── MULTIMODAL.md        # This file
```

## Building

```bash
# Compile multimodal bootloader
make clean
make

# Create disk image with auto-detection
make llama2-disk
```

## Adding Models

### 1. Convert Model to .bin Format
```bash
# stories15M (15M params)
python export_model.py stories15M.pt

# NanoGPT (124M params)  
python export_model.py nanogpt_124M.pt

# TinyLlama-Chat (1.1B params)
python export_model.py tinyllama_1b_chat.pth
```

### 2. Copy to Boot Disk
```bash
# Copy models to EFI partition
cp stories15M.bin /mnt/efi/
cp nanogpt.bin /mnt/efi/
cp tinyllama_chat.bin /mnt/efi/
```

### 3. Boot
The bootloader will automatically detect available models.

## Performance Characteristics

### stories15M (Fastest)
- **Speed**: ~10 tokens/sec on bare metal
- **Memory**: 60MB model + 50MB runtime
- **Best for**: Quick demos, resource-constrained systems

### NanoGPT-124M (Balanced)
- **Speed**: ~3 tokens/sec on bare metal
- **Memory**: 48MB model + 150MB runtime
- **Best for**: General text generation

### TinyLlama-1.1B-Chat (Powerful)
- **Speed**: ~0.5 tokens/sec on bare metal (no GPU)
- **Memory**: 440MB model + 600MB runtime
- **Best for**: Conversational AI, complex tasks

## Technical Details

### Forward Pass Compatibility
All models use the **same inference engine**:
```c
float* forward(Transformer* t, int token, int pos)
```

Configuration differences handled via `Config` struct.

### Weight Loading
```c
EFI_STATUS load_model(EFI_HANDLE ImageHandle, 
                      EFI_SYSTEM_TABLE *SystemTable,
                      Transformer* transformer, 
                      CHAR16* checkpoint_path)
```

Supports arbitrary model sizes up to MAX_* limits.

### Tokenizer
Shared BPE tokenizer (32K vocab) works for stories15M and TinyLlama.  
GPT-2 uses separate 50K vocab.

## Future Improvements

### Option 2: Tokenizer Enhancement
- [ ] Full BPE encoding (currently BOS only)
- [ ] Multi-vocab support (32K/50K auto-detect)
- [ ] Unicode normalization

### Option 3: Dynamic Prompts
- [ ] Load prompts from `prompts.txt` file
- [ ] User-defined prompt library
- [ ] Prompt templates per model type

### Option 5: Performance Optimization
- [ ] AVX/SSE SIMD for matmul
- [ ] Quantization (INT8/FP16)
- [ ] Flash Attention for TinyLlama
- [ ] KV-cache optimization

## Limitations

### Current
- ⚠️ Keyboard input crashes in QEMU/OVMF (REPL auto-demo mode)
- ⚠️ Simplified tokenization (BOS token only)
- ⚠️ No GPU acceleration (CPU-only inference)

### Hardware Requirements
- **Minimum**: 1GB RAM (stories15M)
- **Recommended**: 2GB RAM (TinyLlama-Chat)
- **UEFI**: x86_64 firmware
- **CPU**: SSE/AVX support (optional but faster)

## Testing

### QEMU (Emulation)
```bash
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
                   -drive format=raw,file=llama2-disk.img \
                   -m 512M
```

### Real Hardware
1. Write image to USB: `dd if=llama2-disk.img of=/dev/sdX bs=4M`
2. Boot from USB
3. **Keyboard input works!** (unlike QEMU)

## Contributing

To add a new model:
1. Update `ModelType` enum in `llama2_efi.c`
2. Add detection logic in `select_model()`
3. Update `MAX_*` constants if needed
4. Add model-specific prompts
5. Test with QEMU + real hardware

## License

MIT License - Based on Andrej Karpathy's llama2.c
