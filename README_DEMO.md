# LLM Bare Metal - Demo Mode

## Status

✅ **Model compiles and runs in QEMU**
✅ **Trained weights are embedded** (120,576 parameters)
✅ **Forward pass works** (Transformer architecture)

⚠️ **Current issue**: Model needs more training
- Currently trained: 100 steps
- Loss: 5.54 → 3.61
- Output: Still noisy (non-ASCII characters)

## Next Steps

### 1. Train Longer
```bash
cd llm.c
gcc -O3 -o train_nano.exe train_nano.c -lm
./train_nano.exe 5000
```

### 2. Convert Weights
```bash
cd llm.c
python convert_weights_to_c.py
# This updates llm-baremetal/trained_weights.h
```

### 3. Rebuild EFI
```bash
cd llm-baremetal
wsl bash -c "cd /mnt/c/.../llm-baremetal && make clean && make && make disk"
```

### 4. Test in QEMU
```bash
wsl bash -c "qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=llm-disk.img -m 512M -serial mon:stdio -nographic"
```

## Expected Timeline

- **1000 steps**: ~2 hours, Loss ~2.5
- **5000 steps**: ~10 hours, Loss ~2.0
- **10000 steps**: ~20 hours, Loss ~1.8

## Demo Output (Current - 100 steps)

```
>>> Prompt: To be or not to be
>>> Generation: To be or not to be5*[ZK(}&@7)@$
```

## Demo Output (Expected - 5000 steps)

```
>>> Prompt: To be or not to be
>>> Generation: To be or not to be, that is the question
```

## Keyboard Issue in QEMU

The keyboard doesn't work properly in QEMU because:
1. EFI keyboard input (`ConIn->ReadKeyStroke`) doesn't work well with QEMU's serial console
2. Solution: Use demo mode with hardcoded prompts (current version)
3. Alternative: Run on real hardware or use different QEMU display mode

## Architecture Verified

✅ Token Embedding
✅ Position Embedding  
✅ Layer Normalization
✅ Multi-Head Attention (simplified)
✅ GELU Activation
✅ MLP (Feed-Forward)
✅ Residual Connections
✅ Softmax Sampling

All math is bare-metal (no stdlib, custom exp/sqrt/tanh).
