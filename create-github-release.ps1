# Script pour créer une GitHub Release et uploader les assets
# Repository: djibydiop/llm-baremetal

param(
    [string]$Version = "v1.0",
    [string]$ReleaseName = "Baremetal LLM v1.0 - Made in Senegal 🇸🇳",
    [switch]$Draft = $false
)

Write-Host "======================================" -ForegroundColor Cyan
Write-Host "  GITHUB RELEASE CREATOR" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"

# Vérifier que gh CLI est installé
Write-Host "🔍 Vérification GitHub CLI..." -ForegroundColor Yellow
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Host "❌ GitHub CLI (gh) n'est pas installé" -ForegroundColor Red
    Write-Host ""
    Write-Host "Installation:" -ForegroundColor Yellow
    Write-Host "  winget install --id GitHub.cli" -ForegroundColor Gray
    Write-Host "  ou télécharger depuis https://cli.github.com" -ForegroundColor Gray
    exit 1
}
Write-Host "✓ GitHub CLI trouvé" -ForegroundColor Green

# Vérifier authentification
Write-Host "🔐 Vérification authentification..." -ForegroundColor Yellow
$authStatus = gh auth status 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Non authentifié avec GitHub" -ForegroundColor Red
    Write-Host ""
    Write-Host "Authentification:" -ForegroundColor Yellow
    Write-Host "  gh auth login" -ForegroundColor Gray
    exit 1
}
Write-Host "✓ Authentifié avec GitHub" -ForegroundColor Green
Write-Host ""

# Fichiers à uploader
$assets = @(
    @{Name="stories15M.bin"; Size=58MB; Required=$true},
    @{Name="tokenizer.bin"; Size=434KB; Required=$true},
    @{Name="llama2.efi"; Size=700KB; Required=$true},
    @{Name="llm-baremetal-complete.img"; Size=512MB; Required=$false},
    @{Name="llm-network-boot.img"; Size=2MB; Required=$false}
)

Write-Host "📦 Vérification des fichiers assets..." -ForegroundColor Yellow
$filesToUpload = @()

foreach ($asset in $assets) {
    if (Test-Path $asset.Name) {
        $actualSize = (Get-Item $asset.Name).Length / 1MB
        Write-Host "  ✓ $($asset.Name) ($($actualSize.ToString('0.00')) MB)" -ForegroundColor Green
        $filesToUpload += $asset.Name
    } elseif ($asset.Required) {
        Write-Host "  ✗ $($asset.Name) MANQUANT (requis)" -ForegroundColor Red
        exit 1
    } else {
        Write-Host "  ⚠ $($asset.Name) non trouvé (optionnel)" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Fichiers à uploader: $($filesToUpload.Count)" -ForegroundColor Cyan
Write-Host ""

# Créer le body de la release
$releaseBody = @"
# 🚀 Baremetal LLM with Network Boot

**Made in Senegal 🇸🇳**

## ✨ Features

- **Bare-metal LLM**: Stories15M (15M parameters) running on UEFI x86_64
- **DRC v5.1**: Djibion Reasoning Core with token 3 suppression
- **Network Boot**: Download models directly from GitHub Releases (no LFS limits!)
- **WiFi Stack**: WPA2 crypto from scratch (550 lines)
- **2 MB Network Boot**: Minimal image downloads everything on boot

## 📦 Assets

### Required Files
- \`stories15M.bin\` (58 MB) - Stories15M model weights
- \`tokenizer.bin\` (434 KB) - Tokenizer for text generation
- \`llama2.efi\` (720 KB) - UEFI bootable binary

### Bootable Images
- \`llm-network-boot.img\` (2 MB) - Minimal network boot (downloads from GitHub)
- \`llm-baremetal-complete.img\` (512 MB) - Complete standalone image with fallback

## 🚀 Quick Start

### Option 1: Network Boot (2 MB - Recommended)
\`\`\`bash
# Flash with Rufus (DD mode)
# Boot with Ethernet/WiFi connected
# System downloads from GitHub automatically!
\`\`\`

### Option 2: Complete Boot (512 MB)
\`\`\`bash
# Flash llm-baremetal-complete.img
# Boot (works offline with network fallback)
\`\`\`

## 📖 Documentation

- [README.md](https://github.com/djibydiop/llm-baremetal/blob/main/README.md) - Full documentation
- [ROADMAP_POST_BOOT.md](https://github.com/djibydiop/llm-baremetal/blob/main/ROADMAP_POST_BOOT.md) - Future features (DRC Consensus, P2P Mesh)

## 🔗 URLs

These assets are automatically downloaded when using network boot:

\`\`\`
https://github.com/djibydiop/llm-baremetal/releases/download/$Version/stories15M.bin
https://github.com/djibydiop/llm-baremetal/releases/download/$Version/tokenizer.bin
\`\`\`

## 🛡️ Coming Soon

- **DRC Network Consensus**: Boot validation by multiple servers (January 2026)
- **P2P LLM Mesh**: First bare-metal LLM cluster (February 2026)
- **Live Model Migration**: Hot-swap models without reboot (March 2026)

## 📊 Technical Details

- **Architecture**: x86_64 UEFI
- **Compiler**: GCC with GNU-EFI
- **Model**: Stories15M (6 layers × 288 dim)
- **Binary Size**: 720 KB (optimized)
- **Dependencies**: None (runs on bare metal)

---

**Built with ❤️ in Senegal 🇸🇳**
"@

Write-Host "📝 Release Notes:" -ForegroundColor Yellow
Write-Host $releaseBody -ForegroundColor Gray
Write-Host ""

# Confirmation
Write-Host "🎯 Release Configuration:" -ForegroundColor Yellow
Write-Host "  Version: $Version" -ForegroundColor Cyan
Write-Host "  Name: $ReleaseName" -ForegroundColor Cyan
Write-Host "  Draft: $Draft" -ForegroundColor Cyan
Write-Host "  Files: $($filesToUpload.Count) assets" -ForegroundColor Cyan
Write-Host ""

$confirm = Read-Host "Créer cette release? (y/N)"
if ($confirm -ne "y" -and $confirm -ne "Y") {
    Write-Host "❌ Annulé" -ForegroundColor Red
    exit 0
}

Write-Host ""
Write-Host "🚀 Création de la release..." -ForegroundColor Yellow

# Créer la release
$releaseArgs = @(
    "release", "create", $Version,
    "--title", $ReleaseName,
    "--notes", $releaseBody,
    "--repo", "djibydiop/llm-baremetal"
)

if ($Draft) {
    $releaseArgs += "--draft"
}

# Ajouter les fichiers
$releaseArgs += $filesToUpload

Write-Host "Commande: gh $($releaseArgs -join ' ')" -ForegroundColor Gray
Write-Host ""

try {
    & gh @releaseArgs
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "=====================================" -ForegroundColor Green
        Write-Host "  ✅ RELEASE CRÉÉE AVEC SUCCÈS!" -ForegroundColor Green
        Write-Host "=====================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "🔗 URLs des assets:" -ForegroundColor Yellow
        foreach ($file in $filesToUpload) {
            $url = "https://github.com/djibydiop/llm-baremetal/releases/download/$Version/$file"
            Write-Host "  $url" -ForegroundColor Cyan
        }
        Write-Host ""
        Write-Host "📖 Voir la release: https://github.com/djibydiop/llm-baremetal/releases/tag/$Version" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "🎉 Le boot network peut maintenant télécharger depuis GitHub Releases!" -ForegroundColor Green
    } else {
        Write-Host "❌ Erreur lors de la création de la release" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "❌ Erreur: $_" -ForegroundColor Red
    exit 1
}
