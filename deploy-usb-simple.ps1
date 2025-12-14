# Déploiement USB Simplifié - LLM Bare-Metal
# Usage: .\deploy-usb-simple.ps1

$ErrorActionPreference = "Stop"

Write-Host "`n=== DÉPLOIEMENT USB BARE-METAL ===" -ForegroundColor Cyan
Write-Host "URS v3.0 + Température 0.9 + Répétition Penalty`n" -ForegroundColor Yellow

# Trouver la clé USB
Write-Host "[1/5] Détection clé USB..." -ForegroundColor Cyan
$usbDisks = Get-Disk | Where-Object { $_.BusType -eq 'USB' }

if ($usbDisks.Count -eq 0) {
    Write-Host "  ✗ Aucune clé USB détectée!" -ForegroundColor Red
    Write-Host "  → Brancher une clé USB (minimum 2GB)" -ForegroundColor Yellow
    exit 1
}

$disk = $usbDisks[0]
Write-Host "  ✓ Clé USB trouvée: $($disk.FriendlyName) ($([math]::Round($disk.Size/1GB,1)) GB)" -ForegroundColor Green

# Vérifier si montée
$volumes = Get-Volume | Where-Object { $_.DriveType -eq 'Removable' -and $_.DriveLetter -ne $null }

if ($volumes.Count -eq 0) {
    Write-Host "`n  ⚠️  ATTENTION: Clé USB non montée!" -ForegroundColor Yellow
    Write-Host "  → Ouvrir 'Gestion des disques' (diskmgmt.msc)" -ForegroundColor Yellow
    Write-Host "  → Assigner une lettre de lecteur à la clé USB" -ForegroundColor Yellow
    Write-Host "  → Relancer ce script`n" -ForegroundColor Yellow
    
    # Essayer de monter automatiquement
    Write-Host "  Tentative de montage automatique..." -ForegroundColor Cyan
    Start-Process "diskmgmt.msc"
    exit 1
}

$volume = $volumes[0]
$driveLetter = $volume.DriveLetter
$usbPath = "${driveLetter}:"

Write-Host "  ✓ Lecteur: $usbPath" -ForegroundColor Green
Write-Host "  ✓ Système: $($volume.FileSystem)" -ForegroundColor Green
Write-Host "  ✓ Espace: $([math]::Round($volume.SizeRemaining/1MB,1)) MB libres" -ForegroundColor Green

# Vérifier FAT32
if ($volume.FileSystem -ne "FAT32") {
    Write-Host "`n  ⚠️  AVERTISSEMENT: Le système de fichiers est $($volume.FileSystem)" -ForegroundColor Yellow
    Write-Host "  → UEFI nécessite FAT32 pour booter" -ForegroundColor Yellow
    Write-Host "  → Reformater en FAT32 si boot échoue`n" -ForegroundColor Yellow
    Start-Sleep -Seconds 3
}

# Vérifier fichiers sources
Write-Host "`n[2/5] Vérification fichiers sources..." -ForegroundColor Cyan

if (!(Test-Path "llama2.efi")) {
    Write-Host "  ✗ llama2.efi manquant!" -ForegroundColor Red
    Write-Host "  → Compiler d'abord: make clean && make" -ForegroundColor Yellow
    exit 1
}
Write-Host "  ✓ llama2.efi ($([math]::Round((Get-Item 'llama2.efi').Length/1KB,1)) KB)" -ForegroundColor Green

if (!(Test-Path "stories15M.bin")) {
    Write-Host "  ✗ stories15M.bin manquant!" -ForegroundColor Red
    Write-Host "  → Télécharger le modèle" -ForegroundColor Yellow
    exit 1
}
Write-Host "  ✓ stories15M.bin ($([math]::Round((Get-Item 'stories15M.bin').Length/1MB,1)) MB)" -ForegroundColor Green

if (!(Test-Path "tokenizer.bin")) {
    Write-Host "  ✗ tokenizer.bin manquant!" -ForegroundColor Red
    exit 1
}
Write-Host "  ✓ tokenizer.bin ($([math]::Round((Get-Item 'tokenizer.bin').Length/1KB,1)) KB)" -ForegroundColor Green

# Créer structure EFI
Write-Host "`n[3/5] Création structure EFI..." -ForegroundColor Cyan
$efiPath = "$usbPath\EFI\BOOT"
if (!(Test-Path $efiPath)) {
    New-Item -Path $efiPath -ItemType Directory -Force | Out-Null
    Write-Host "  ✓ $efiPath créé" -ForegroundColor Green
} else {
    Write-Host "  ✓ $efiPath existe" -ForegroundColor Green
}

# Copier fichiers
Write-Host "`n[4/5] Copie des fichiers..." -ForegroundColor Cyan

Write-Host "  → Copie llama2.efi -> BOOTX64.EFI..." -ForegroundColor Yellow
Copy-Item "llama2.efi" -Destination "$efiPath\BOOTX64.EFI" -Force
Write-Host "    ✓ BOOTX64.EFI" -ForegroundColor Green

Write-Host "  → Copie stories15M.bin..." -ForegroundColor Yellow
Copy-Item "stories15M.bin" -Destination "$usbPath\stories15M.bin" -Force
Write-Host "    ✓ stories15M.bin" -ForegroundColor Green

