# Legacy / Experimental Entry Points

This folder contains many historical scripts and artifacts produced during debugging. They are kept for reference, but the maintained workflow is:

- Windows: `./build.ps1` then `./run.ps1`
- WSL: `./build-image-wsl.sh`

## Build (legacy)

- `compile-optimized.sh`, `compile-fix.sh`: older build pipelines (kept for experiments).
- `Makefile` target `kernel`: builds `llama2_efi.c` + `djiblas.c` experiments.

## Image creation (legacy)

- `create-bootable-image.ps1`, `create-image-simple.ps1`, `create-bootable-image.sh`, `create-bootable-wsl.sh`: older image builders.
- Current image builder: `create-boot-mtools.sh` (used by the blessed workflow).

## QEMU launchers (legacy)

- `run-qemu.ps1`, `launch-qemu.ps1`, `launch-qemu-simple.ps1`, `run-qemu.sh`, `test-qemu*.sh`, `test-qemu*.ps1`: older launch/test scripts.
- Current QEMU entrypoint: `run.ps1`.

## Diagnostics (legacy)

- `diagnose-boot.ps1`, `debug-boot-failure.ps1`, `verify-image.ps1`: boot/image diagnostics.

## Artifacts (generated)

Many `*.efi`, `*.img`, and `qemu-*.log` files in this directory are build outputs from past sessions. They should not be edited by hand; regenerate via the blessed workflow when needed.
