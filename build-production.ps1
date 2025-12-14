#!/usr/bin/env pwsh
# BUILD PRODUCTION - Version finale pour démo

Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║         🚀 BUILD PRODUCTION - LLM BARE-METAL            ║" -ForegroundColor Cyan
Write-Host "║         Bug Fixé: sizeof(Config) → 28 bytes             ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Compile
Write-Host "📦 Compilation..." -ForegroundColor Yellow
wsl make clean
wsl make

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Compilation échouée!" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Compilation réussie!`n" -ForegroundColor Green

# Deploy to image
Write-Host "💾 Déploiement vers l'image..." -ForegroundColor Yellow
wsl mcopy -i llama2_efi.img -o llama2.efi ::EFI/BOOT/BOOTX64.EFI

Write-Host "✅ Déployé dans llama2_efi.img!`n" -ForegroundColor Green

# Test in QEMU
Write-Host "🖥️  Test dans QEMU..." -ForegroundColor Yellow
Write-Host "`nVous devriez voir:`n" -ForegroundColor Cyan
Write-Host "  ✓ Chargement du modèle stories15M.bin" -ForegroundColor White
Write-Host "  ✓ Génération: 'Once upon a time, there was...'" -ForegroundColor White
Write-Host "  ✓ ~28 tok/s sur x86_64 QEMU" -ForegroundColor White
Write-Host "  ✓ DRC v4.0 Ultra-Advanced actif`n" -ForegroundColor White

$response = Read-Host "Lancer QEMU maintenant? (o/n)"
if ($response -eq "o" -or $response -eq "O") {
    Start-Process "C:\Program Files\qemu\qemu-system-x86_64.exe" -ArgumentList `
        "-bios","OVMF.fd", `
        "-drive","file=llama2_efi.img,format=raw", `
        "-m","2048M", `
        "-cpu","qemu64,+sse2", `
        "-smp","2"
    
    Write-Host "`n✅ QEMU lancé!`n" -ForegroundColor Green
}

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║              ✅ BUILD PRODUCTION COMPLETE!               ║" -ForegroundColor Green
Write-Host "╠════════════════════════════════════════════════════════════╣" -ForegroundColor White
Write-Host "║  Prochaines étapes:                                        ║" -ForegroundColor White
Write-Host "║  1. ✅ Bug fixé (sizeof Config)                           ║" -ForegroundColor Green
Write-Host "║  2. 🎭 Entraîner Shakespeare: python train_shakespeare_fast.py ║" -ForegroundColor Yellow
Write-Host "║  3. 🎨 Intégrer beautiful_ui.c                            ║" -ForegroundColor Yellow
Write-Host "║  4. 💿 Créer USB bootable                                 ║" -ForegroundColor Yellow
Write-Host "║  5. 🎥 Filmer demo à Dakar                                ║" -ForegroundColor Yellow
Write-Host "║  6. 🌍 Poster sur HN + Twitter (@karpathy)                ║" -ForegroundColor Yellow
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
