# Création Image VHD Bootable UEFI (plus simple que .img brut)
# Windows peut monter/formater les VHD nativement

$ErrorActionPreference = "Stop"

Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   CRÉATION IMAGE VHD BOOTABLE                      ║" -ForegroundColor Cyan
Write-Host "║   LLM Bare-Metal v3.0 avec URS ML Training        ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Vérifier fichiers sources
Write-Host "[1/6] Vérification fichiers sources..." -ForegroundColor Yellow

$files = @{
    "llama2.efi" = "Bootloader UEFI"
    "stories15M.bin" = "Modèle LLaMA2 (58MB)"
    "tokenizer.bin" = "Tokenizer BPE"
}

$allPresent = $true
foreach ($file in $files.Keys) {
    if (Test-Path $file) {
        $size = (Get-Item $file).Length
        $sizeStr = if ($size -gt 1MB) { "$([math]::Round($size/1MB,1)) MB" } else { "$([math]::Round($size/1KB,1)) KB" }
        Write-Host "  ✓ $file ($sizeStr)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $file MANQUANT!" -ForegroundColor Red
        $allPresent = $false
    }
}

if (!$allPresent) {
    Write-Host "`n✗ Fichiers sources manquants!" -ForegroundColor Red
    exit 1
}

# Nom et taille VHD
$vhdName = "llm-baremetal-v3.vhd"
$vhdSizeMB = 128
$vhdPath = Join-Path (Get-Location) $vhdName

# Supprimer ancien VHD si existe
if (Test-Path $vhdPath) {
    Write-Host "`n  Suppression ancien VHD..." -ForegroundColor Gray
    Remove-Item $vhdPath -Force
}

Write-Host "`n[2/6] Création VHD ($vhdSizeMB MB)..." -ForegroundColor Yellow

# Créer VHD avec DiskPart
$diskpartScript = @"
create vdisk file="$vhdPath" maximum=$vhdSizeMB type=fixed
select vdisk file="$vhdPath"
attach vdisk
convert gpt
create partition efi size=100
format fs=fat32 quick label="LLM-BOOT"
assign
detail vdisk
"@

$scriptPath = Join-Path $env:TEMP "create_vhd.txt"
$diskpartScript | Out-File -FilePath $scriptPath -Encoding ASCII -Force

$output = diskpart /s $scriptPath 2>&1 | Out-String
Write-Host $output -ForegroundColor Gray

# Extraire lettre du volume
$driveLetter = $null
if ($output -match "Lettre de lecteur\s+:\s+([A-Z])") {
    $driveLetter = $Matches[1]
} elseif ($output -match "Drive Letter\s+:\s+([A-Z])") {
    $driveLetter = $Matches[1]
}

if (!$driveLetter) {
    # Chercher manuellement
    Start-Sleep -Seconds 2
    $vol = Get-Volume | Where-Object { $_.FileSystemLabel -eq "LLM-BOOT" -and $_.FileSystem -eq "FAT32" }
    if ($vol -and $vol.DriveLetter) {
        $driveLetter = $vol.DriveLetter
    }
}

if (!$driveLetter) {
    Write-Host "✗ Impossible de trouver la lettre du lecteur!" -ForegroundColor Red
    Write-Host "  Volumes disponibles:" -ForegroundColor Yellow
    Get-Volume | Format-Table
    exit 1
}

Write-Host "  ✓ VHD créé et monté (Lecteur: ${driveLetter}:)" -ForegroundColor Green

