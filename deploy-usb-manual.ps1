# Déploiement USB Manuel - Spécifier la lettre
# Usage: .\deploy-usb-manual.ps1 E
# (ou F, G, etc. selon votre système)

param(
    [Parameter(Mandatory=$false)]
    [string]$DriveLetter
)

$ErrorActionPreference = "Stop"

Write-Host "`n=== DÉPLOIEMENT USB MANUEL ===" -ForegroundColor Cyan

# Si pas de lettre fournie, afficher l'aide
if (!$DriveLetter) {
    Write-Host "`n📝 ÉTAPES POUR ASSIGNER UNE LETTRE:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  MÉTHODE 1 - Gestion des disques (GUI):" -ForegroundColor Cyan
    Write-Host "  1. Win + X → Gestion des disques" -ForegroundColor White
    Write-Host "  2. Trouver votre clé USB (LLM-BOOT ou General UDisk)" -ForegroundColor White
    Write-Host "  3. Clic droit → Modifier la lettre..." -ForegroundColor White
    Write-Host "  4. Ajouter → Choisir E: (ou autre)" -ForegroundColor White
    Write-Host "  5. OK → Fermer" -ForegroundColor White
    Write-Host ""
    Write-Host "  MÉTHODE 2 - Explorateur Windows:" -ForegroundColor Cyan
    Write-Host "  1. Ouvrir 'Ce PC'" -ForegroundColor White
    Write-Host "  2. Si la clé apparaît, noter sa lettre (E:, F:, etc.)" -ForegroundColor White
    Write-Host ""
    Write-Host "  PUIS RELANCER:" -ForegroundColor Cyan
    Write-Host "  .\deploy-usb-manual.ps1 E" -ForegroundColor Green
    Write-Host "  (remplacer E par votre lettre)" -ForegroundColor White
    Write-Host ""
    
    # Afficher les lecteurs actuels
    Write-Host "📀 LECTEURS ACTUELS:" -ForegroundColor Yellow
    Get-PSDrive -PSProvider FileSystem | Where-Object { $_.Name.Length -eq 1 } | 
        Select-Object Name, @{Name='Espace (GB)';Expression={[math]::Round($_.Free/1GB,1)}} | 
        Format-Table -AutoSize
    
    exit 0
}

# Nettoyer la lettre
$DriveLetter = $DriveLetter.TrimEnd(':').ToUpper()
$usbPath = "${DriveLetter}:"

# Vérifier que le lecteur existe
if (!(Test-Path $usbPath)) {
    Write-Host "✗ Lecteur $usbPath introuvable!" -ForegroundColor Red
    Write-Host "  Vérifier dans 'Ce PC' ou Gestion des disques" -ForegroundColor Yellow
    exit 1
}

Write-Host "✓ Lecteur $usbPath détecté`n" -ForegroundColor Green

# Vérifier FAT32
$volume = Get-Volume -DriveLetter $DriveLetter -ErrorAction SilentlyContinue
if ($volume -and $volume.FileSystem -ne "FAT32") {
    Write-Host "⚠️  Système: $($volume.FileSystem) (UEFI recommande FAT32)" -ForegroundColor Yellow
}

# Vérifier fichiers sources
Write-Host "Vérification fichiers sources..." -ForegroundColor Cyan

$files = @(
    @{Name="llama2.efi"; Required=$true},
    @{Name="stories15M.bin"; Required=$true},
    @{Name="tokenizer.bin"; Required=$true}
)

$allPresent = $true
foreach ($file in $files) {
    if (Test-Path $file.Name) {
        $size = (Get-Item $file.Name).Length
        if ($size -gt 1MB) {
            $sizeStr = "$([math]::Round($size/1MB,1)) MB"
        } else {
            $sizeStr = "$([math]::Round($size/1KB,1)) KB"
        }
        Write-Host "  ✓ $($file.Name) ($sizeStr)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $($file.Name) MANQUANT!" -ForegroundColor Red
        $allPresent = $false
    }
}

if (!$allPresent) {
    Write-Host "`n✗ Fichiers manquants!" -ForegroundColor Red
    exit 1
}

