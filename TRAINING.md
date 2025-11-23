# 🎓 Model Training Guide - Multimodal LLM Bootloader

## Quick Start

### Option 1: Download Pre-trained Models (Recommended)

```bash
# Download all models automatically
python3 download_models.py

# Or manually:
# stories15M (60MB) - Ready to use
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin

# Tokenizer (required)
wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin
```

### Option 2: Use Training Script (Linux/WSL)

```bash
# Interactive training menu
./train_models.sh

# Options:
# 1. Download stories15M (fastest)
# 2. Download + convert NanoGPT-124M
# 3. Download + convert TinyLlama-Chat
# 4. Train custom stories15M from scratch
# 5. Fine-tune NanoGPT
# 6. Download all
```

## Model Details

### 1️⃣ stories15M (60MB) ✅ Recommended for Testing

**Status**: Pre-trained, ready to use  
**Source**: https://huggingface.co/karpathy/tinyllamas

```bash
# Download
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin
wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin

# Copy to boot disk
cp stories15M.bin tokenizer.bin /path/to/efi/partition/

# Build and test
make llama2-disk
```

**Specs**:
- 15M parameters
- 6 layers, 288 dim
- 32K vocab, 256 seq_len
- Trained on TinyStories dataset
- ~10 tokens/sec on bare metal

### 2️⃣ NanoGPT-124M (48MB) - Requires Conversion

**Status**: Pre-trained, needs export  
**Source**: https://github.com/karpathy/nanoGPT

```bash
# Clone nanoGPT repo
git clone https://github.com/karpathy/nanoGPT.git
cd nanoGPT

# Download GPT-2 124M weights
python3 train.py --device=cpu --compile=False \
                 --eval_only=True --init_from=gpt2

# Export to binary format
python3 export.py gpt2 --output=../nanogpt.bin
cd ..

# Verify
ls -lh nanogpt.bin  # Should be ~48MB
```

**Specs**:
- 124M parameters
- 12 layers, 768 dim
- 50K vocab, 1024 seq_len
- GPT-2 architecture
- ~3 tokens/sec on bare metal

### 3️⃣ TinyLlama-1.1B-Chat (440MB) - Requires Conversion

**Status**: Pre-trained chat model  
**Source**: https://huggingface.co/TinyLlama/TinyLlama-1.1B-Chat-v1.0

```bash
# Clone llama2.c repo
git clone https://github.com/karpathy/llama2.c.git
cd llama2.c

# Install dependencies
pip install torch transformers sentencepiece

# Export TinyLlama to binary
python3 export.py TinyLlama/TinyLlama-1.1B-Chat-v1.0 \
                  --output=../tinyllama_chat.bin

cd ..

# Verify
ls -lh tinyllama_chat.bin  # Should be ~440MB
```

**Specs**:
- 1.1B parameters
- 22 layers, 2048 dim
- 32K vocab, 2048 seq_len
- Chat-tuned for conversations
- ~0.5 tokens/sec on bare metal (CPU only)

## Training from Scratch

### Custom stories15M (1-2 hours on GPU)

```bash
cd llama2.c

# Download TinyStories dataset
python3 tinystories.py download
python3 tinystories.py pretokenize

# Train
python3 train.py \
    --vocab_size=32000 \
    --dim=288 \
    --n_layers=6 \
    --n_heads=6 \
    --n_kv_heads=6 \
    --multiple_of=32 \
    --max_seq_len=256 \
    --batch_size=128 \
    --learning_rate=1e-3 \
    --max_iters=100000

# Export
python3 export.py stories15M.pt --output=../stories15M_custom.bin
```

### Fine-tune NanoGPT (3-4 hours on GPU)

