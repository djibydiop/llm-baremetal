# Makefile for llm-baremetal EFI - Essential Files Only
# Created by Djiby Diop - Made in Senegal 🇸🇳

ARCH = x86_64
CC = gcc
CFLAGS = -ffreestanding -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
         -I/usr/include/efi -I/usr/include/efi/$(ARCH) -DEFI_FUNCTION_WRAPPER \
         -O2 -msse2 -fno-asynchronous-unwind-tables

LDFLAGS = -nostdlib -znocombreloc -T /usr/lib/elf_$(ARCH)_efi.lds \
          -shared -Bsymbolic -L/usr/lib /usr/lib/crt0-efi-$(ARCH).o

LIBS = -lefi -lgnuefi

LLAMA2 = llama2.efi

all: $(LLAMA2)

djiblas.o: djiblas.c djiblas.h
	$(CC) $(CFLAGS) -mavx2 -mfma -c djiblas.c -o djiblas.o

llama2_efi.o: llama2_efi.c heap_allocator.h matmul_optimized.h
	$(CC) $(CFLAGS) -c llama2_efi.c -o llama2_efi.o

llama2.so: llama2_efi.o djiblas.o
	ld $(LDFLAGS) llama2_efi.o djiblas.o -o llama2.so $(LIBS)

$(LLAMA2): llama2.so
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	        -j .rel -j .rela -j .reloc --target=efi-app-$(ARCH) llama2.so $(LLAMA2)

clean:
	rm -f llama2_efi.o djiblas.o llama2.so $(LLAMA2)

.PHONY: all clean


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
llama2.so: $(LLAMA2_OBJ) network_boot.o wifi_ax200.o wifi_firmware.o wifi_wpa2.o wifi_802_11.o drc_v5.o model_streaming.o drc_consensus.o p2p_llm_mesh.o drc_selfmod.o crbc.o djiblas.o $(DRC_OBJS)
	ld $(LDFLAGS) $(LLAMA2_OBJ) network_boot.o wifi_ax200.o wifi_firmware.o wifi_wpa2.o wifi_802_11.o drc_v5.o model_streaming.o drc_consensus.o p2p_llm_mesh.o drc_selfmod.o crbc.o djiblas.o $(DRC_OBJS) -o llama2.so $(LIBS)

# Convert llama2 to EFI
$(LLAMA2): llama2.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) llama2.so $(LLAMA2)

# Compile test_llm_kernel (LLM-Kernel foundation tests)
$(TEST_KERNEL_OBJ): test_llm_kernel.c memory_zones.h memory_sentinel.h
	$(CC) $(CFLAGS) -msse2 -c test_llm_kernel.c -o $(TEST_KERNEL_OBJ)

# Link test_llm_kernel
test_llm_kernel.so: $(TEST_KERNEL_OBJ)
	ld $(LDFLAGS) $(TEST_KERNEL_OBJ) -o test_llm_kernel.so $(LIBS)

# Convert to EFI
$(TEST_KERNEL): test_llm_kernel.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) test_llm_kernel.so $(TEST_KERNEL)

# Test LLM-Kernel foundations in QEMU
test-kernel: $(TEST_KERNEL)
	@echo "Creating test disk image..."
	dd if=/dev/zero of=test-kernel.img bs=1M count=16
	mkfs.fat -F32 test-kernel.img
	mmd -i test-kernel.img ::/EFI
	mmd -i test-kernel.img ::/EFI/BOOT
	mcopy -i test-kernel.img $(TEST_KERNEL) ::/EFI/BOOT/BOOTX64.EFI
	@echo "Testing LLM-Kernel in QEMU..."
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
	                   -drive format=raw,file=test-kernel.img \
	                   -m 2048M \
	                   -serial mon:stdio

# Test target (compile and run unit tests)
test: test_llm_baremetal
	./test_llm_baremetal

test_llm_baremetal: test_llm_baremetal.c djiblas.h
	gcc -O3 -mavx2 -mfma -msse2 -o test_llm_baremetal test_llm_baremetal.c -lm

# Clean build artifacts
clean:
	rm -f $(LLAMA2_OBJ) network_boot.o wifi_ax200.o wifi_firmware.o wifi_wpa2.o wifi_802_11.o drc_v5.o model_streaming.o drc_consensus.o p2p_llm_mesh.o drc_selfmod.o crbc.o djiblas.o llama2.so $(LLAMA2)
	rm -f drc_integration.o
	rm -f $(DRC_DIR)/*.o
	rm -f test_llm_baremetal test_llm_baremetal.exe

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

# LLM-Kernel test targets
test-kernel-quick: llama2.efi
	@echo "🧪 Quick LLM-Kernel test (15s)..."
	@bash quick-qemu-test.sh

test-kernel-full: llama2.efi
	@echo "🧪 Full LLM-Kernel test (60s)..."
	@bash test-qemu-full.sh

test-kernel-final: llama2.efi
	@echo "🚀 Final LLM-Kernel test (120s)..."
	@bash test-final-kernel.sh

# Rebuild disk with latest EFI binary
qemu-test.img: llama2.efi stories15M.bin tokenizer.bin
	@echo "Creating QEMU test disk image..."
	@rm -f qemu-test.img
	@dd if=/dev/zero of=qemu-test.img bs=1M count=128 2>&1 | grep -v records
	@mkfs.fat -F32 qemu-test.img 2>&1 | head -1
	@mmd -i qemu-test.img ::/EFI
	@mmd -i qemu-test.img ::/EFI/BOOT
	@mcopy -i qemu-test.img llama2.efi ::/EFI/BOOT/BOOTX64.EFI
	@mcopy -i qemu-test.img stories15M.bin tokenizer.bin ::/
	@echo "✅ Disk image ready (128 MB)"

# Default run target
run: test-llama2

.PHONY: all clean llama2-disk test-llama2 run usb-image test-kernel test-kernel-quick test-kernel-full test-kernel-final
