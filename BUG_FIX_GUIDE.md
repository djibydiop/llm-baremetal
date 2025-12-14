# 🐛 BUG FIX GUIDE - Garbled Output Investigation

## 🔍 Current Status

**Problem**: Garbled output despite correct tokenizer
- Stories15M.bin WORKS in llama2.c (reference) → Perfect English
- Stories15M.bin FAILS in llama2_efi.c → Garbled ("Se Run want ing daygoogle")

**Hypothesis**: Bug in forward pass OR sampling/RNG

---

## 📊 Step 1: Logits Comparison (IN PROGRESS)

### Reference (llama2.c - WORKING):
```
First 10 logits: [0]=-6.7908 [1]=0.8281 [2]=-6.7904 [3]=-6.7905 [4]=-6.7905 [5]=-6.7906 [6]=-6.7905 [7]=-6.7907 [8]=-6.7905 [9]=-6.7905
Output: Once upon a time, there was a little girl named Lily...
```

### UEFI (llama2_efi.c - BROKEN):
```
Look in QEMU for: [DEBUG pos=0] First 10 logits: ...
Values: [PENDING USER OBSERVATION]
```

---

## 🔧 Step 2: Diagnosis Paths

### Path A: If logits MATCH reference → BUG IS IN SAMPLING

**Problem Areas**:
1. **rand_efi() not uniform**
   - Check if random numbers cluster
   - Verify UEFI RNG initialization
   
2. **sample_mult() incorrect probability**
   - Softmax overflow causing degenerate distribution
   - CDF accumulation error
   
3. **Temperature handling broken**
   - Division by temperature corrupts values
   
**Fixes to Try**:

```c
// FIX 1: Add RNG debug
unsigned int rand_efi(void) {
    EFI_STATUS Status;
    EFI_TIME Time;
    Status = uefi_call_wrapper(RT->GetTime, 2, &Time, NULL);
    
    // DEBUG: Print raw random value
    unsigned int val = (Time.Nanosecond * 1103515245U + 12345U) & 0x7FFFFFFF;
    if (pos < 3) {
        Print(L"[DEBUG RNG] val=%u\r\n", val);
    }
    return val;
}

// FIX 2: Test greedy sampling (temperature=0)
// In generate(), temporarily force:
temperature = 0.0f;  // Greedy argmax - if this works, RNG is the issue

// FIX 3: Verify softmax doesn't overflow
void softmax(float* x, int size) {
    // Find max for numerical stability
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    
    // DEBUG: Check max value
    if (pos < 3) {
        Print(L"[DEBUG SOFTMAX] max_val=%.4f\r\n", max_val);
    }
    
    // Subtract max BEFORE exp
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    
    // DEBUG: Check sum
    if (pos < 3) {
        Print(L"[DEBUG SOFTMAX] sum=%.4f\r\n", sum);
    }
    
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}
```

---

### Path B: If logits DIFFER from reference → BUG IS IN FORWARD PASS

**Problem Areas**:
1. **Embedding lookup wrong offset**
   - `w->token_embedding_table + token * dim` miscalculation
   
2. **MatMul dimension mismatch**
   - Attention Q, K, V dimensions incorrect
   - FFN hidden layer size wrong
   
3. **RoPE calculation error**
   - Frequency calculation wrong: `1.0 / powf(10000.0, head_dim / head_size)`
   - sin/cos application incorrect
   
4. **Attention softmax corrupted**
   - Scaling by `sqrt(head_size)` wrong
   - Mask application broken
   
**Fixes to Try**:

```c
// FIX 1: Debug embedding lookup
float* content_row = w->token_embedding_table + token * dim;

// DEBUG: Print first 5 embedding values
if (pos == 0) {
    Print(L"[DEBUG EMBED token=%d] ", token);
    for (int i = 0; i < 5; i++) {
        Print(L"[%d]=%.4f ", i, content_row[i]);
    }
    Print(L"\r\n");
}

// FIX 2: Verify dimensions in forward()
Print(L"[DEBUG DIMS] dim=%d n_layers=%d n_heads=%d vocab=%d\r\n",
      p->dim, p->n_layers, p->n_heads, p->vocab_size);

// FIX 3: Check RoPE frequencies
void rope_computation_debug(float* q, float* k, int head_size, int pos) {
    for (int i = 0; i < head_size; i += 2) {
        float freq = 1.0f / powf(10000.0f, (float)i / (float)head_size);
        float val = pos * freq;
        
        if (pos == 0 && i < 4) {
            Print(L"[DEBUG ROPE i=%d] freq=%.6f val=%.6f\r\n", i, freq, val);
        }
        
        float fcr = cosf(val);
        float fci = sinf(val);
        
        float q0 = q[i];
        float q1 = q[i+1];
        q[i] = q0 * fcr - q1 * fci;
        q[i+1] = q0 * fci + q1 * fcr;
        
        float k0 = k[i];
        float k1 = k[i+1];
        k[i] = k0 * fcr - k1 * fci;
        k[i+1] = k0 * fci + k1 * fcr;
    }
}

// FIX 4: Debug attention scores
float score = 0.0f;
for (int i = 0; i < head_size; i++) {
    score += q[i] * k[i];
}
score /= sqrtf(head_size);

if (pos < 3 && h == 0 && t == 0) {
    Print(L"[DEBUG ATT pos=%d] score=%.4f head_size=%d\r\n", 
          pos, score, head_size);
}
```

---

## 🎯 Step 3: Apply Fix Based on Diagnosis

### If RNG/Sampling Bug:
1. Replace `rand_efi()` with better UEFI RNG
2. Add `EFI_RNG_PROTOCOL` if available
3. Test with greedy sampling first
4. Verify probability distribution sums to 1.0

### If Forward Pass Bug:
1. Compare intermediate values with llama2.c
2. Add assert-style checks for dimensions
3. Print Q/K/V matrices for first token
4. Verify weight loading is byte-for-byte identical

---

## ✅ Step 4: Verification

Once fixed, output should be:
```
Once upon a time, there was a little girl named Lily. She loved to play outside...
```

Test with:
- stories15M.bin → Children's stories
- shakespeare15M_trained.bin → Elizabethan English
- Multiple prompts to ensure consistency

---

## 🚀 Step 5: Deploy & Create Viral Demo

1. **Integrate beautiful_ui.c** (Gemini 3 style)
2. **Train Shakespeare model** (`python train_shakespeare_fast.py`)
3. **Film in Dakar** (outdoor location showing Senegal)
4. **Post on Twitter**: Tag @karpathy
5. **Submit to HN**: "LLM trained on Shakespeare running bare-metal from USB in Senegal 🇸🇳"

Expected reach:
- Karpathy retweet → 1.4M followers
- HN front page → 500K+ views
- Meta AI / LLaMA team notice

---

## 📝 Notes

- Current llama2_efi.c is 8498 lines, fully optimized
- ARM expf() working (Justine's code)
- WaitForEvent keyboard working
- DRC v4.0 Ultra-Advanced active (10+ domains)
- Compilation successful, deploys to QEMU
- Just needs this ONE bug fixed!

**We're 99% there! Just need to identify if bug is sampling or forward pass!**
