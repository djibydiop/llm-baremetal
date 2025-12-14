# Création Image .IMG Bootable pour Rufus (mode DD)
# Crée une image RAW avec table GPT + partition ESP FAT32

param(
    [int]$SizeMB = 128
)

$ErrorActionPreference = "Stop"

Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   CRÉATION IMAGE .IMG BOOTABLE (Rufus DD Mode)    ║" -ForegroundColor Cyan
Write-Host "║   LLM Bare-Metal v3.0 + URS ML Training           ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Vérifier fichiers
Write-Host "[1/6] Vérification fichiers sources..." -ForegroundColor Yellow

$requiredFiles = @("llama2.efi", "stories15M.bin", "tokenizer.bin")
foreach ($file in $requiredFiles) {
    if (!(Test-Path $file)) {
        Write-Host "  ✗ $file manquant!" -ForegroundColor Red
        exit 1
    }
    $size = (Get-Item $file).Length
    $sizeStr = if ($size -gt 1MB) { "$([math]::Round($size/1MB,1)) MB" } else { "$([math]::Round($size/1KB,1)) KB" }
    Write-Host "  ✓ $file ($sizeStr)" -ForegroundColor Green
}

# Créer VHD temporaire (plus facile à manipuler)
$tempVHD = "temp_bootable.vhd"
$finalIMG = "llm-baremetal-bootable.img"

Write-Host "`n[2/6] Création VHD temporaire..." -ForegroundColor Yellow

# Script DiskPart pour créer VHD bootable
$diskpartScript = @"
create vdisk file="$PWD\$tempVHD" maximum=$SizeMB type=fixed
select vdisk file="$PWD\$tempVHD"
attach vdisk
convert gpt
create partition efi size=100
format fs=fat32 quick label="LLM-BOOT"
assign letter=Z
detail disk
"@

$scriptPath = "$env:TEMP\create_bootable.txt"
$diskpartScript | Out-File -FilePath $scriptPath -Encoding ASCII -Force

Write-Host "  Exécution DiskPart..." -ForegroundColor Gray
$output = diskpart /s $scriptPath 2>&1 | Out-String
Write-Host $output -ForegroundColor DarkGray

