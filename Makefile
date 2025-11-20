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
OBJ = llm_efi.o
CHATBOT_OBJ = llm_chatbot.o
HELLO_OBJ = hello_efi.o

# Default target
all: $(TARGET) $(CHATBOT) $(HELLO)

# Compile C to object file
$(OBJ): llm_efi.c
	$(CC) $(CFLAGS) -c llm_efi.c -o $(OBJ)

# Compile chatbot
$(CHATBOT_OBJ): llm_chatbot.c
	$(CC) $(CFLAGS) -c llm_chatbot.c -o $(CHATBOT_OBJ)

# Link to EFI shared object
llm.so: $(OBJ)
	ld $(LDFLAGS) $(OBJ) -o llm.so $(LIBS)

# Convert to EFI executable
$(TARGET): llm.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) llm.so $(TARGET)

# Link chatbot
chatbot.so: $(CHATBOT_OBJ)
	ld $(LDFLAGS) $(CHATBOT_OBJ) -o chatbot.so $(LIBS)

# Convert chatbot to EFI
$(CHATBOT): chatbot.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) chatbot.so $(CHATBOT)

# Compile hello world
$(HELLO_OBJ): hello_efi.c
	$(CC) $(CFLAGS) -c hello_efi.c -o $(HELLO_OBJ)

# Link hello world
hello.so: $(HELLO_OBJ)
	ld $(LDFLAGS) $(HELLO_OBJ) -o hello.so $(LIBS)

# Convert hello to EFI
$(HELLO): hello.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) hello.so $(HELLO)

# Clean build artifacts
clean:
	rm -f $(OBJ) llm.so $(TARGET) $(CHATBOT_OBJ) chatbot.so $(CHATBOT) $(HELLO_OBJ) hello.so $(HELLO)

# Create bootable disk image with chatbot
disk: $(CHATBOT)
	@echo "Creating EFI disk image with chatbot..."
	dd if=/dev/zero of=llm-disk.img bs=1M count=64
	mkfs.fat -F32 llm-disk.img
	mmd -i llm-disk.img ::/EFI
	mmd -i llm-disk.img ::/EFI/BOOT
	mcopy -i llm-disk.img $(CHATBOT) ::/EFI/BOOT/BOOTX64.EFI
	@echo "✓ Disk image created: llm-disk.img (chatbot mode)"

# Create bootable disk with hello world (for testing)
hello-disk: $(HELLO)
	@echo "Creating EFI disk image with hello world..."
	dd if=/dev/zero of=hello-disk.img bs=1M count=16
	mkfs.fat -F32 hello-disk.img
	mmd -i hello-disk.img ::/EFI
	mmd -i hello-disk.img ::/EFI/BOOT
	mcopy -i hello-disk.img $(HELLO) ::/EFI/BOOT/BOOTX64.EFI
	@echo "✓ Disk image created: hello-disk.img (test mode)"

# Test hello world in QEMU
test-hello: hello-disk
	@echo "🎬 Testing Hello World in QEMU..."
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
	                   -drive format=raw,file=hello-disk.img \
	                   -m 256M \
	                   -serial mon:stdio

# Run in QEMU
run: disk
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
	                   -drive format=raw,file=llm-disk.img \
	                   -m 512M \
	                   -serial mon:stdio

.PHONY: all clean disk run
