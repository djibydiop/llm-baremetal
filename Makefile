# Makefile for llm-baremetal EFI application

# Architecture
ARCH = x86_64

# Compiler and flags
CC = gcc
CFLAGS = -ffreestanding -fno-stack-protector -fpic \
         -fshort-wchar -mno-red-zone -I/usr/include/efi \
         -I/usr/include/efi/$(ARCH) -DEFI_FUNCTION_WRAPPER \
         -O3 -funroll-loops -ffast-math -finline-functions

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

# Compile network_boot.c
network_boot.o: network_boot.c
	$(CC) $(CFLAGS) -msse2 -c network_boot.c -o network_boot.o

# Compile wifi_ax200.c
wifi_ax200.o: wifi_ax200.c wifi_ax200.h
	$(CC) $(CFLAGS) -msse2 -c wifi_ax200.c -o wifi_ax200.o

# Compile llama2_efi with SSE2 (QEMU-compatible)
$(LLAMA2_OBJ): llama2_efi.c
	$(CC) $(CFLAGS) -msse2 -c llama2_efi.c -o $(LLAMA2_OBJ)

# Link llama2_efi with network_boot and wifi_ax200
llama2.so: $(LLAMA2_OBJ) network_boot.o wifi_ax200.o
	ld $(LDFLAGS) $(LLAMA2_OBJ) network_boot.o wifi_ax200.o -o llama2.so $(LIBS)

# Convert llama2 to EFI
$(LLAMA2): llama2.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) llama2.so $(LLAMA2)

# Clean build artifacts
clean:
	rm -f $(LLAMA2_OBJ) network_boot.o wifi_ax200.o llama2.so $(LLAMA2)

# Create disk image with stories15M model ONLY
disk: $(LLAMA2)
	@echo "Creating EFI disk image with stories15M..."
	dd if=/dev/zero of=qemu-test.img bs=1M count=128
	mkfs.fat -F32 qemu-test.img
	mmd -i qemu-test.img ::/EFI
	mmd -i qemu-test.img ::/EFI/BOOT
	mcopy -i qemu-test.img $(LLAMA2) ::/EFI/BOOT/BOOTX64.EFI
	@if [ -f stories15M.bin ]; then mcopy -i qemu-test.img stories15M.bin ::/; echo "OK Copied stories15M.bin (60MB)"; else echo "ERROR: stories15M.bin not found!"; exit 1; fi
	@if [ -f tokenizer.bin ]; then mcopy -i qemu-test.img tokenizer.bin ::/; echo "OK Copied tokenizer.bin"; fi
	@echo "Disk image created: qemu-test.img (128MB)"

# Legacy target for compatibility
llama2-disk: disk

# Test llama2 in QEMU
test-llama2: llama2-disk
	@echo "Testing LLaMA2 (stories15M) in QEMU..."
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
	                   -drive format=raw,file=llama2-disk.img \
	                   -m 512M \
	                   -serial mon:stdio

# Create USB bootable image for Rufus
usb-image: $(LLAMA2)
	@echo "Creating USB bootable image for Rufus..."
	dd if=/dev/zero of=llm-baremetal-usb.img bs=1M count=128
	mkfs.fat -F32 llm-baremetal-usb.img
	mmd -i llm-baremetal-usb.img ::/EFI
	mmd -i llm-baremetal-usb.img ::/EFI/BOOT
	mcopy -i llm-baremetal-usb.img $(LLAMA2) ::/EFI/BOOT/BOOTX64.EFI
	@if [ -f stories15M.bin ]; then mcopy -i llm-baremetal-usb.img stories15M.bin ::/; echo "OK Copied stories15M.bin (60MB)"; else echo "ERROR: stories15M.bin not found!"; exit 1; fi
	@if [ -f tokenizer.bin ]; then mcopy -i llm-baremetal-usb.img tokenizer.bin ::/; echo "OK Copied tokenizer.bin"; fi
	@echo ""
	@echo "========================================="
	@echo "USB BOOTABLE IMAGE READY!"
	@echo "========================================="
	@echo "File: llm-baremetal-usb.img (128MB)"
	@echo ""
	@echo "Next steps:"
	@echo "  1. Open Rufus"
	@echo "  2. Select your USB drive"
	@echo "  3. Boot selection: Disk or ISO image (DD Image)"
	@echo "  4. SELECT: llm-baremetal-usb.img"
	@echo "  5. Partition scheme: GPT"
	@echo "  6. Target system: UEFI (non CSM)"
	@echo "  7. Click START"
	@echo ""
	@echo "Made in Dakar, Senegal!"

# Default run target
run: test-llama2

.PHONY: all clean llama2-disk test-llama2 run usb-image
