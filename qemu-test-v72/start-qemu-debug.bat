@echo off
cd /d "%~dp0"
echo ========================================
echo QEMU v7.2 SPECULATIVE - Debug Mode
echo ========================================
echo.
echo Launching with serial output...
echo.
"C:\Program Files\qemu\qemu-system-x86_64.exe" -L "C:\Program Files\qemu\share" -drive format=raw,file=fat:rw:. -m 4096 -cpu qemu64 -net none -serial stdio
