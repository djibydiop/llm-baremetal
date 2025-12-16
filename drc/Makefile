# Makefile for DRC Unit Tests
# Tests each new module independently

ARCH = x86_64
CC = gcc
CFLAGS = -ffreestanding -fno-stack-protector -fpic \
         -fshort-wchar -mno-red-zone -I/usr/include/efi \
         -I/usr/include/efi/$(ARCH) -DEFI_FUNCTION_WRAPPER \
         -O3 -I. -I..

LDFLAGS = -nostdlib -znocombreloc -T /usr/lib/elf_$(ARCH)_efi.lds \
          -shared -Bsymbolic -L/usr/lib \
          /usr/lib/crt0-efi-$(ARCH).o

LIBS = -lefi -lgnuefi

# Test targets
TESTS = test_perf.efi test_config.efi test_trace.efi \
        test_uam.efi test_upe.efi test_uiv.efi

all: $(TESTS)
	@echo "✓ All unit tests compiled!"
	@echo ""
	@echo "Run tests in QEMU:"
	@echo "  make test-perf    # Test Performance Monitoring"
	@echo "  make test-config  # Test Configuration System"
	@echo "  make test-trace   # Test Decision Trace"
	@echo "  make test-uam     # Test Auto-Moderation"
	@echo "  make test-upe     # Test Plausibility Checking"
	@echo "  make test-uiv     # Test Intention & Values"

# Performance Monitoring test
test_perf.o: test_perf.c drc_perf.h
	$(CC) $(CFLAGS) -c test_perf.c -o test_perf.o

drc_perf.o: drc_perf.c drc_perf.h
	$(CC) $(CFLAGS) -c drc_perf.c -o drc_perf.o

test_perf.so: test_perf.o drc_perf.o
	ld $(LDFLAGS) test_perf.o drc_perf.o -o test_perf.so $(LIBS)

test_perf.efi: test_perf.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) test_perf.so test_perf.efi

# Configuration System test
test_config.o: test_config.c drc_config.h
	$(CC) $(CFLAGS) -c test_config.c -o test_config.o

drc_config.o: drc_config.c drc_config.h
	$(CC) $(CFLAGS) -c drc_config.c -o drc_config.o

test_config.so: test_config.o drc_config.o
	ld $(LDFLAGS) test_config.o drc_config.o -o test_config.so $(LIBS)

test_config.efi: test_config.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) test_config.so test_config.efi

# Decision Trace test
test_trace.o: test_trace.c drc_trace.h
	$(CC) $(CFLAGS) -c test_trace.c -o test_trace.o

drc_trace.o: drc_trace.c drc_trace.h
	$(CC) $(CFLAGS) -c drc_trace.c -o drc_trace.o

test_trace.so: test_trace.o drc_trace.o
	ld $(LDFLAGS) test_trace.o drc_trace.o -o test_trace.so $(LIBS)

test_trace.efi: test_trace.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) test_trace.so test_trace.efi

# Auto-Moderation test
test_uam.o: test_uam.c drc_uam.h
	$(CC) $(CFLAGS) -c test_uam.c -o test_uam.o

drc_uam.o: drc_uam.c drc_uam.h
	$(CC) $(CFLAGS) -c drc_uam.c -o drc_uam.o

test_uam.so: test_uam.o drc_uam.o
	ld $(LDFLAGS) test_uam.o drc_uam.o -o test_uam.so $(LIBS)

test_uam.efi: test_uam.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) test_uam.so test_uam.efi

# Plausibility Checking test
test_upe.o: test_upe.c drc_upe.h
	$(CC) $(CFLAGS) -c test_upe.c -o test_upe.o

drc_upe.o: drc_upe.c drc_upe.h
	$(CC) $(CFLAGS) -c drc_upe.c -o drc_upe.o

test_upe.so: test_upe.o drc_upe.o
	ld $(LDFLAGS) test_upe.o drc_upe.o -o test_upe.so $(LIBS)

test_upe.efi: test_upe.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) test_upe.so test_upe.efi

