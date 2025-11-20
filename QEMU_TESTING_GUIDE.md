# 🧪 QEMU Testing Guide - LLaMA2 Bare-Metal

## Current Test Status

**Test Running**: QEMU with llama2-disk.img  
**Expected Duration**: 30-60 seconds for model loading + inference

---

## What Should Happen

### 1. UEFI Boot (5-10 seconds)
```
UEFI firmware initializing...
BdsDxe: loading Boot0001 "UEFI Shell"
Starting: EFI Boot Manager...
```

### 2. EFI Application Launch (2-3 seconds)
```
Loading EFI application: BOOTX64.EFI
Starting llama2.efi...
```

### 3. LLaMA2 Output (Expected)
```
========================================
  LLaMA2 Bare-Metal EFI (stories15M)
  95% code from Andrej Karpathy
  Architecture by Meta Platforms
========================================

Model config: dim=288, n_layers=6, n_heads=6, vocab=32000

Running forward pass (token=1, pos=0)...
Top token: 234 (logit=8.451)

Generating 20 tokens:
234 567 12 4567 890 23 456 78 901 234 ...

Done! Press any key to exit.
```

---

## Possible Issues & Solutions

### Issue 1: File Not Found
**Symptom**: "Failed to open checkpoint: stories15M.bin"

**Cause**: Model file not in disk image

**Fix**:
```bash
cd llm-baremetal
make llama2-disk  # Rebuild with model
```

### Issue 2: Memory Allocation Error
**Symptom**: "Model too large for static allocation!"

**Cause**: Config exceeds MAX_DIM/MAX_LAYERS

**Check**: stories15M config should be:
- dim=288 (< MAX_DIM=288) ✅
- n_layers=6 (< MAX_LAYERS=6) ✅
- vocab=32000 (< MAX_VOCAB=32000) ✅

### Issue 3: Math Library Issues
**Symptom**: Undefined references to `exp`, `sqrt`, `pow`

**Status**: Should be resolved (no -lm needed for EFI)

### Issue 4: Black Screen / No Output
**Symptom**: QEMU window opens but stays black

**Possible Causes**:
1. OVMF not loading correctly
2. Disk image corrupted
3. EFI binary not in right location

**Verify**:
```bash
# Check disk contents
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && mdir -i llama2-disk.img ::/"
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && mdir -i llama2-disk.img ::/EFI/BOOT"
```

### Issue 5: QEMU Exits Immediately
**Symptom**: QEMU closes right after opening

**Cause**: EFI application crashes

**Debug**:
```bash
# Test hello.efi first (simpler)
make test-hello
```

---

## Performance Expectations

### Model Loading
- **File I/O**: Read 60MB (stories15M.bin)
- **Time**: ~5-15 seconds on QEMU
- **Memory**: ~17MB static allocation

### Forward Pass
- **Single token**: ~100-500ms
- **20 tokens**: ~2-10 seconds
- **Bottleneck**: Matrix multiplications (no SIMD/optimization)

---

## Debug Commands

### Check Disk Image Contents
```bash
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && mdir -i llama2-disk.img ::/"
```

**Expected Output**:
```
stories15M.bin
tokenizer.bin
EFI/
```

### Verify EFI Binary Location
```bash
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && mdir -i llama2-disk.img ::/EFI/BOOT"
```

**Expected Output**:
```
BOOTX64.EFI
```

### Test Simpler EFI First
```bash
make test-hello  # Test hello.efi (just prints text)
```

---

## Alternative Test Methods

### Method 1: QEMU with Serial Output
```bash
qemu-system-x86_64 \
  -bios /usr/share/ovmf/OVMF.fd \
  -drive format=raw,file=llama2-disk.img \
  -m 512M \
  -serial stdio \
  -nographic
```

### Method 2: QEMU with Logging
```bash
qemu-system-x86_64 \
  -bios /usr/share/ovmf/OVMF.fd \
  -drive format=raw,file=llama2-disk.img \
  -m 512M \
  -d guest_errors,unimp \
  -D qemu-debug.log
```

### Method 3: Windows Script
```powershell
.\run-qemu-gui.ps1
```

---

## What We're Testing

### Core Functionality
1. ✅ EFI binary loads
2. ✅ File system access (read stories15M.bin)
3. ✅ Model config parsing
4. ✅ Weight loading (60MB)
5. ✅ Static allocation works
6. ✅ Transformer forward pass
7. ✅ Token generation
8. ✅ Console output (Print)

### Expected Behavior
- Model loads successfully
- Config matches: dim=288, n_layers=6, n_heads=6
- Forward pass executes (may be slow)
- Tokens are generated (may be random at first)
- Clean exit with "Press any key"

---

## Success Criteria

### Minimal Success ✅
- EFI boots
- Model config prints
- No crashes

### Full Success 🎉
- Model loads (60MB)
- Forward pass executes
- Tokens generated
- Output visible

---

## Next Steps After Test

### If Success ✅
1. Verify token quality
2. Implement full BPE tokenizer
3. Test longer generation
4. Profile performance
5. Optimize hotspots

### If Failure ❌
1. Check debug logs
2. Test hello.efi first
3. Verify disk contents
4. Review compilation output
5. Add more Print() debug statements

---

## Current Status

**Running**: QEMU test in progress...  
**Watching**: QEMU window for output  
**Expecting**: Model config print + token generation  

---

**Patience**: First run may take 30-60 seconds as model loads from disk! 🕐
