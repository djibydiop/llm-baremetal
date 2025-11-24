# Test stories110M sur QEMU avec support AVX
# Usage: .\test-qemu.ps1

Write-Host ""
Write-Host "Test stories110M sur QEMU (CPU Haswell avec AVX)..." -ForegroundColor Cyan
Write-Host ""

# Verifier que l'image existe
if (-not (Test-Path "qemu-test.img")) {
    Write-Host "ERROR qemu-test.img introuvable" -ForegroundColor Red
    Write-Host "Creer d'abord l'image avec: wsl bash -c 'make test-image'" -ForegroundColor Yellow
    exit 1
}

# Vérifier les fichiers
$files = @("llama2.efi", "stories110M.bin", "tokenizer.bin")
foreach ($file in $files) {
    if (Test-Path $file) {
        $size = [math]::Round((Get-Item $file).Length / 1MB, 1)
        Write-Host "  OK $file ($size MB)" -ForegroundColor Green
    } else {
        Write-Host "  X $file manquant" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "=========================================" -ForegroundColor Yellow
Write-Host "  Demarrage QEMU avec CPU Haswell (AVX)" -ForegroundColor Yellow
Write-Host "=========================================" -ForegroundColor Yellow
Write-Host ""
Write-Host "Appuyez sur Ctrl+C dans cette fenetre pour arreter" -ForegroundColor White
Write-Host ""

# Lancer QEMU avec CPU Haswell
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=qemu-test.img,format=raw -m 4G -cpu Haswell -nographic -serial mon:stdio 2>&1"