Write-Host "  → Copie tokenizer.bin..." -ForegroundColor Yellow
Copy-Item "tokenizer.bin" -Destination "$usbPath\tokenizer.bin" -Force
Write-Host "    ✓ tokenizer.bin" -ForegroundColor Green

# Créer README
Write-Host "`n[5/5] Création README..." -ForegroundColor Cyan
$readme = @"
╔══════════════════════════════════════════════════════════════╗
║       LLM BARE-METAL - UEFI BOOT SYSTEM v3.0               ║
║  Inférence LLaMA2 sans OS - URS Extended avec ML Training  ║
╚══════════════════════════════════════════════════════════════╝

📦 CONTENU:
  • EFI/BOOT/BOOTX64.EFI - Bootloader UEFI
  • stories15M.bin       - Modèle LLaMA2 (15M paramètres, 58MB)
  • tokenizer.bin        - Tokenizer BPE (32K vocab, 434KB)

🚀 DÉMARRAGE:
  1. Brancher la clé USB sur un PC compatible UEFI
  2. Redémarrer et accéder au menu de boot (F12/F11/ESC/DEL)
  3. Sélectionner la clé USB en mode UEFI (pas Legacy!)
  4. Le système démarre automatiquement

⚙️ SYSTÈME:
  • Architecture: x86-64 UEFI bare-metal
  • Moteur mathématique: URS Extended v3.0
    - Solar/Lunar/Elemental/Quantum engines
    - ML training actif avec cache (64 entrées)
    - Stratégies adaptatives avec exponential moving average
  • Génération de texte:
    - Température: 0.9 (créativité élevée)
    - Top-p sampling: nucleus 0.9
    - Répétition penalty: 2.5x progressif (FIFO 64 tokens)
  • Instructions: SSE2 seulement (compatible tout x86-64)

📊 CARACTÉRISTIQUES:
  • Pas d'OS requis - boot direct depuis USB
  • Modèle: 15M paramètres (60MB)
  • Vocabulaire: 32000 tokens BPE
  • Contexte: 256 tokens
  • Architecture: 6 layers, 6 heads, dim 288

🎯 AMÉLIORATIONS v3.0:
  ✅ URS ML Training - apprentissage automatique
  ✅ Cache de solutions (LRU 64 entrées)
  ✅ Taux d'apprentissage adaptatif (0.01 → 0.001)
  ✅ Validation ground truth (sqrt, exp, softmax)
  ✅ Température 0.9 (vs 0.0 greedy avant)
  ✅ Répétition penalty progressif 2.5x
  ✅ Top-p nucleus sampling actif

📈 PERFORMANCE:
  • Boot: ~10-30 secondes
  • Chargement modèle: ~5-10 secondes
  • Génération: ~1-5 tokens/sec (CPU dépendant)
  • Training URS: 9 itérations sqrt en <1 sec

🔧 CONFIGURATION:
  • RAM minimale: 512MB (recommandé: 1-2GB)
  • CPU: x86-64 avec SSE2 (Intel 2003+, AMD 2005+)
  • BIOS: UEFI (mode Secure Boot désactivé recommandé)
  • Stockage: 128MB minimum

💡 NOTES:
  • Si boot échoue, désactiver Secure Boot dans BIOS
  • Clé USB doit être FAT32 (pas exFAT/NTFS)
  • Pour modèle 110M (420MB), remplacer stories15M.bin

🌐 SOURCE:
  GitHub: llm-baremetal
  Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm')
  Build: URS v3.0 ML Training Edition
"@

$readme | Out-File -FilePath "$usbPath\README.txt" -Encoding UTF8 -Force
Write-Host "  ✓ README.txt" -ForegroundColor Green

# Résumé final
Write-Host "`n╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║              DÉPLOIEMENT USB TERMINÉ AVEC SUCCÈS            ║" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Green

Write-Host "`n📊 CONTENU DE LA CLÉ USB ($usbPath):" -ForegroundColor Cyan
Get-ChildItem $usbPath -Recurse | Where-Object { !$_.PSIsContainer } | 
    Select-Object @{Name='Fichier';Expression={$_.FullName.Replace($usbPath+'\','')}}, 
                  @{Name='Taille';Expression={
                      if ($_.Length -gt 1MB) { "$([math]::Round($_.Length/1MB,1)) MB" }
                      else { "$([math]::Round($_.Length/1KB,1)) KB" }
                  }} | Format-Table -AutoSize

Write-Host "`n✅ PRÊT À BOOTER!" -ForegroundColor Green
Write-Host "`n🚀 ÉTAPES SUIVANTES:" -ForegroundColor Yellow
Write-Host "  1. Débrancher la clé USB en toute sécurité" -ForegroundColor White
Write-Host "  2. Brancher sur le PC cible" -ForegroundColor White
Write-Host "  3. Redémarrer et appuyer sur F12 (ou F11/ESC/DEL selon PC)" -ForegroundColor White
Write-Host "  4. Sélectionner 'USB HDD' ou nom de la clé en mode UEFI" -ForegroundColor White
Write-Host "  5. Le système démarre automatiquement!`n" -ForegroundColor White

Write-Host "⚠️  IMPORTANT:" -ForegroundColor Yellow
Write-Host "  • Choisir mode UEFI (pas Legacy/CSM)" -ForegroundColor White
Write-Host "  • Désactiver Secure Boot si boot échoue" -ForegroundColor White
Write-Host "  • Avoir patience: première génération peut prendre 30-60 sec`n" -ForegroundColor White
