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
OBJ = llm_efi.o

# Default target
all: $(TARGET)

# Compile C to object file
$(OBJ): llm_efi.c
	$(CC) $(CFLAGS) -c llm_efi.c -o $(OBJ)

# Link to EFI shared object
llm.so: $(OBJ)
	ld $(LDFLAGS) $(OBJ) -o llm.so $(LIBS)

# Convert to EFI executable
$(TARGET): llm.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) llm.so $(TARGET)

# Clean build artifacts
clean:
	rm -f $(OBJ) llm.so $(TARGET)

# Create bootable disk image
disk: $(TARGET)
	@echo "Creating EFI disk image..."
	dd if=/dev/zero of=llm-disk.img bs=1M count=64
	mkfs.fat -F32 llm-disk.img
	mmd -i llm-disk.img ::/EFI
	mmd -i llm-disk.img ::/EFI/BOOT
	mcopy -i llm-disk.img $(TARGET) ::/EFI/BOOT/BOOTX64.EFI
	@echo "✓ Disk image created: llm-disk.img"

# Run in QEMU
run: disk
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
	                   -drive format=raw,file=llm-disk.img \
	                   -m 512M \
	                   -serial mon:stdio

.PHONY: all clean disk run
