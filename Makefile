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

# Compile llama2_efi
$(LLAMA2_OBJ): llama2_efi.c
	$(CC) $(CFLAGS) -c llama2_efi.c -o $(LLAMA2_OBJ)

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

# Create disk image with llama2 (stories15M)
llama2-disk: $(LLAMA2)
	@echo "Creating EFI disk image with LLaMA2..."
	dd if=/dev/zero of=llama2-disk.img bs=1M count=128
	mkfs.fat -F32 llama2-disk.img
	mmd -i llama2-disk.img ::/EFI
	mmd -i llama2-disk.img ::/EFI/BOOT
	mcopy -i llama2-disk.img $(LLAMA2) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i llama2-disk.img stories15M.bin ::/
	@echo "Disk image created: llama2-disk.img (15M params)"

# Test llama2 in QEMU
test-llama2: llama2-disk
	@echo "Testing LLaMA2 (stories15M) in QEMU..."
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
	                   -drive format=raw,file=llama2-disk.img \
	                   -m 512M \
	                   -serial mon:stdio

# Default run target
run: test-llama2

.PHONY: all clean llama2-disk test-llama2 run