```bash
cd nanoGPT

# Prepare custom dataset (bootloader domain)
cat > bootloader_conversations.txt << EOF
User: What is UEFI?
Assistant: UEFI (Unified Extensible Firmware Interface) is modern firmware that replaces BIOS.

User: How to boot from USB?
Assistant: Enter BIOS/UEFI setup (usually F2/Del), change boot order, select USB device.

User: What is a bootloader?
Assistant: A bootloader loads and starts the operating system kernel during boot.
EOF

# Prepare dataset
python3 prepare.py

# Fine-tune from GPT-2
python3 train.py \
    --init_from=gpt2 \
    --dataset=bootloader \
    --batch_size=12 \
    --learning_rate=3e-4 \
    --max_iters=10000

# Export
python3 export.py ckpt.pt --output=../nanogpt_finetuned.bin
```

## Deployment

### Copy Models to Boot Disk

```bash
# Mount EFI partition
sudo mount /dev/sdX1 /mnt/efi

# Copy models (copy all or just one)
sudo cp stories15M.bin /mnt/efi/
sudo cp nanogpt.bin /mnt/efi/
sudo cp tinyllama_chat.bin /mnt/efi/
sudo cp tokenizer.bin /mnt/efi/

# Unmount
sudo umount /mnt/efi
```

### Build Bootloader Disk

```bash
# Clean and rebuild
make clean
make

# Create disk image with stories15M
make llama2-disk

# The bootloader will auto-detect all available models!
```

## Testing

### Test in QEMU

```bash
qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive format=raw,file=llama2-disk.img \
    -m 512M \
    -serial mon:stdio

# Expected boot sequence:
# 1. Model detection scan
# 2. Selection menu (if multiple models)
# 3. Model loading
# 4. Auto-demo REPL
```

### Test on Real Hardware

```bash
# Write to USB
sudo dd if=llama2-disk.img of=/dev/sdX bs=4M status=progress

# Boot from USB
# - Keyboard input works (unlike QEMU)
# - Can interact with REPL
# - Faster inference on real CPU
```

## Model Selection at Boot

The bootloader automatically detects available models:

```
╔═══════════════════════════════════════════════╗
║   MULTIMODAL LLM BOOTLOADER - Model Selector  ║
╚═══════════════════════════════════════════════╝

Scanning for models...

  ✓ [1] stories15M (60MB) - Story generation
  ✓ [2] NanoGPT-124M (48MB) - GPT-2 architecture
  ✗ [3] TinyLlama-1.1B-Chat (440MB) - Conversational (not found)

Found 2 model(s).

Select model (1-3): _
```

## Performance Benchmarks

| Model | Size | Inference Speed | Memory | Best For |
|-------|------|-----------------|--------|----------|
| stories15M | 60MB | ~10 tok/sec | 110MB | Quick demos, testing |
| NanoGPT-124M | 48MB | ~3 tok/sec | 200MB | General completion |
| TinyLlama-Chat | 440MB | ~0.5 tok/sec | 1GB | Conversational AI |

*Benchmarks on bare metal UEFI (no OS overhead), Intel Core i5, no GPU*

## Troubleshooting

### Model Not Detected
```bash
# Check file is on root of EFI partition
ls /mnt/efi/*.bin

# Verify filename matches exactly:
# - stories15M.bin
# - nanogpt.bin
# - tinyllama_chat.bin
```

### Conversion Failed
```bash
# Install dependencies
pip install torch transformers sentencepiece

# Use exact export command from above
# Check disk space (need 2x model size for conversion)
```

### Out of Memory
```bash
# TinyLlama needs 1GB+ RAM
# Try smaller model (stories15M or NanoGPT)
# Or increase QEMU memory: -m 2048M
```

## Next Steps

After training models:
1. ✅ Test model selection menu
2. ⏭️ **Option 4**: Improve tokenizer BPE
3. ⏭️ **Option 5**: Optimize AVX/SSE performance
4. ⏭️ **Option 6**: Add conversational features

## Resources

- **llama2.c**: https://github.com/karpathy/llama2.c
- **nanoGPT**: https://github.com/karpathy/nanoGPT
- **TinyLlama**: https://huggingface.co/TinyLlama
- **TinyStories**: https://huggingface.co/datasets/roneneldan/TinyStories

## License

MIT - Based on Karpathy's llama2.c and nanoGPT
