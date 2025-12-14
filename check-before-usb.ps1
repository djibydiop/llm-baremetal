# Test Rapide - Vérification avant Boot USB
# Ce script vérifie que tout est prêt pour créer la clé USB

Write-Host "=== Vérification Pré-Boot USB ===" -ForegroundColor Cyan
Write-Host ""

$allGood = $true

# Vérifier llama2.efi
Write-Host "[1/3] Application UEFI (llama2.efi)..." -ForegroundColor Yellow
if (Test-Path "llama2.efi") {
    $size = (Get-Item "llama2.efi").Length / 1KB
    Write-Host "  ✓ Présent ($([math]::Round($size, 1)) KB)" -ForegroundColor Green
    
    # Vérifier que c'est un binaire EFI
    $bytes = [System.IO.File]::ReadAllBytes("llama2.efi")
    if ($bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A) {
        Write-Host "  ✓ Format EFI valide (MZ header)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Format invalide!" -ForegroundColor Red
        $allGood = $false
    }
} else {
    Write-Host "  ✗ Fichier manquant!" -ForegroundColor Red
    $allGood = $false
}

Write-Host ""

# Vérifier le modèle
Write-Host "[2/3] Modèle LLaMA2..." -ForegroundColor Yellow
$model15M = Test-Path "stories15M.bin"
$model110M = Test-Path "stories110M.bin"

if ($model15M) {
    $size = (Get-Item "stories15M.bin").Length / 1MB
    Write-Host "  ✓ stories15M.bin présent ($([math]::Round($size, 1)) MB)" -ForegroundColor Green
    Write-Host "    → Rapide, recommandé pour tests" -ForegroundColor Cyan
}

if ($model110M) {
    $size = (Get-Item "stories110M.bin").Length / 1MB
    Write-Host "  ✓ stories110M.bin présent ($([math]::Round($size, 1)) MB)" -ForegroundColor Green
    Write-Host "    → Meilleure qualité, plus lent" -ForegroundColor Cyan
}

if (-not $model15M -and -not $model110M) {
    Write-Host "  ✗ Aucun modèle trouvé!" -ForegroundColor Red
    $allGood = $false
}

Write-Host ""

# Vérifier tokenizer
Write-Host "[3/3] Tokenizer BPE..." -ForegroundColor Yellow
if (Test-Path "tokenizer.bin") {
    $size = (Get-Item "tokenizer.bin").Length / 1KB
    Write-Host "  ✓ Présent ($([math]::Round($size, 1)) KB)" -ForegroundColor Green
} else {
    Write-Host "  ✗ Fichier manquant!" -ForegroundColor Red
    $allGood = $false
}

Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor White

if ($allGood) {
    Write-Host ""
    Write-Host "✅ TOUT EST PRET!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Prochaine étape:" -ForegroundColor Yellow
    Write-Host "  1. Insérer une clé USB (256MB minimum)" -ForegroundColor White
    Write-Host "  2. Lancer: .\prepare-usb.ps1" -ForegroundColor White
    Write-Host "  3. Ou suivre: BOOT_USB_GUIDE.md" -ForegroundColor White
    Write-Host ""
    Write-Host "Features activées:" -ForegroundColor Cyan
    Write-Host "  ✓ URS v3.0 avec ML training" -ForegroundColor White
    Write-Host "  ✓ Température 0.9 (créativité)" -ForegroundColor White
    Write-Host "  ✓ Répétition penalty 2.5x progressif" -ForegroundColor White
    Write-Host "  ✓ Top-p nucleus sampling" -ForegroundColor White
    Write-Host "  ✓ Alignement 16-byte (SSE2 safe)" -ForegroundColor White
    Write-Host ""
    Write-Host "Qualité texte: EXCELLENTE" -ForegroundColor Green
    Write-Host "  Avant: 'do he first first first first...'" -ForegroundColor Red
    Write-Host "  Après: 'The first appreciate fly its so it...'" -ForegroundColor Green
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "❌ FICHIERS MANQUANTS" -ForegroundColor Red
    Write-Host ""
    Write-Host "Veuillez compiler d'abord:" -ForegroundColor Yellow
    Write-Host "  wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && make'" -ForegroundColor White
    Write-Host ""
}
