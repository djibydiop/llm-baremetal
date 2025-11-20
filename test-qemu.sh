#!/bin/bash
cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal
# Lancer QEMU et capturer stdout/stderr
timeout 8 qemu-system-x86_64 \
    -drive format=raw,file=llm-disk.img \
    -nographic \
    -m 512M \
    2>&1 | tee qemu_output.txt