# Intention & Values test
test_uiv.o: test_uiv.c drc_uiv.h
	$(CC) $(CFLAGS) -c test_uiv.c -o test_uiv.o

drc_uiv.o: drc_uiv.c drc_uiv.h
	$(CC) $(CFLAGS) -c drc_uiv.c -o drc_uiv.o

test_uiv.so: test_uiv.o drc_uiv.o
	ld $(LDFLAGS) test_uiv.o drc_uiv.o -o test_uiv.so $(LIBS)

test_uiv.efi: test_uiv.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .reloc \
	        --target=efi-app-$(ARCH) test_uiv.so test_uiv.efi

# QEMU test runners (requires test.img to exist)
test-perf: test_perf.efi
	@echo "Creating test image for Performance Monitoring..."
	@mkfs.vfat -F 32 -C test.img 10240 2>/dev/null || true
	@mmd -i test.img ::EFI 2>/dev/null || true
	@mmd -i test.img ::EFI/BOOT 2>/dev/null || true
	@mcopy -oi test.img test_perf.efi ::EFI/BOOT/BOOTX64.EFI
	@echo "✓ Running Performance Monitoring test..."
	@qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=test.img,format=raw -m 256 -nographic || true

test-config: test_config.efi
	@echo "Creating test image for Configuration System..."
	@mkfs.vfat -F 32 -C test.img 10240 2>/dev/null || true
	@mmd -i test.img ::EFI 2>/dev/null || true
	@mmd -i test.img ::EFI/BOOT 2>/dev/null || true
	@mcopy -oi test.img test_config.efi ::EFI/BOOT/BOOTX64.EFI
	@echo "✓ Running Configuration System test..."
	@qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=test.img,format=raw -m 256 -nographic || true

test-trace: test_trace.efi
	@echo "Creating test image for Decision Trace..."
	@mkfs.vfat -F 32 -C test.img 10240 2>/dev/null || true
	@mmd -i test.img ::EFI 2>/dev/null || true
	@mmd -i test.img ::EFI/BOOT 2>/dev/null || true
	@mcopy -oi test.img test_trace.efi ::EFI/BOOT/BOOTX64.EFI
	@echo "✓ Running Decision Trace test..."
	@qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=test.img,format=raw -m 256 -nographic || true

test-uam: test_uam.efi
	@echo "Creating test image for Auto-Moderation..."
	@mkfs.vfat -F 32 -C test.img 10240 2>/dev/null || true
	@mmd -i test.img ::EFI 2>/dev/null || true
	@mmd -i test.img ::EFI/BOOT 2>/dev/null || true
	@mcopy -oi test.img test_uam.efi ::EFI/BOOT/BOOTX64.EFI
	@echo "✓ Running Auto-Moderation test..."
	@qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=test.img,format=raw -m 256 -nographic || true

test-upe: test_upe.efi
	@echo "Creating test image for Plausibility Checking..."
	@mkfs.vfat -F 32 -C test.img 10240 2>/dev/null || true
	@mmd -i test.img ::EFI 2>/dev/null || true
	@mmd -i test.img ::EFI/BOOT 2>/dev/null || true
	@mcopy -oi test.img test_upe.efi ::EFI/BOOT/BOOTX64.EFI
	@echo "✓ Running Plausibility Checking test..."
	@qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=test.img,format=raw -m 256 -nographic || true

test-uiv: test_uiv.efi
	@echo "Creating test image for Intention & Values..."
	@mkfs.vfat -F 32 -C test.img 10240 2>/dev/null || true
	@mmd -i test.img ::EFI 2>/dev/null || true
	@mmd -i test.img ::EFI/BOOT 2>/dev/null || true
	@mcopy -oi test.img test_uiv.efi ::EFI/BOOT/BOOTX64.EFI
	@echo "✓ Running Intention & Values test..."
	@qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=test.img,format=raw -m 256 -nographic || true

# Clean
clean:
	rm -f *.o *.so *.efi test.img

.PHONY: all clean test-perf test-config test-trace test-uam test-upe test-uiv
