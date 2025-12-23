@echo off
echo Starting QEMU with LLM Bare-Metal...
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=llm-baremetal-usb.img,format=raw,media=disk -m 2048 -cpu qemu64,+sse2 -vga std"
pause
