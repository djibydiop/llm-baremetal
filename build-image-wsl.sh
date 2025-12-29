#!/bin/bash
# One-command build + image (WSL)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "== Build (Makefile: REPL default) =="
make clean
make

echo "== Create bootable image (mtools, no sudo) =="
./create-boot-mtools.sh

echo "== Done =="
ls -lh llama2.efi llm-baremetal-boot.img