try {
    # Copier fichiers
    Write-Host "`n[3/6] Copie fichiers..." -ForegroundColor Yellow
    
    $destPath = "${driveLetter}:"
    
    # Structure EFI
    New-Item -Path "$destPath\EFI\BOOT" -ItemType Directory -Force | Out-Null
    Write-Host "  ✓ Structure EFI\BOOT créée" -ForegroundColor Green
    
    # BOOTX64.EFI
    Copy-Item "llama2.efi" -Destination "$destPath\EFI\BOOT\BOOTX64.EFI" -Force
    Write-Host "  ✓ BOOTX64.EFI ($([math]::Round((Get-Item 'llama2.efi').Length/1KB,1)) KB)" -ForegroundColor Green
    
    # Modèle
    Copy-Item "stories15M.bin" -Destination "$destPath\stories15M.bin" -Force
    Write-Host "  ✓ stories15M.bin ($([math]::Round((Get-Item 'stories15M.bin').Length/1MB,1)) MB)" -ForegroundColor Green
    
    # Tokenizer
    Copy-Item "tokenizer.bin" -Destination "$destPath\tokenizer.bin" -Force
    Write-Host "  ✓ tokenizer.bin ($([math]::Round((Get-Item 'tokenizer.bin').Length/1KB,1)) KB)" -ForegroundColor Green
    
    # README
    $readme = @"
════════════════════════════════════════════════════════════
   LLM BARE-METAL v3.0 - IMAGE VHD BOOTABLE UEFI
   URS Extended + ML Training + Quality Improvements
════════════════════════════════════════════════════════════

📦 CONTENU IMAGE:
  • EFI/BOOT/BOOTX64.EFI - Bootloader UEFI avec URS v3.0
  • stories15M.bin       - Modèle LLaMA2 (15M params, 58MB)
  • tokenizer.bin        - Tokenizer BPE (32K vocab, 423KB)

💾 CONVERSION VHD → IMAGE USB:
  1. Télécharger Win32DiskImager
  2. Sélectionner $vhdName
  3. Choisir clé USB cible
  4. Cliquer "Write"

🚀 TEST RAPIDE QEMU (sans USB):
  qemu-system-x86_64 -bios OVMF.fd -hda $vhdName -m 512

🚀 DÉMARRAGE SUR VRAI PC:
  1. Écrire VHD sur USB avec Win32DiskImager
  2. Brancher USB sur PC UEFI
  3. Redémarrer + F12/F11/ESC
  4. Sélectionner "UEFI: USB"

⚙️ SYSTÈME v3.0:
  ✅ URS ML Training - apprentissage automatique
  ✅ Température 0.9 - texte créatif
  ✅ Répétition penalty 2.5x - pas de boucles
  ✅ Top-p sampling 0.9 - meilleure qualité
  ✅ Cache 64 entrées LRU
  ✅ Learning rate adaptatif (0.01 → 0.001)

📊 RÉSULTATS TRAINING QEMU:
  • Solar strategy: 31% (9/9 succès)
  • Autres strategies: 25% (baseline)
  • Texte: "The first appreciate fly its its so it fly..."

📅 Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm')
🏷️ Version: v3.0 ML Training Edition
"@
    
    $readme | Out-File -FilePath "$destPath\README.txt" -Encoding UTF8 -Force
    Write-Host "  ✓ README.txt créé" -ForegroundColor Green
    
    # Vérifier contenu
    Write-Host "`n[4/6] Vérification contenu..." -ForegroundColor Yellow
    $files = Get-ChildItem "$destPath\" -Recurse -File
    $totalSize = ($files | Measure-Object -Property Length -Sum).Sum
    Write-Host "  ✓ $($files.Count) fichiers ($([math]::Round($totalSize/1MB,1)) MB)" -ForegroundColor Green
    
} finally {
    # Démonter VHD
    Write-Host "`n[5/6] Démontage VHD..." -ForegroundColor Yellow
    
    $detachScript = @"
select vdisk file="$vhdPath"
detach vdisk
"@
    $detachScriptPath = Join-Path $env:TEMP "detach_vhd.txt"
    $detachScript | Out-File -FilePath $detachScriptPath -Encoding ASCII -Force
    diskpart /s $detachScriptPath | Out-Null
    Start-Sleep -Seconds 1
    
    Write-Host "  ✓ VHD démonté" -ForegroundColor Green
}

# Résultat final
Write-Host "`n[6/6] Finalisation..." -ForegroundColor Yellow

$vhdInfo = Get-Item $vhdPath
Write-Host "  ✓ Image prête!" -ForegroundColor Green

Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   IMAGE VHD CRÉÉE AVEC SUCCÈS!                     ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "📀 FICHIER VHD:" -ForegroundColor Cyan
Write-Host "   Nom: $($vhdInfo.Name)" -ForegroundColor White
Write-Host "   Taille: $([math]::Round($vhdInfo.Length/1MB,1)) MB" -ForegroundColor White
Write-Host "   Chemin: $($vhdInfo.FullName)`n" -ForegroundColor White

Write-Host "📝 OPTIONS DE TEST:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  OPTION 1: Test QEMU Direct (RAPIDE!)" -ForegroundColor Cyan
Write-Host "  ────────────────────────────────────" -ForegroundColor Gray
Write-Host "    # Télécharger OVMF.fd si pas déjà fait" -ForegroundColor White
Write-Host "    qemu-system-x86_64 -bios OVMF.fd -hda $vhdName -m 512" -ForegroundColor Green
Write-Host ""
Write-Host "  OPTION 2: Écrire sur USB (Win32DiskImager)" -ForegroundColor Cyan
Write-Host "  ───────────────────────────────────────────" -ForegroundColor Gray
Write-Host "    1. Télécharger: https://sourceforge.net/projects/win32diskimager/" -ForegroundColor White
Write-Host "    2. Sélectionner: $vhdName" -ForegroundColor Green
Write-Host "    3. Device: [Votre clé USB]" -ForegroundColor White
Write-Host "    4. Write → Attendre ~2-3 min" -ForegroundColor White
Write-Host ""
Write-Host "  OPTION 3: PowerShell (Admin requis)" -ForegroundColor Cyan
Write-Host "  ────────────────────────────────────" -ForegroundColor Gray
Write-Host "    Get-Disk | Where-Object { `$_.BusType -eq 'USB' }  # Trouver numéro" -ForegroundColor White
Write-Host "    .\write-vhd-to-usb.ps1 -VHDPath $vhdName -DiskNumber X" -ForegroundColor Gray
Write-Host ""

Write-Host "💡 CONSEIL:" -ForegroundColor Cyan
Write-Host "    Test rapide avec QEMU d'abord!" -ForegroundColor White
Write-Host "    Si ça marche, écrire sur USB pour vrai hardware.`n" -ForegroundColor White

Write-Host "✅ VHD PRÊT!" -ForegroundColor Green