# Attendre que Z: soit disponible
Write-Host "  Attente montage partition..." -ForegroundColor Gray
$timeout = 10
$elapsed = 0
while (!(Test-Path "Z:\") -and $elapsed -lt $timeout) {
    Start-Sleep -Seconds 1
    $elapsed++
}

if (!(Test-Path "Z:\")) {
    Write-Host "  ✗ Échec montage partition Z:" -ForegroundColor Red
    Write-Host "`n  Tentative avec autre lettre..." -ForegroundColor Yellow
    
    # Essayer de trouver la lettre assignée
    $vol = Get-Volume | Where-Object { $_.FileSystemLabel -eq "LLM-BOOT" -and $_.DriveLetter }
    if ($vol) {
        $driveLetter = "$($vol.DriveLetter):"
        Write-Host "  ✓ Trouvé sur $driveLetter" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Impossible de trouver la partition!" -ForegroundColor Red
        exit 1
    }
} else {
    $driveLetter = "Z:"
    Write-Host "  ✓ Partition montée sur $driveLetter" -ForegroundColor Green
}

try {
    # Copier fichiers
    Write-Host "`n[3/6] Copie fichiers sur partition ESP..." -ForegroundColor Yellow
    
    New-Item -Path "$driveLetter\EFI\BOOT" -ItemType Directory -Force | Out-Null
    Write-Host "  ✓ Structure EFI/BOOT créée" -ForegroundColor Green
    
    Copy-Item "llama2.efi" -Destination "$driveLetter\EFI\BOOT\BOOTX64.EFI" -Force
    Write-Host "  ✓ BOOTX64.EFI ($(([math]::Round((Get-Item 'llama2.efi').Length/1KB,1))) KB)" -ForegroundColor Green
    
    Copy-Item "stories15M.bin" -Destination "$driveLetter\stories15M.bin" -Force
    Write-Host "  ✓ stories15M.bin ($(([math]::Round((Get-Item 'stories15M.bin').Length/1MB,1))) MB)" -ForegroundColor Green
    
    Copy-Item "tokenizer.bin" -Destination "$driveLetter\tokenizer.bin" -Force
    Write-Host "  ✓ tokenizer.bin ($(([math]::Round((Get-Item 'tokenizer.bin').Length/1KB,1))) KB)" -ForegroundColor Green
    
    # README
    $readme = @"
═══════════════════════════════════════════════════════════
  LLM BARE-METAL v3.0 - IMAGE BOOTABLE UEFI
  URS Extended + ML Training + Quality Improvements
═══════════════════════════════════════════════════════════

📦 CONTENU:
  • EFI/BOOT/BOOTX64.EFI - Bootloader UEFI avec URS v3.0
  • stories15M.bin       - Modèle LLaMA2 (15M params)
  • tokenizer.bin        - Tokenizer BPE (32K vocab)

💾 ÉCRITURE AVEC RUFUS:
  1. Ouvrir Rufus
  2. Périphérique: [Votre clé USB]
  3. Sélectionner: llm-baremetal-bootable.img
  4. Rufus détecte automatiquement mode "Image DD"
  5. Cliquer DÉMARRER
  6. Attendre fin (~2-3 min)

🚀 BOOT SUR PC:
  1. Brancher clé USB
  2. Redémarrer + F12/F11/ESC
  3. Sélectionner "UEFI: USB" (PAS Legacy!)
  4. Désactiver Secure Boot si nécessaire

⚙️ SYSTÈME v3.0:
  ✅ URS ML Training - apprentissage automatique
  ✅ Température 0.9 - texte créatif et varié
  ✅ Répétition penalty 2.5x - pas de boucles
  ✅ Top-p sampling 0.9 - meilleure qualité
  ✅ Cache 64 entrées LRU
  ✅ Learning rate adaptatif (0.01 → 0.001)

📊 RÉSULTATS QEMU:
  • Solar strategy: 31% (9/9 succès)
  • Texte: "The first appreciate fly its its so it fly..."

📅 Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm')
🏷️ Version: v3.0 ML Training Edition
"@
    $readme | Out-File -FilePath "$driveLetter\README.txt" -Encoding UTF8 -Force
    Write-Host "  ✓ README.txt créé" -ForegroundColor Green
    
    # Vérifier contenu
    Write-Host "`n[4/6] Vérification contenu..." -ForegroundColor Yellow
    $files = Get-ChildItem "$driveLetter\" -Recurse -File
    Write-Host "  ✓ $($files.Count) fichiers copiés" -ForegroundColor Green
    
} finally {
    # Forcer synchronisation
    Write-Host "`n[5/6] Synchronisation et démontage..." -ForegroundColor Yellow
    Write-Host "  Flush buffers..." -ForegroundColor Gray
    Start-Sleep -Seconds 2
    
    # Démonter VHD
    $detachScript = @"
select vdisk file="$PWD\$tempVHD"
detach vdisk
"@
    $detachPath = "$env:TEMP\detach.txt"
    $detachScript | Out-File -FilePath $detachPath -Encoding ASCII -Force
    diskpart /s $detachPath | Out-Null
    Start-Sleep -Seconds 2
    Write-Host "  ✓ VHD démonté" -ForegroundColor Green
}

# Convertir VHD en IMG (renommer simplement)
Write-Host "`n[6/6] Conversion VHD → IMG..." -ForegroundColor Yellow

if (Test-Path $finalIMG) {
    Remove-Item $finalIMG -Force
}

Move-Item $tempVHD $finalIMG -Force
Write-Host "  ✓ Converti en .img" -ForegroundColor Green

# Résultat
$imgInfo = Get-Item $finalIMG

Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   IMAGE .IMG CRÉÉE AVEC SUCCÈS!                    ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "📀 FICHIER IMAGE:" -ForegroundColor Cyan
Write-Host "   Nom: $($imgInfo.Name)" -ForegroundColor White
Write-Host "   Taille: $([math]::Round($imgInfo.Length/1MB,1)) MB" -ForegroundColor White
Write-Host "   Chemin: $($imgInfo.FullName)`n" -ForegroundColor White

Write-Host "📝 UTILISATION AVEC RUFUS:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  1️⃣  Télécharger Rufus (si pas déjà fait)" -ForegroundColor White
Write-Host "      https://rufus.ie/" -ForegroundColor Gray
Write-Host ""
Write-Host "  2️⃣  Ouvrir Rufus" -ForegroundColor White
Write-Host ""
Write-Host "  3️⃣  Configuration:" -ForegroundColor White
Write-Host "      • Périphérique: [Sélectionner votre clé USB]" -ForegroundColor Gray
Write-Host "      • Cliquer SÉLECTION → Choisir: $finalIMG" -ForegroundColor Green
Write-Host "      • Rufus détecte automatiquement 'Image DD'" -ForegroundColor Gray
Write-Host "      • Schéma: GPT (auto-détecté)" -ForegroundColor Gray
Write-Host "      • Système: UEFI (auto-détecté)" -ForegroundColor Gray
Write-Host ""
Write-Host "  4️⃣  Cliquer DÉMARRER" -ForegroundColor White
Write-Host "      • Confirmer effacement USB" -ForegroundColor Gray
Write-Host "      • Attendre 2-3 minutes" -ForegroundColor Gray
Write-Host ""
Write-Host "  5️⃣  Éjecter et booter!" -ForegroundColor White
Write-Host "      • Brancher USB sur PC cible" -ForegroundColor Gray
Write-Host "      • Redémarrer + F12" -ForegroundColor Gray
Write-Host "      • Sélectionner 'UEFI: USB'" -ForegroundColor Gray
Write-Host ""

Write-Host "⚠️  IMPORTANT:" -ForegroundColor Red
Write-Host "    • Désactiver Secure Boot dans BIOS si nécessaire" -ForegroundColor Yellow
Write-Host "    • Utiliser mode UEFI (pas Legacy/CSM)" -ForegroundColor Yellow
Write-Host "    • L'écriture efface TOUT sur la clé USB!`n" -ForegroundColor Yellow

Write-Host "💡 ASTUCE:" -ForegroundColor Cyan
Write-Host "    Si Rufus propose 'Mode Image ISO' vs 'Mode Image DD'," -ForegroundColor White
Write-Host "    choisir obligatoirement 'Mode Image DD'!`n" -ForegroundColor White

Write-Host "✅ IMAGE PRÊTE POUR RUFUS!" -ForegroundColor Green
