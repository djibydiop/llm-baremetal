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
all: $(TARGET) $(CHATBOT) $(HELLO) $(LLAMA2)

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

# Compile llama2_efi (95% Karpathy's code)
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
	rm -f $(OBJ) llm.so $(TARGET) $(CHATBOT_OBJ) chatbot.so $(CHATBOT) \
	      $(HELLO_OBJ) hello.so $(HELLO) $(LLAMA2_OBJ) llama2.so $(LLAMA2)

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

# Create disk image with llama2 (stories15M)
llama2-disk: $(LLAMA2)
	@echo "Creating EFI disk image with LLaMA2..."
	dd if=/dev/zero of=llama2-disk.img bs=1M count=128
	mkfs.fat -F32 llama2-disk.img
	mmd -i llama2-disk.img ::/EFI
	mmd -i llama2-disk.img ::/EFI/BOOT
	mcopy -i llama2-disk.img $(LLAMA2) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i llama2-disk.img stories15M.bin ::/
	mcopy -i llama2-disk.img tokenizer.bin ::/
	@echo "✓ Disk image created: llama2-disk.img (15M params)"

# Test llama2 in QEMU
test-llama2: llama2-disk
	@echo "🚀 Testing LLaMA2 (stories15M) in QEMU..."
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
	                   -drive format=raw,file=llama2-disk.img \
	                   -m 512M \
	                   -serial mon:stdio

# Run in QEMU
run: disk
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
	                   -drive format=raw,file=llm-disk.img \
	                   -m 512M \
	                   -serial mon:stdio

.PHONY: all clean disk run test-hello llama2-disk test-llama2
