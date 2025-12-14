LLM Bare-Metal UEFI System
===========================

This USB drive contains a complete LLM inference system that boots directly
on UEFI hardware without an operating system.

Contents:
- EFI\BOOT\BOOTX64.EFI - UEFI bootloader with LLM implementation (v5.1 - LLaMA 3 support)
- stories110M.bin - 110M parameter language model (418MB) - Better quality
- tokenizer.bin - BPE tokenizer for text encoding/decoding (434KB)

Boot Instructions:
1. Insert USB drive into target machine
2. Enter BIOS/UEFI settings (F2, F10, F12, or DEL key at boot)
3. Set USB drive as first boot device
4. Save and exit BIOS
5. System will boot and load LLM automatically

Hardware Requirements:
- UEFI firmware (most PCs since 2010)
- x86-64 CPU with AVX2 support
- 4GB+ RAM
- USB boot capability

Features:
- Interactive menu with 6 categories (Stories, Science, Adventure, Philosophy, History, Technology)
- 41 pre-configured prompts
- BPE tokenization for prompt understanding
- AVX2 optimized inference
- Temperature-controlled generation

Troubleshooting:
- If boot fails, ensure USB is FAT32 formatted
- Check Secure Boot is disabled in BIOS
- Verify UEFI boot mode (not Legacy/CSM)

GitHub: https://github.com/djibydiop/llm-baremetal
