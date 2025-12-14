#!/usr/bin/env pwsh
# Test l'interface colorée dans QEMU
# Made in Senegal by Djiby Diop

Write-Host "`n=== Testing Bare-Metal Neural LLM ===" -ForegroundColor Cyan
Write-Host "Lancement de QEMU avec l'image..." -ForegroundColor Yellow
Write-Host ""

wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=qemu-test.img,format=raw -m 512'

Write-Host "`nQEMU terminé." -ForegroundColor Green
