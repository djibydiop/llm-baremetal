@echo off
echo ========================================
echo QEMU Test: LlamaUltimate v7.2 SPECULATIVE
echo ========================================
echo.
echo Features:
echo - Speculative Decoding (draft + verify)
echo - High-Precision Timing (EFI_TIME)
echo - Real-time Speedup Metrics
echo.
echo Models:
echo - Draft: stories15M.bin (60M params)
echo - Target: stories110M.bin (110M params)
echo.
echo Prompt: "Once upon a time" (court pour test rapide)
echo.
pause

cd /d "%~dp0"

qemu-system-x86_64 ^
  -bios C:\Users\djibi\Desktop\baremetal\OVMF.fd ^
  -drive format=raw,file=fat:rw:. ^
  -m 4096 ^
  -cpu qemu64 ^
  -net none ^
  -serial stdio ^
  -nographic

pause