Write-Host "`n📦 Création structure UEFI..." -ForegroundColor Cyan
$efiPath = "$usbPath\EFI\BOOT"
New-Item -Path $efiPath -ItemType Directory -Force | Out-Null
Write-Host "  ✓ $efiPath" -ForegroundColor Green

Write-Host "`n📂 Copie des fichiers..." -ForegroundColor Cyan

Write-Host "  → BOOTX64.EFI..." -ForegroundColor Yellow
Copy-Item "llama2.efi" -Destination "$efiPath\BOOTX64.EFI" -Force
Write-Host "    ✓" -ForegroundColor Green

Write-Host "  → stories15M.bin..." -ForegroundColor Yellow
Copy-Item "stories15M.bin" -Destination "$usbPath\stories15M.bin" -Force
Write-Host "    ✓" -ForegroundColor Green

Write-Host "  → tokenizer.bin..." -ForegroundColor Yellow
Copy-Item "tokenizer.bin" -Destination "$usbPath\tokenizer.bin" -Force
Write-Host "    ✓" -ForegroundColor Green

# README
$readme = @"
═══════════════════════════════════════════════════════════
    LLM BARE-METAL v3.0 - UEFI BOOT SYSTEM
    URS Extended + ML Training + Quality Improvements
═══════════════════════════════════════════════════════════

CONTENU:
  • EFI/BOOT/BOOTX64.EFI - Bootloader UEFI (URS v3.0)
  • stories15M.bin       - Modèle LLaMA2 (15M, 58MB)
  • tokenizer.bin        - Tokenizer BPE (32K vocab)

DÉMARRAGE:
  1. Brancher clé USB sur PC UEFI
  2. Redémarrer + F12/F11/ESC (menu boot)
  3. Sélectionner USB en mode UEFI
  4. Boot automatique!

NOUVEAUTÉS v3.0:
  ✅ URS ML Training - apprentissage automatique
  ✅ Température 0.9 - texte plus créatif
  ✅ Répétition penalty 2.5x - pas de boucles
  ✅ Top-p sampling - qualité améliorée
  ✅ Cache 64 entrées - stratégies optimisées

SPECS:
  • RAM: 512MB min (1GB recommandé)
  • CPU: x86-64 avec SSE2
  • BIOS: UEFI (Secure Boot OFF)
  • Stockage: 128MB min

Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm')
"@

$readme | Out-File -FilePath "$usbPath\README.txt" -Encoding UTF8 -Force
Write-Host "  → README.txt..." -ForegroundColor Yellow
Write-Host "    ✓" -ForegroundColor Green

# Résumé
Write-Host "`n╔══════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   DÉPLOIEMENT USB TERMINÉ AVEC SUCCÈS   ║" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "📊 CONTENU FINAL ($usbPath):" -ForegroundColor Cyan
Get-ChildItem $usbPath -Recurse -File | 
    Select-Object @{Name='Fichier';Expression={$_.FullName.Replace($usbPath+'\','')}}, 
                  @{Name='Taille';Expression={
                      if ($_.Length -gt 1MB) { "$([math]::Round($_.Length/1MB,1)) MB" }
                      else { "$([math]::Round($_.Length/1KB,1)) KB" }
                  }} | Format-Table -AutoSize

Write-Host "`n✅ CLÉ USB PRÊTE!" -ForegroundColor Green
Write-Host "`n🚀 PROCHAINES ÉTAPES:" -ForegroundColor Yellow
Write-Host "  1. Éjecter la clé USB en toute sécurité" -ForegroundColor White
Write-Host "  2. Brancher sur PC cible" -ForegroundColor White
Write-Host "  3. Redémarrer + F12 (menu boot)" -ForegroundColor White
Write-Host "  4. Choisir USB en mode UEFI (pas Legacy!)" -ForegroundColor White
Write-Host "  5. Profiter du LLM bare-metal!" -ForegroundColor White
Write-Host "`n💡 ASTUCES:" -ForegroundColor Cyan
Write-Host "  • Désactiver Secure Boot si problème" -ForegroundColor White
Write-Host "  • Première génération: 30-60 sec" -ForegroundColor White
Write-Host "  • Texte généré: créatif avec temp 0.9`n" -ForegroundColor White
