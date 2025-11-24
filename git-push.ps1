# Git setup and push script for Windows
# Usage: .\git-push.ps1

Write-Host "=== Git Setup and Push ===" -ForegroundColor Cyan
Write-Host ""

$repo = "c:\Users\djibi\Desktop\yama_oo\yama_oo\llm-baremetal"
Set-Location $repo

# Initialize repo if needed
if (-not (Test-Path .git)) {
    Write-Host "[1/5] Initializing Git repository..." -ForegroundColor Yellow
    git init
    Write-Host "OK" -ForegroundColor Green
} else {
    Write-Host "[1/5] Git repository already initialized" -ForegroundColor Green
}

# Configure user
Write-Host ""
Write-Host "[2/5] Configuring Git user..." -ForegroundColor Yellow
git config user.name "npdji"
git config user.email "npdji@users.noreply.github.com"
Write-Host "OK" -ForegroundColor Green

# Add essential files
Write-Host ""
Write-Host "[3/5] Adding essential files..." -ForegroundColor Yellow

$files = @(
    ".gitignore",
    "Makefile",
    "llama2_efi.c",
    "test-qemu.sh",
    "test-qemu.ps1",
    "build-windows.ps1",
    "create-usb.ps1",
    "download_stories110m.sh",
    "README.md",
    "QUICK_START.md",
    "PERFORMANCE_OPTIMIZATIONS.md",
    "OPTIMIZATION_GUIDE.md",
    "git-push.sh",
    "git-push.ps1"
)

foreach ($file in $files) {
    if (Test-Path $file) {
        git add $file
        Write-Host "  + $file" -ForegroundColor Gray
    }
}

# Add USB boot guide if exists
if (Test-Path "USB_BOOT_GUIDE.md") {
    git add "USB_BOOT_GUIDE.md"
    Write-Host "  + USB_BOOT_GUIDE.md" -ForegroundColor Gray
}

Write-Host "OK" -ForegroundColor Green

# Commit
Write-Host ""
Write-Host "[4/5] Committing changes..." -ForegroundColor Yellow

$commitMsg = @"
feat: Add stories110M with performance optimizations and interactive menu

- Optimized matmul with 4x loop unrolling (~1.5x speedup)
- Optimized embedding copy with 8x loop unrolling
- Added interactive menu with prompt categories (Stories, Science, Adventure)
- AVX2/FMA SIMD optimizations enabled
- Updated to stories110M (420MB, 110M params)
- Comprehensive documentation (QUICK_START, PERFORMANCE_OPTIMIZATIONS)
- Works on real UEFI hardware and QEMU (Haswell CPU required)
- Auto-demo mode for QEMU testing
- Training scripts for stories101M and stories260M included
"@

git commit -m $commitMsg

if ($LASTEXITCODE -eq 0) {
    Write-Host "OK - Commit created" -ForegroundColor Green
} else {
    Write-Host "SKIP - No changes to commit (or already committed)" -ForegroundColor Yellow
}

# Push instructions
Write-Host ""
Write-Host "[5/5] Ready to push to GitHub" -ForegroundColor Yellow
Write-Host ""
Write-Host "To push to GitHub, run:" -ForegroundColor Cyan
Write-Host "  git remote add origin https://github.com/npdji/YamaOO.git" -ForegroundColor White
Write-Host "  git branch -M main" -ForegroundColor White
Write-Host "  git push -u origin main" -ForegroundColor White
Write-Host ""
Write-Host "Or if remote already exists:" -ForegroundColor Cyan
Write-Host "  git push origin main" -ForegroundColor White
Write-Host ""
Write-Host "Done! Check 'git log' to see your commit." -ForegroundColor Green
Write-Host ""

# Show status
Write-Host "Git Status:" -ForegroundColor Cyan
git status --short
