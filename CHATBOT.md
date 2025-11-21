# Bare Metal LLM Chatbot (Based on llm.c)

## What We Built

A **bare metal chatbot REPL** that runs without an operating system, inspired by Djiby Diop's guidance to use llm.c's inference code.

### Architecture

```
┌──────────────────────────────────┐
│   BIOS/UEFI → chatbot.efi        │
├──────────────────────────────────┤
│  REPL Loop:                      │
│  1. Display >>> prompt           │
│  2. Generate tokens              │
│  3. Sample from distribution     │
│  4. Print character-by-character │
└──────────────────────────────────┘
```

### Implementation Based on llm.c

Following [train_gpt2.c lines 1130-1160](https://github.com/karpathy/llm.c/blob/master/train_gpt2.c#L1130-L1160), we implemented:

1. **Token-by-token generation**:
   ```c
   for (int t = prompt_len; t < max_tokens; t++) {
       gpt_nano_forward_logits(&g_model, context, context_len, logits);
       softmax_temp(logits, VOCAB_SIZE, temperature);
       
       float coin = random_f32();
       int next_token = sample_mult(logits, VOCAB_SIZE, coin);
       
       tokens[t] = (UINT8)next_token;
       Print(L"%c", (CHAR16)next_token);
   }
   ```

2. **Temperature-based sampling** (creative vs deterministic):
   - Temperature = 1.0 (default): Creative/random
   - Temperature < 1.0: More focused
   - Temperature > 1.0: More random

3. **Multinomial sampling from distribution**:
   - Not greedy (argmax)
   - Samples proportional to probabilities
   - Enables diverse outputs

### Differences from Original Demo

| Feature | llm_efi.c (old) | llm_chatbot.c (new) |
|---------|-----------------|---------------------|
| Sampling | Greedy (argmax) | Multinomial (llm.c style) |
| Temperature | None | 1.0 (configurable) |
| Output style | Dual (GPT + curated) | Pure GPT streaming |
| Code base | Custom | Based on llm.c |

## Current Output

```
================================================
  Bare Metal LLM Chatbot (Demo Mode)
================================================

Model: Nano GPT (29056 params)
Temperature: 1.0 (creative)
Max tokens: 80

>>> HelloB4Je<HqxU/~bmSNGY>?}T9e7yuTru
>>> The meaning of life is"wO]'(7V]3lF=cxyX-1<j<m'%T;
>>> Once upon a timeLG|+:mRW,iI74s:uc:/:5)jXA
>>> To be or not to beWd?1L^JqPpPFAbbeE-E7~&(4kL
```

**Why gibberish?**
- Model has ~29K untrained parameters
- Weights initialized with pseudo-random values
- Need training on real text corpus (Shakespeare, etc.)

## Building

```bash
cd llm-baremetal
make clean
make disk
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
                   -drive format=raw,file=llm-disk.img \
                   -m 512M -nographic -serial mon:stdio
```

## Next Steps (Per Djiby's Guidance)

### 1. Train Model Properly
- [x] Training infrastructure exists (train_nano.py)
- [ ] Train on full Shakespeare corpus (10K+ tokens)
- [ ] Run for 1000+ epochs on CPU
- [ ] Export weights to .bin file

### 2. Implement File Loading
- [ ] Save weights to USB/disk: `nano_gpt_weights.bin`
- [ ] Implement EFI file reading API
- [ ] Load weights at boot time from disk
- [ ] Reference: [train_gpt2.c lines 1084-1087](https://github.com/karpathy/llm.c/blob/master/train_gpt2.c#L1084-L1087)

### 3. Add Keyboard Input
- [ ] Implement proper EFI keyboard input (no crash)
- [ ] Read user prompts character-by-character
- [ ] Echo input to screen
- [ ] Submit on Enter key

### 4. CTRL+C Interrupt Handling
**Vision**: "Freeze time, inject thought"
- [ ] Detect CTRL+C during generation
- [ ] Stop token generation mid-stream
- [ ] Return to `>>>` prompt
- [ ] Implement as conscious process control

### 5. Real REPL Loop
```
>>> [user types prompt]
[model generates response token-by-token]

>>> [next prompt]
[streaming response...]

>>> ^C
>>> [interrupted, back to prompt]
```

## Code Structure

```
llm-baremetal/
├── llm_chatbot.c      ← New chatbot REPL (this file)
├── gpt_nano.h         ← Transformer (added forward_logits)
├── trained_weights.h  ← Generated weights (not yet loaded)
├── train_nano.py      ← Training script
└── Makefile           ← Build system
```

## Comparaison avec llm.c

| llm.c | llm-baremetal |
|-------|---------------|
| C (Linux/Unix) | C (EFI/bare metal) |
| GPT-2 (124M) | Nano GPT (29K) |
| CUDA/CPU | CPU only (bare metal) |
| Full training | Inference focus |
| File I/O | EFI file APIs (todo) |
| stdin/stdout | EFI console |

## Key Learnings from llm.c

1. **Inference is simpler than training**
   - Forward pass only
   - No gradients, no backprop
   - Perfect for bare metal

2. **Token streaming = REPL ready**
   - Generate one token at a time
   - Print immediately
   - User sees progress

3. **Temperature matters**
   - Too low: repetitive
   - Too high: incoherent
   - 0.6-1.0 is sweet spot

4. **Sampling > Greedy**
   - Greedy (argmax) is boring
   - Multinomial adds variety
   - Still coherent with good weights

## Performance

- **Boot time**: ~2 seconds (OVMF UEFI)
- **Generation**: ~instant (model is tiny)
- **Memory**: 512MB allocated (barely used)
- **Binary size**: ~50KB EFI executable

## Credits

- **Andrej Karpathy** - llm.c (inference code inspiration)
- **Djiby Diop** - Technical guidance + vision
- Code adapted from train_gpt2.c (lines 1130-1160)
