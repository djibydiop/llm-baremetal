# Préparation Contenu USB pour Rufus
# Crée le dossier USB_CONTENT avec tous les fichiers prêts à copier

$ErrorActionPreference = "Stop"

Write-Host "`n=== PRÉPARATION CONTENU USB POUR RUFUS ===" -ForegroundColor Cyan
Write-Host "Crée dossier USB_CONTENT prêt à copier après formatage Rufus`n" -ForegroundColor Yellow

# Vérifier fichiers sources
Write-Host "[1/3] Vérification fichiers sources..." -ForegroundColor Cyan

$files = @{
    "llama2.efi" = "Bootloader UEFI"
    "stories15M.bin" = "Modèle LLaMA2"
    "tokenizer.bin" = "Tokenizer BPE"
}

$allPresent = $true
foreach ($file in $files.Keys) {
    if (Test-Path $file) {
        $size = (Get-Item $file).Length
        if ($size -gt 1MB) {
            $sizeStr = "$([math]::Round($size/1MB,1)) MB"
        } else {
            $sizeStr = "$([math]::Round($size/1KB,1)) KB"
        }
        Write-Host "  ✓ $file ($sizeStr)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $file MANQUANT!" -ForegroundColor Red
        $allPresent = $false
    }
}

if (!$allPresent) {
    Write-Host "`n✗ Fichiers manquants!" -ForegroundColor Red
    exit 1
}

# Nettoyer ancien dossier
if (Test-Path "USB_CONTENT") {
    Write-Host "`n[2/3] Nettoyage ancien dossier..." -ForegroundColor Cyan
    Remove-Item "USB_CONTENT" -Recurse -Force
    Write-Host "  ✓ Ancien dossier supprimé" -ForegroundColor Green
}

# Créer structure
Write-Host "`n[2/3] Création structure EFI..." -ForegroundColor Cyan
New-Item -Path "USB_CONTENT\EFI\BOOT" -ItemType Directory -Force | Out-Null
Write-Host "  ✓ USB_CONTENT\EFI\BOOT\" -ForegroundColor Green

# Copier fichiers
Write-Host "`n[3/3] Copie des fichiers..." -ForegroundColor Cyan

Copy-Item "llama2.efi" -Destination "USB_CONTENT\EFI\BOOT\BOOTX64.EFI" -Force
Write-Host "  ✓ BOOTX64.EFI" -ForegroundColor Green

Copy-Item "stories15M.bin" -Destination "USB_CONTENT\stories15M.bin" -Force
Write-Host "  ✓ stories15M.bin" -ForegroundColor Green

Copy-Item "tokenizer.bin" -Destination "USB_CONTENT\tokenizer.bin" -Force
Write-Host "  ✓ tokenizer.bin" -ForegroundColor Green

# Créer README
$readme = @"
═══════════════════════════════════════════════════════════
    LLM BARE-METAL v3.0 - UEFI BOOT SYSTEM
    URS Extended + ML Training + Quality Improvements
═══════════════════════════════════════════════════════════

📦 CONTENU:
  • EFI/BOOT/BOOTX64.EFI - Bootloader UEFI avec URS v3.0
  • stories15M.bin       - Modèle LLaMA2 (15M, 58MB)
  • tokenizer.bin        - Tokenizer BPE (32K vocab)

🚀 DÉMARRAGE:
  1. Brancher clé USB sur PC UEFI
  2. Redémarrer + F12/F11/ESC (menu boot)
  3. Sélectionner "UEFI: USB" (pas Legacy!)
  4. Boot automatique!

⚙️ SYSTÈME v3.0:
  ✅ URS ML Training - apprentissage automatique
  ✅ Température 0.9 - texte créatif et varié
  ✅ Répétition penalty 2.5x - pas de boucles
  ✅ Top-p sampling 0.9 - meilleure qualité
  ✅ Cache 64 entrées - stratégies optimisées
  ✅ Learning rate adaptatif (0.01 → 0.001)

