# Makefile for Llama2 Bare-Metal UEFI (stable REPL build)
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

# Stable build: chat REPL (single-file + kernel primitives)
TARGET = llama2.efi
REPL_SRC = llama2_efi_final.c
REPL_OBJ = llama2_repl.o
REPL_OBJS = $(REPL_OBJ) llmk_zones.o llmk_log.o llmk_sentinel.o djiblas.o djiblas_avx2.o attention_avx2.o
REPL_SO  = llama2_repl.so

all: repl

repl: $(TARGET)
	@echo "✅ Build complete: $(TARGET)"
	@ls -lh $(TARGET)

$(REPL_OBJ): $(REPL_SRC) djiblas.h
	$(CC) $(CFLAGS) -c $(REPL_SRC) -o $(REPL_OBJ)

llmk_zones.o: llmk_zones.c llmk_zones.h
	$(CC) $(CFLAGS) -c llmk_zones.c -o llmk_zones.o

llmk_log.o: llmk_log.c llmk_log.h llmk_zones.h
	$(CC) $(CFLAGS) -c llmk_log.c -o llmk_log.o

llmk_sentinel.o: llmk_sentinel.c llmk_sentinel.h llmk_zones.h llmk_log.h
	$(CC) $(CFLAGS) -c llmk_sentinel.c -o llmk_sentinel.o

$(REPL_SO): $(REPL_OBJS)
	ld $(LDFLAGS) $(REPL_OBJS) -o $(REPL_SO) $(LIBS)

$(TARGET): $(REPL_SO)
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
			-j .rel -j .rela -j .reloc --target=efi-app-$(ARCH) $(REPL_SO) $(TARGET)

djiblas.o: djiblas.c djiblas.h
	$(CC) $(CFLAGS) -c djiblas.c -o djiblas.o

djiblas_avx2.o: djiblas_avx2.c djiblas.h
	$(CC) $(CFLAGS) -mavx2 -mfma -c djiblas_avx2.c -o djiblas_avx2.o

attention_avx2.o: attention_avx2.c
	$(CC) $(CFLAGS) -mavx2 -mfma -c attention_avx2.c -o attention_avx2.o

clean:
	rm -f *.o *.so $(TARGET)
	@echo "✅ Clean complete"

rebuild: clean all

test: all
	@echo "Creating bootable image..."
	@./create-boot-mtools.sh

