# GitHub Issues à Créer

Copiez-collez ces textes lors de la création des Issues sur GitHub.

---

## Issue #1: [Feature] Support for larger models (110M+)

**Labels:** `enhancement`, `help wanted`

**Description:**

Currently, the system supports Stories15M (60 MB) perfectly. We need to add support for larger models:

- Stories110M (420 MB)
- Stories260M (~1 GB)
- LLaMA 3.2 1B/3B models

**Challenges:**
- Memory allocation (need efficient chunking)
- Boot time (larger file loading)
- Possibly need quantization (4-bit/8-bit) for really large models

**Technical Requirements:**
- [ ] Test 110M model loading
- [ ] Implement 4-bit/8-bit quantization
- [ ] Optimize memory allocation for large weights
- [ ] Add model size detection and validation

**Help Wanted:**
If you have experience with model quantization or large file I/O in UEFI, contributions are very welcome!

**Related:** This would make the system usable for more complex generation tasks.

---

## Issue #2: [Feature] ARM64 port for Raspberry Pi

**Labels:** `enhancement`, `help wanted`, `good first issue`

**Description:**

Port the bare-metal LLM to ARM64 architecture, specifically targeting:

- Raspberry Pi 4/5
- Other ARM64 single-board computers
- ARM-based embedded systems

**Why This Matters:**
ARM is dominant in embedded/IoT. This would open the project to a massive new audience.

**Technical Requirements:**
- [ ] Replace x86-64 UEFI calls with ARM64 equivalents
- [ ] Port SSE2 optimizations to NEON (ARM SIMD)
- [ ] Test on Raspberry Pi 4B/5
- [ ] Create ARM64 build target in Makefile
- [ ] Documentation for ARM setup

**Challenges:**
- UEFI on ARM is less common (may need U-Boot integration)
- NEON intrinsics different from SSE2
- Different memory models

**Help Wanted:**
If you have ARM64 development experience or a Raspberry Pi to test on, please contribute!

---

## Issue #3: [Feature] Network boot (PXE) support

**Labels:** `enhancement`, `experimental`

**Description:**

Enable booting and loading models over the network instead of USB.

**Use Cases:**
- Datacenter deployment
- Diskless workstations
- Model updates without reflashing USB
- Multi-machine deployments

**Technical Requirements:**
- [ ] Implement UEFI PXE boot protocol
- [ ] HTTP/TFTP model downloading
- [ ] Progress indicators during network download
- [ ] Fallback to local storage if network fails
- [ ] Configuration file support (specify model URL)

**Architecture:**
```
1. PXE boot → Load llama2.efi from network
2. llama2.efi → Download model via HTTP/TFTP
3. Cache in RAM → Run inference
4. Optional: Cache to disk for future boots
```

**Help Wanted:**
If you have experience with PXE boot or UEFI network protocols, your expertise is needed!

---

## Issue #4: [Help Wanted] Testing on more hardware

**Labels:** `help wanted`, `testing`

**Description:**

We need community testing on various hardware to ensure compatibility.

**Currently Tested:**
- ✅ QEMU (x86-64)
- ✅ Dell laptops
- ✅ HP desktops
- ✅ Lenovo ThinkPad

**Need Testing On:**
- [ ] AMD Ryzen systems
- [ ] Intel 12th/13th/14th gen
- [ ] ASUS motherboards
- [ ] MSI motherboards
- [ ] Gigabyte motherboards
- [ ] MacBook Pro (Intel) with UEFI boot
- [ ] Surface Pro devices
- [ ] Custom-built PCs
- [ ] Server hardware

**How to Help:**

1. Flash `llm-baremetal-usb.img` to a USB stick using Rufus
2. Boot your PC from USB (UEFI mode, not Legacy)
3. Report results in this issue:
   - Hardware specs (CPU, RAM, motherboard)
   - Did it boot? (Yes/No)
   - Did model load? (Yes/No)
   - Did text generate? (Yes/No)
   - Any errors or freezes?
   - Screenshot if possible

**Template:**
```
**Hardware:**
- CPU: Intel i7-12700K
- RAM: 32GB DDR4
- Motherboard: ASUS ROG Strix Z690
- BIOS: UEFI 2.7

**Results:**
- Boot: ✅ Success
- Model Load: ✅ Success
- Text Generation: ✅ Success
- Speed: ~15 tokens/sec
- Notes: Worked perfectly!
```

Your testing helps make this work on more hardware! 🙏

---

## Issue #5: [Discussion] What models would you like to see?

**Labels:** `question`, `discussion`

**Description:**

This project currently supports Stories15M (60 MB). What other models would the community find useful?

**Current Status:**
- ✅ Stories15M (15M params) - Working perfectly
- 🚧 Stories110M (110M params) - Planned
- 🚧 Stories260M (260M params) - Planned

**Possible Future Models:**
- LLaMA 3.2 1B/3B (would need quantization)
- TinyLlama 1.1B
- GPT-2 variants (124M, 355M)
- Phi-2 (2.7B, would need heavy quantization)
- Custom fine-tuned models

**Questions for Discussion:**

1. **What use case are you interested in?**
   - Code generation?
   - Story/creative writing?
   - Chatbot/Q&A?
   - Text classification?
   - Other?

2. **What size model is acceptable?**
   - Prefer smaller (faster boot) or larger (better quality)?
   - What's your hardware RAM limit?

3. **Quantization preferences?**
   - OK with 4-bit/8-bit quantized models?
   - Or prefer full FP32 precision?

4. **Domain-specific models?**
   - Medical/legal/technical jargon?
   - Multilingual support?
   - Code-specific models?

**Share your thoughts!** This helps prioritize development. 🚀

---

## Issue #6: [Enhancement] Add color support (carefully)

**Labels:** `enhancement`, `ui/ux`

**Description:**

Original version had colorful UI, but `SetAttribute()` UEFI calls caused system freezes on some firmware.

**Goal:** Implement color support that:
- Detects firmware compatibility first
- Falls back to plain text if colors cause issues
- Adds visual appeal without breaking stability

**Technical Approach:**
```c
// Test if SetAttribute works without freezing
BOOLEAN test_color_support() {
    // Try setting color and immediately check if system responds
    // If timeout or freeze detected, disable colors
}
```

**Help Wanted:**
If you understand UEFI firmware quirks or have ideas for safe color detection, please contribute!

---

## Issue #7: [Enhancement] Multi-threaded inference

**Labels:** `enhancement`, `performance`

**Description:**

Current implementation is single-threaded. Multi-threading could significantly speed up inference.

**Potential Speedup:**
- Matrix multiplication parallelization
- Attention heads computed in parallel
- Layer-wise parallelism

**Challenges:**
- UEFI threading primitives are limited
- Memory synchronization
- Debugging multi-threaded bare-metal code is hard

**Help Wanted:**
If you have experience with UEFI multi-threading or parallel computing, let's discuss!

---

## How to Create These Issues

1. Go to https://github.com/djibydiop/llm-baremetal/issues
2. Click "New Issue"
3. Copy-paste the title and description
4. Add the suggested labels
5. Click "Submit new issue"

**Create in this order for best impact:**
1. Issue #4 (Testing) - Gets community engaged immediately
2. Issue #5 (Discussion) - Generates conversation
3. Issue #1 (Larger models) - Shows roadmap
4. Issue #2 (ARM64) - Opens new platforms
5. Issue #3 (Network boot) - Advanced feature
6. Issue #6 (Colors) - Nice-to-have
7. Issue #7 (Multi-threading) - Research/future

This creates momentum and invites collaboration! 🚀