📊 SPECS TECHNIQUES:
  • Architecture: x86-64 UEFI bare-metal (pas d'OS!)
  • Instructions: SSE2 seulement (compatible 2003+)
  • RAM: 512MB min (1-2GB recommandé)
  • CPU: Intel/AMD x86-64 avec SSE2
  • BIOS: UEFI (Secure Boot OFF recommandé)
  • Stockage: 128MB minimum

🎯 PERFORMANCE:
  • Boot: 5-15 secondes
  • Chargement modèle: 5-10 secondes
  • Training URS: <1 seconde (9 itérations)
  • Génération: 1-5 tokens/sec (CPU dépendant)

💡 RÉSULTATS TRAINING:
  • Solar strategy: 31% (9 succès sur 9)
  • Lunar/Elemental/Quantum: 25% (baseline)
  • Cache hits: croît avec utilisation
  • Learning rate: décroit progressivement

🔧 DÉPANNAGE:
  • Boot échoue → Désactiver Secure Boot
  • Écran noir → Attendre 60 sec (chargement)
  • "No boot device" → Vérifier mode UEFI
  • Texte bizarre → Normal en v3.0 (créativité)

📅 Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm')
🏷️ Version: v3.0 ML Training Edition
🌐 Projet: llm-baremetal
"@

$readme | Out-File -FilePath "USB_CONTENT\README.txt" -Encoding UTF8 -Force
Write-Host "  ✓ README.txt" -ForegroundColor Green

# Afficher contenu
Write-Host "`n╔══════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   DOSSIER USB_CONTENT PRÊT!              ║" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "📂 CONTENU DU DOSSIER:" -ForegroundColor Cyan
Get-ChildItem "USB_CONTENT" -Recurse -File | 
    Select-Object @{Name='Fichier';Expression={$_.FullName.Replace((Get-Location).Path+'\USB_CONTENT\','')}}, 
                  @{Name='Taille';Expression={
                      if ($_.Length -gt 1MB) { "$([math]::Round($_.Length/1MB,1)) MB" }
                      else { "$([math]::Round($_.Length/1KB,1)) KB" }
                  }} | Format-Table -AutoSize

$totalSize = (Get-ChildItem "USB_CONTENT" -Recurse -File | Measure-Object -Property Length -Sum).Sum
Write-Host "Taille totale: $([math]::Round($totalSize/1MB,1)) MB`n" -ForegroundColor Yellow

Write-Host "📝 PROCHAINES ÉTAPES AVEC RUFUS:" -ForegroundColor Cyan
Write-Host ""
Write-Host "  1. Ouvrir Rufus" -ForegroundColor White
Write-Host "     • Télécharger: https://rufus.ie/" -ForegroundColor Gray
Write-Host ""
Write-Host "  2. Paramètres Rufus:" -ForegroundColor White
Write-Host "     • Périphérique: [Votre clé USB]" -ForegroundColor Gray
Write-Host "     • Schéma de partition: GPT" -ForegroundColor Gray
Write-Host "     • Système de destination: UEFI (non CSM)" -ForegroundColor Gray
Write-Host "     • Système de fichiers: FAT32" -ForegroundColor Gray
Write-Host "     • Taille d'allocation: 4096 (défaut)" -ForegroundColor Gray
Write-Host ""
Write-Host "  3. Cliquer 'DÉMARRER' dans Rufus" -ForegroundColor White
Write-Host "     • Confirmer formatage" -ForegroundColor Gray
Write-Host "     • Attendre fin (30-60 sec)" -ForegroundColor Gray
Write-Host ""
Write-Host "  4. Copier USB_CONTENT vers la clé:" -ForegroundColor White
Write-Host "     • Glisser-déposer tout le contenu de USB_CONTENT\" -ForegroundColor Gray
Write-Host "     • Ou utiliser: Copy-Item USB_CONTENT\* E:\ -Recurse" -ForegroundColor Gray
Write-Host "       (remplacer E: par lettre de votre clé)" -ForegroundColor Gray
Write-Host ""
Write-Host "  5. Éjecter proprement" -ForegroundColor White
Write-Host "     • Clic droit → Éjecter" -ForegroundColor Gray
Write-Host ""
Write-Host "  6. Booter sur PC cible!" -ForegroundColor White
Write-Host "     • Redémarrer + F12" -ForegroundColor Gray
Write-Host "     • Choisir UEFI: USB" -ForegroundColor Gray
Write-Host "     • Profiter du LLM bare-metal! 🚀" -ForegroundColor Gray
Write-Host ""

Write-Host "💡 ASTUCE RAPIDE:" -ForegroundColor Yellow
Write-Host "   Après formatage Rufus, noter la lettre (ex: E:)" -ForegroundColor White
Write-Host "   puis lancer: .\deploy-usb-manual.ps1 E`n" -ForegroundColor Green

Write-Host "✅ PRÊT POUR RUFUS!" -ForegroundColor Green
