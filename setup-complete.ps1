# 🚀 SETUP COMPLET - Boot Network + DRC Consensus
# Made in Senegal 🇸🇳

Write-Host "=====================================" -ForegroundColor Cyan
Write-Host "  LLM BAREMETAL - SETUP COMPLET" -ForegroundColor Cyan
Write-Host "=====================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"

# 1. Vérifier fichiers nécessaires
Write-Host "📦 Vérification des fichiers..." -ForegroundColor Yellow
$requiredFiles = @("llama2.efi", "stories15M.bin", "tokenizer.bin")
$allPresent = $true

foreach ($file in $requiredFiles) {
    if (Test-Path $file) {
        $size = (Get-Item $file).Length / 1MB
        Write-Host "  ✓ $file ($($size.ToString('0.00')) MB)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $file MANQUANT" -ForegroundColor Red
        $allPresent = $false
    }
}

if (-not $allPresent) {
    Write-Host "`n❌ Certains fichiers sont manquants. Compilez d'abord avec 'make'." -ForegroundColor Red
    exit 1
}

Write-Host "`n✅ Tous les fichiers présents!" -ForegroundColor Green
Write-Host ""

# 2. Mettre à jour les images bootables
Write-Host "💾 Mise à jour des images bootables..." -ForegroundColor Yellow

# Image complete (512 MB)
if (Test-Path "llm-baremetal-complete.img") {
    Write-Host "  Mise à jour de llm-baremetal-complete.img..."
    wsl mcopy -o -i llm-baremetal-complete.img@@1M llama2.efi ::/EFI/BOOT/BOOTX64.EFI
    Write-Host "  ✓ Image complète mise à jour (512 MB)" -ForegroundColor Green
} else {
    Write-Host "  ℹ Image complète non trouvée, créez-la avec create-complete-usb.ps1" -ForegroundColor Yellow
}

# Image network (2 MB)
if (Test-Path "llm-network-boot.img") {
    Write-Host "  Mise à jour de llm-network-boot.img..."
    wsl mcopy -o -i llm-network-boot.img@@1M llama2.efi ::/EFI/BOOT/BOOTX64.EFI
    Write-Host "  ✓ Image network mise à jour (2 MB)" -ForegroundColor Green
} else {
    Write-Host "  ℹ Image network non trouvée, créez-la avec create-network-boot-usb.ps1" -ForegroundColor Yellow
}

Write-Host ""

# 3. Configuration Git
Write-Host "🔧 Configuration Git..." -ForegroundColor Yellow

if (-not (Test-Path ".git")) {
    Write-Host "  Initialisation Git..."
    git init
    Write-Host "  ✓ Git initialisé" -ForegroundColor Green
} else {
    Write-Host "  ✓ Git déjà initialisé" -ForegroundColor Green
}

# Vérifier remote
$remoteExists = git remote get-url origin 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "  Ajout du remote origin..."
    git remote add origin https://github.com/djibydiop/llm-baremetal.git
    Write-Host "  ✓ Remote ajouté" -ForegroundColor Green
} else {
    Write-Host "  ✓ Remote déjà configuré: $remoteExists" -ForegroundColor Green
}

# Git LFS
Write-Host "  Configuration Git LFS pour gros fichiers..."
git lfs install 2>&1 | Out-Null
git lfs track "*.bin" 2>&1 | Out-Null
git lfs track "*.img" 2>&1 | Out-Null
git lfs track "*.efi" 2>&1 | Out-Null
Write-Host "  ✓ Git LFS configuré" -ForegroundColor Green

Write-Host ""

# 4. DRC Validator Setup
Write-Host "🛡️  Configuration DRC Validator..." -ForegroundColor Yellow

if (-not (Test-Path "drc-validator")) {
    Write-Host "  ❌ Dossier drc-validator manquant" -ForegroundColor Red
} else {
    Write-Host "  ✓ Serveur validator prêt dans drc-validator/" -ForegroundColor Green
    Write-Host "    Pour démarrer: cd drc-validator && python validator_server.py" -ForegroundColor Gray
}

Write-Host ""

# 5. Résumé des URLs GitHub
Write-Host "🌐 URLs GitHub configurées:" -ForegroundColor Yellow
Write-Host "  Repository: https://github.com/djibydiop/llm-baremetal" -ForegroundColor Cyan
Write-Host "  Raw URLs:" -ForegroundColor Gray
Write-Host "    - https://raw.githubusercontent.com/djibydiop/llm-baremetal/main/stories15M.bin" -ForegroundColor Gray
Write-Host "    - https://raw.githubusercontent.com/djibydiop/llm-baremetal/main/tokenizer.bin" -ForegroundColor Gray
Write-Host "    - https://raw.githubusercontent.com/djibydiop/llm-baremetal/main/llama2.efi" -ForegroundColor Gray

Write-Host ""

# 6. Prochaines étapes
Write-Host "=====================================" -ForegroundColor Cyan
Write-Host "  PROCHAINES ÉTAPES" -ForegroundColor Cyan
Write-Host "=====================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "1️⃣  UPLOAD SUR GITHUB:" -ForegroundColor Yellow
Write-Host "    git add ." -ForegroundColor Gray
Write-Host "    git commit -m 'Initial commit: Baremetal LLM - Made in Senegal 🇸🇳'" -ForegroundColor Gray
Write-Host "    git branch -M main" -ForegroundColor Gray
Write-Host "    git push -u origin main" -ForegroundColor Gray
Write-Host ""

Write-Host "2️⃣  TESTER BOOT NETWORK:" -ForegroundColor Yellow
Write-Host "    - Flasher llm-network-boot.img (2 MB) avec Rufus (mode DD)" -ForegroundColor Gray
Write-Host "    - Connecter Ethernet/WiFi" -ForegroundColor Gray
Write-Host "    - Boot sur le PC" -ForegroundColor Gray
Write-Host "    - Le système télécharge depuis GitHub automatiquement!" -ForegroundColor Gray
Write-Host ""

Write-Host "3️⃣  DÉMARRER DRC CONSENSUS (après validation boot):" -ForegroundColor Yellow
Write-Host "    cd drc-validator" -ForegroundColor Gray
Write-Host "    pip install -r requirements.txt" -ForegroundColor Gray
Write-Host "    python validator_server.py" -ForegroundColor Gray
Write-Host ""

Write-Host "📚 Documentation:" -ForegroundColor Yellow
Write-Host "    - GITHUB_UPLOAD.md : Instructions upload détaillées" -ForegroundColor Gray
Write-Host "    - ROADMAP_POST_BOOT.md : Plan complet 2026" -ForegroundColor Gray
Write-Host "    - drc-validator/README.md : Guide DRC Consensus" -ForegroundColor Gray
Write-Host ""

Write-Host "=====================================" -ForegroundColor Green
Write-Host "  ✅ SETUP TERMINÉ!" -ForegroundColor Green
Write-Host "=====================================" -ForegroundColor Green
Write-Host ""
Write-Host "Made in Senegal 🇸🇳" -ForegroundColor Cyan
