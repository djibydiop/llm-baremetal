#!/bin/bash
# Git setup and push script

cd "$(dirname "$0")"

echo "=== Git Setup and Push ==="
echo ""

# Initialize repo if needed
if [ ! -d .git ]; then
    echo "[1/5] Initializing Git repository..."
    git init
    echo "OK"
else
    echo "[1/5] Git repository already initialized"
fi

# Configure user (change these if needed)
echo ""
echo "[2/5] Configuring Git user..."
git config user.name "npdji"
git config user.email "npdji@users.noreply.github.com"
echo "OK"

# Add essential files
echo ""
echo "[3/5] Adding essential files..."
git add .gitignore
git add Makefile
git add llama2_efi.c
git add test-qemu.sh
git add test-qemu.ps1
git add build-windows.ps1
git add create-usb.ps1
git add download_stories110m.sh
git add README.md
git add QUICK_START.md
git add PERFORMANCE_OPTIMIZATIONS.md
git add OPTIMIZATION_GUIDE.md
git add USB_BOOT_GUIDE.md 2>/dev/null || true
echo "OK"

# Commit
echo ""
echo "[4/5] Committing changes..."
git commit -m "feat: Add stories110M with performance optimizations and interactive menu

- Optimized matmul with 4x loop unrolling (~1.5x speedup)
- Optimized embedding copy with 8x loop unrolling
- Added interactive menu with prompt categories (Stories, Science, Adventure)
- AVX2/FMA SIMD optimizations enabled
- Updated to stories110M (420MB, 110M params)
- Comprehensive documentation (QUICK_START, PERFORMANCE_OPTIMIZATIONS)
- Works on real UEFI hardware and QEMU (Haswell CPU required)
- Auto-demo mode for QEMU testing
- Training scripts for stories101M and stories260M included"

if [ $? -eq 0 ]; then
    echo "OK - Commit created"
else
    echo "SKIP - No changes to commit (or error)"
fi

# Push
echo ""
echo "[5/5] Pushing to GitHub..."
echo ""
echo "To push to GitHub, run:"
echo "  git remote add origin https://github.com/npdji/YamaOO.git"
echo "  git branch -M main"
echo "  git push -u origin main"
echo ""
echo "Or if remote already exists:"
echo "  git push origin main"
echo ""
echo "Done! Check 'git log' to see your commit."
