# Script pour uploader les fichiers sur GitHub
# Repository: djibydiop/llm-baremetal

Write-Host "=== Upload vers GitHub - llm-baremetal ===" -ForegroundColor Cyan
Write-Host ""

# Vérifier que le repo existe
$repoPath = "."
if (-not (Test-Path "$repoPath\.git")) {
    Write-Host "ERREUR: Pas de dépôt Git trouvé" -ForegroundColor Red
    Write-Host "Initialiser d'abord avec:" -ForegroundColor Yellow
    Write-Host "  git init" -ForegroundColor Gray
    Write-Host "  git remote add origin https://github.com/djibydiop/llm-baremetal.git" -ForegroundColor Gray
    exit 1
}

# Fichiers à uploader
$files = @(
    "stories15M.bin",
    "tokenizer.bin",
    "llama2.efi",
    "llm-baremetal-complete.img",
    "llm-network-boot.img"
)

Write-Host "Fichiers à uploader:" -ForegroundColor Yellow
foreach ($file in $files) {
    if (Test-Path $file) {
        $size = (Get-Item $file).Length / 1MB
        Write-Host "  ✓ $file ($($size.ToString('0.00')) MB)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $file (manquant)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "ATTENTION:" -ForegroundColor Red
Write-Host "GitHub a une limite de 100 MB par fichier!" -ForegroundColor Yellow
Write-Host ""
Write-Host "Pour les gros fichiers (stories15M.bin, *.img):" -ForegroundColor Cyan
Write-Host "  1. Utiliser GitHub Releases (pas de limite)" -ForegroundColor Gray
Write-Host "  2. Ou Git LFS (Large File Storage)" -ForegroundColor Gray
Write-Host ""

$response = Read-Host "Continuer? (o/n)"
if ($response -ne "o") {
    Write-Host "Annulé" -ForegroundColor Yellow
    exit 0
}

# Git LFS pour les gros fichiers
Write-Host ""
Write-Host "Configuration Git LFS..." -ForegroundColor Cyan
git lfs install

# Track les gros fichiers
git lfs track "*.bin"
git lfs track "*.img"
git lfs track "*.efi"

# Add et commit
Write-Host "Ajout des fichiers..." -ForegroundColor Cyan
git add .gitattributes
git add stories15M.bin tokenizer.bin llama2.efi
git add llm-baremetal-complete.img llm-network-boot.img
git add README.md

git commit -m "Add LLM baremetal boot images and models"

Write-Host ""
Write-Host "Push vers GitHub..." -ForegroundColor Cyan
git push origin main

Write-Host ""
Write-Host "=== UPLOAD TERMINÉ ===" -ForegroundColor Green
Write-Host ""
Write-Host "URLs de téléchargement:" -ForegroundColor Cyan
Write-Host "  stories15M.bin:" -ForegroundColor Yellow
Write-Host "    https://raw.githubusercontent.com/djibsondiop/llm-baremetal/main/stories15M.bin" -ForegroundColor Gray
Write-Host "  tokenizer.bin:" -ForegroundColor Yellow
Write-Host "    https://raw.githubusercontent.com/djibsondiop/llm-baremetal/main/tokenizer.bin" -ForegroundColor Gray
Write-Host ""
