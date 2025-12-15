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

# Compile wifi_firmware.c
wifi_firmware.o: wifi_firmware.c wifi_firmware.h wifi_ax200.h
	$(CC) $(CFLAGS) -msse2 -c wifi_firmware.c -o wifi_firmware.o

# DRC module compilation - Full cognitive architecture
DRC_DIR = drc
DRC_INCLUDES = -I$(DRC_DIR) -I.
DRC_OBJS = $(DRC_DIR)/drc_urs.o $(DRC_DIR)/drc_modelbridge.o $(DRC_DIR)/drc_verification.o \
           $(DRC_DIR)/drc_uic.o $(DRC_DIR)/drc_ucr.o $(DRC_DIR)/drc_uti.o \
           $(DRC_DIR)/drc_uco.o $(DRC_DIR)/drc_ums.o \
           $(DRC_DIR)/drc_perf.o $(DRC_DIR)/drc_config.o $(DRC_DIR)/drc_trace.o \
           $(DRC_DIR)/drc_uam.o $(DRC_DIR)/drc_upe.o $(DRC_DIR)/drc_uiv.o \
           $(DRC_DIR)/drc_selfdiag.o $(DRC_DIR)/drc_semcluster.o $(DRC_DIR)/drc_timebudget.o \
           $(DRC_DIR)/drc_bias.o $(DRC_DIR)/drc_emergency.o $(DRC_DIR)/drc_radiocog.o \
           drc_integration.o

$(DRC_DIR)/drc_urs.o: $(DRC_DIR)/drc_urs.c $(DRC_DIR)/drc_urs.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_urs.c -o $(DRC_DIR)/drc_urs.o

$(DRC_DIR)/drc_modelbridge.o: $(DRC_DIR)/drc_modelbridge.c $(DRC_DIR)/drc_modelbridge.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_modelbridge.c -o $(DRC_DIR)/drc_modelbridge.o

$(DRC_DIR)/drc_verification.o: $(DRC_DIR)/drc_verification.c $(DRC_DIR)/drc_verification.h $(DRC_DIR)/drc_urs.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_verification.c -o $(DRC_DIR)/drc_verification.o

# Compile new DRC units
$(DRC_DIR)/drc_uic.o: $(DRC_DIR)/drc_uic.c $(DRC_DIR)/drc_uic.h $(DRC_DIR)/drc_verification.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_uic.c -o $(DRC_DIR)/drc_uic.o

$(DRC_DIR)/drc_ucr.o: $(DRC_DIR)/drc_ucr.c $(DRC_DIR)/drc_ucr.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_ucr.c -o $(DRC_DIR)/drc_ucr.o

$(DRC_DIR)/drc_uti.o: $(DRC_DIR)/drc_uti.c $(DRC_DIR)/drc_uti.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_uti.c -o $(DRC_DIR)/drc_uti.o

$(DRC_DIR)/drc_uco.o: $(DRC_DIR)/drc_uco.c $(DRC_DIR)/drc_uco.h $(DRC_DIR)/drc_urs.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_uco.c -o $(DRC_DIR)/drc_uco.o

$(DRC_DIR)/drc_ums.o: $(DRC_DIR)/drc_ums.c $(DRC_DIR)/drc_ums.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_ums.c -o $(DRC_DIR)/drc_ums.o

# Infrastructure modules
$(DRC_DIR)/drc_perf.o: $(DRC_DIR)/drc_perf.c $(DRC_DIR)/drc_perf.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_perf.c -o $(DRC_DIR)/drc_perf.o

$(DRC_DIR)/drc_config.o: $(DRC_DIR)/drc_config.c $(DRC_DIR)/drc_config.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_config.c -o $(DRC_DIR)/drc_config.o

$(DRC_DIR)/drc_trace.o: $(DRC_DIR)/drc_trace.c $(DRC_DIR)/drc_trace.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_trace.c -o $(DRC_DIR)/drc_trace.o

# Additional cognitive units
$(DRC_DIR)/drc_uam.o: $(DRC_DIR)/drc_uam.c $(DRC_DIR)/drc_uam.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_uam.c -o $(DRC_DIR)/drc_uam.o

$(DRC_DIR)/drc_upe.o: $(DRC_DIR)/drc_upe.c $(DRC_DIR)/drc_upe.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_upe.c -o $(DRC_DIR)/drc_upe.o

$(DRC_DIR)/drc_uiv.o: $(DRC_DIR)/drc_uiv.c $(DRC_DIR)/drc_uiv.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_uiv.c -o $(DRC_DIR)/drc_uiv.o

# Phase 3-9: Advanced systems
$(DRC_DIR)/drc_selfdiag.o: $(DRC_DIR)/drc_selfdiag.c $(DRC_DIR)/drc_selfdiag.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_selfdiag.c -o $(DRC_DIR)/drc_selfdiag.o

$(DRC_DIR)/drc_semcluster.o: $(DRC_DIR)/drc_semcluster.c $(DRC_DIR)/drc_semcluster.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_semcluster.c -o $(DRC_DIR)/drc_semcluster.o

$(DRC_DIR)/drc_timebudget.o: $(DRC_DIR)/drc_timebudget.c $(DRC_DIR)/drc_timebudget.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_timebudget.c -o $(DRC_DIR)/drc_timebudget.o

$(DRC_DIR)/drc_bias.o: $(DRC_DIR)/drc_bias.c $(DRC_DIR)/drc_bias.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_bias.c -o $(DRC_DIR)/drc_bias.o

$(DRC_DIR)/drc_emergency.o: $(DRC_DIR)/drc_emergency.c $(DRC_DIR)/drc_emergency.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_emergency.c -o $(DRC_DIR)/drc_emergency.o

$(DRC_DIR)/drc_radiocog.o: $(DRC_DIR)/drc_radiocog.c $(DRC_DIR)/drc_radiocog.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c $(DRC_DIR)/drc_radiocog.c -o $(DRC_DIR)/drc_radiocog.o

# Compile DRC integration layer
drc_integration.o: drc_integration.c drc_integration.h $(DRC_DIR)/drc.h
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c drc_integration.c -o drc_integration.o

# Compile llama2_efi with SSE2 (QEMU-compatible) and DRC includes
$(LLAMA2_OBJ): llama2_efi.c
	$(CC) $(CFLAGS) $(DRC_INCLUDES) -msse2 -c llama2_efi.c -o $(LLAMA2_OBJ)

# Link llama2_efi with all modules
llama2.so: $(LLAMA2_OBJ) network_boot.o wifi_ax200.o wifi_firmware.o $(DRC_OBJS)
	ld $(LDFLAGS) $(LLAMA2_OBJ) network_boot.o wifi_ax200.o wifi_firmware.o $(DRC_OBJS) -o llama2.so $(LIBS)

# Convert llama2 to EFI
$(LLAMA2): llama2.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) llama2.so $(LLAMA2)

# Clean build artifacts
clean:
	rm -f $(LLAMA2_OBJ) network_boot.o wifi_ax200.o wifi_firmware.o llama2.so $(LLAMA2)
	rm -f drc_integration.o
	rm -f $(DRC_DIR)/*.o

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
