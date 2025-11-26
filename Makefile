# Makefile for llm-baremetal EFI application

# Architecture
ARCH = x86_64

# Compiler and flags
CC = gcc
CFLAGS = -ffreestanding -fno-stack-protector -fpic \
         -fshort-wchar -mno-red-zone -I/usr/include/efi \
         -I/usr/include/efi/$(ARCH) -DEFI_FUNCTION_WRAPPER

LDFLAGS = -nostdlib -znocombreloc -T /usr/lib/elf_$(ARCH)_efi.lds \
          -shared -Bsymbolic -L/usr/lib \
          /usr/lib/crt0-efi-$(ARCH).o

LIBS = -lefi -lgnuefi

# Output files
TARGET = llm.efi
CHATBOT = chatbot.efi
HELLO = hello.efi
LLAMA2 = llama2.efi
OBJ = llm_efi.o
CHATBOT_OBJ = llm_chatbot.o
HELLO_OBJ = hello_efi.o
LLAMA2_OBJ = llama2_efi.o

# Default target
all: $(LLAMA2)

# Compile llama2_efi with AVX2 optimizations
$(LLAMA2_OBJ): llama2_efi.c
	$(CC) $(CFLAGS) -mavx2 -mfma -c llama2_efi.c -o $(LLAMA2_OBJ)

# Link llama2_efi
llama2.so: $(LLAMA2_OBJ)
	ld $(LDFLAGS) $(LLAMA2_OBJ) -o llama2.so $(LIBS)

# Convert llama2 to EFI
$(LLAMA2): llama2.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) llama2.so $(LLAMA2)

# Clean build artifacts
clean:
	rm -f $(LLAMA2_OBJ) llama2.so $(LLAMA2)

# Create disk image with auto-detection support
disk: $(LLAMA2)
	@echo "===================================================="
	@echo "Creating EFI disk image with hardware auto-detection..."
	@echo "===================================================="
	dd if=/dev/zero of=qemu-test.img bs=1M count=512
	mkfs.fat -F32 qemu-test.img
	mmd -i qemu-test.img ::/EFI
	mmd -i qemu-test.img ::/EFI/BOOT
	mcopy -i qemu-test.img $(LLAMA2) ::/EFI/BOOT/BOOTX64.EFI
	@echo "\nCopying available model files..."
	@if [ -f stories15M.bin ]; then mcopy -i qemu-test.img stories15M.bin ::/; echo "  ✓ stories15M.bin (60MB)"; fi
	@if [ -f stories42M.bin ]; then mcopy -i qemu-test.img stories42M.bin ::/; echo "  ✓ stories42M.bin (165MB)"; fi
	@if [ -f stories110M.bin ]; then mcopy -i qemu-test.img stories110M.bin ::/; echo "  ✓ stories110M.bin (420MB)"; fi
	@if [ -f stories260M.bin ]; then mcopy -i qemu-test.img stories260M.bin ::/; echo "  ✓ stories260M.bin (1GB)"; fi
	@if [ -f tinyllama_1b.bin ]; then mcopy -i qemu-test.img tinyllama_1b.bin ::/; echo "  ✓ tinyllama_1b.bin (4.4GB)"; fi
	@if [ -f llama2_7b.bin ]; then mcopy -i qemu-test.img llama2_7b.bin ::/; echo "  ✓ llama2_7b.bin (13GB)"; fi
	@if [ -f tokenizer.bin ]; then mcopy -i qemu-test.img tokenizer.bin ::/; echo "  ✓ tokenizer.bin"; fi
	@echo "\n===================================================="
	@echo "Disk image created: qemu-test.img (512MB)"
	@echo "System will auto-select optimal model based on RAM"
	@echo "===================================================="

# Legacy target for compatibility
llama2-disk: disk

# Test llama2 in QEMU
test-llama2: llama2-disk
	@echo "Testing LLaMA2 (stories15M) in QEMU..."
	qemu-system-x86_64 -drive if=pflash,format=raw,readonly=on,file=OVMF.fd \
	                   -drive format=raw,file=qemu-test.img \
	                   -m 512M \
	                   -serial mon:stdio

# Default run target
run: test-llama2

.PHONY: all clean llama2-disk test-llama2 run
