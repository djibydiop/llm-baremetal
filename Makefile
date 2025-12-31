# Makefile for Llama2 Bare-Metal UEFI - Clean & Simple
# Made in Senegal 🇸🇳

ARCH = x86_64
CC = gcc

# Canonical GNU-EFI build flags (known-good for this project)
CFLAGS = -ffreestanding -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
		 -I/usr/include/efi -I/usr/include/efi/$(ARCH) -DEFI_FUNCTION_WRAPPER \
		 -O2 -msse2

LDFLAGS = -nostdlib -znocombreloc -T /usr/lib/elf_$(ARCH)_efi.lds \
		  -shared -Bsymbolic -L/usr/lib /usr/lib/crt0-efi-$(ARCH).o

LIBS = -lefi -lgnuefi

# Default build: chat REPL (single-file)
TARGET = llama2.efi
REPL_SRC = llama2_efi_final.c
REPL_OBJ = llama2_repl.o
REPL_OBJS = $(REPL_OBJ) djiblas.o
REPL_SO  = llama2_repl.so

# LLM-Kernel (new clean workstream)
LLMK_TARGET = llmkernel.efi
LLMK_SRC = llm_kernel_efi.c
LLMK_OBJS = llm_kernel_efi.o llmk_zones.o llmk_sentinel.o djiblas.o
LLMK_SO = llmkernel.so

# Legacy/optimized build (kept for djiblas experiments)
KERNEL_SRC = llama2_efi.c
KERNEL_OBJS = llama2_efi.o djiblas.o
KERNEL_SO = llama2_efi.so

all: repl

repl: $(TARGET)
	@echo "✅ Build complete: $(TARGET)"
	@ls -lh $(TARGET)

$(REPL_OBJ): $(REPL_SRC) djiblas.h
	$(CC) $(CFLAGS) -c $(REPL_SRC) -o $(REPL_OBJ)

llmkernel: $(LLMK_TARGET)
	@echo "✅ Build complete: $(LLMK_TARGET)"
	@ls -lh $(LLMK_TARGET)

llm_kernel_efi.o: $(LLMK_SRC) djiblas.h llmk_zones.h llmk_sentinel.h
	$(CC) $(CFLAGS) -c $(LLMK_SRC) -o llm_kernel_efi.o

llmk_zones.o: llmk_zones.c llmk_zones.h
	$(CC) $(CFLAGS) -c llmk_zones.c -o llmk_zones.o

llmk_sentinel.o: llmk_sentinel.c llmk_sentinel.h llmk_zones.h
	$(CC) $(CFLAGS) -c llmk_sentinel.c -o llmk_sentinel.o

$(LLMK_SO): $(LLMK_OBJS)
	ld $(LDFLAGS) $(LLMK_OBJS) -o $(LLMK_SO) $(LIBS)

$(LLMK_TARGET): $(LLMK_SO)
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
			-j .rel -j .rela -j .reloc --target=efi-app-$(ARCH) $(LLMK_SO) $(LLMK_TARGET)

$(REPL_SO): $(REPL_OBJS)
	ld $(LDFLAGS) $(REPL_OBJS) -o $(REPL_SO) $(LIBS)

$(TARGET): $(REPL_SO)
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
			-j .rel -j .rela -j .reloc --target=efi-app-$(ARCH) $(REPL_SO) $(TARGET)

kernel: $(KERNEL_SO)
	@echo "✅ Build complete: $(KERNEL_SO)"
	@ls -lh $(KERNEL_SO)

llama2_efi.o: $(KERNEL_SRC) djiblas.h
	$(CC) $(CFLAGS) -c $(KERNEL_SRC) -o llama2_efi.o

djiblas.o: djiblas.c djiblas.h
	$(CC) $(CFLAGS) -c djiblas.c -o djiblas.o

$(KERNEL_SO): $(KERNEL_OBJS)
	ld $(LDFLAGS) $(KERNEL_OBJS) -o $(KERNEL_SO) $(LIBS)

clean:
	rm -f *.o *.so $(TARGET) $(LLMK_TARGET)
	@echo "✅ Clean complete"

rebuild: clean all

test: all
	@echo "Creating bootable image..."
	@./create-boot-mtools.sh

