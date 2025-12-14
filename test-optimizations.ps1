# Quick Test Script
# Tests the improvements from Karpathy and Justine

Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║      🚀 TESTING KARPATHY + JUSTINE OPTIMIZATIONS             ║" -ForegroundColor Cyan
Write-Host "╠════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║  ✅ ARM-optimized expf() (Justine's code)                     ║" -ForegroundColor Green
Write-Host "║  ✅ WaitForEvent keyboard input (no busy-wait)                ║" -ForegroundColor Green
Write-Host "║  ✅ Interactive model selection menu                          ║" -ForegroundColor Green
Write-Host "║  ✅ Logits debug for comparison with llama2.c                 ║" -ForegroundColor Green
Write-Host "╠════════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║  📊 Expected logits (reference):                              ║" -ForegroundColor Yellow
Write-Host "║     [0]=-6.7908 [1]=0.8281 [2]=-6.7904...                    ║" -ForegroundColor White
Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "🔍 Comparing with llama2.c reference..." -ForegroundColor Yellow
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llama2.c && ./run stories15M.bin -t 0.8 -n 50 2>&1 | head -20"

Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━`n" -ForegroundColor Gray

Write-Host "🎯 Now launching QEMU to test llama2.efi..." -ForegroundColor Yellow
Write-Host "   Compare the [DEBUG] logits output with reference above`n" -ForegroundColor Gray

Start-Process -FilePath "C:\Program Files\qemu\qemu-system-x86_64.exe" -ArgumentList `
    "-bios", "C:\Users\djibi\Desktop\baremetal\llm-baremetal\OVMF.fd", `
    "-drive", "file=C:\Users\djibi\Desktop\baremetal\llm-baremetal\llama2_efi.img,format=raw", `
    "-m", "2048M", `
    "-cpu", "qemu64,+sse2", `
    "-smp", "2"

Write-Host "`n✅ QEMU launched!" -ForegroundColor Green
Write-Host "   Look for:" -ForegroundColor Yellow
Write-Host "   • Model selection menu (press 1)" -ForegroundColor Gray
Write-Host "   • [DEBUG pos=0] First 10 logits: ..." -ForegroundColor Gray
Write-Host "   • Generated text quality" -ForegroundColor Gray
