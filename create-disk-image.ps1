# Création Image Disque Bootable UEFI
# Crée un fichier .img prêt à écrire sur USB avec Win32DiskImager ou dd

$ErrorActionPreference = "Stop"

Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   CRÉATION IMAGE DISQUE BOOTABLE (.img)           ║" -ForegroundColor Cyan
Write-Host "║   LLM Bare-Metal v3.0 avec URS ML Training        ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Vérifier fichiers sources
Write-Host "[1/5] Vérification fichiers sources..." -ForegroundColor Yellow

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

# Taille image: 128MB (largement suffisant)
$imageSizeMB = 128
$imageName = "llm-baremetal-v3.img"

Write-Host "`n[2/5] Création image vide ($imageSizeMB MB)..." -ForegroundColor Yellow

# Créer fichier vide de 128MB
$imagePath = Join-Path (Get-Location) $imageName
$imageBytes = New-Object byte[] (1024 * 1024) # 1MB buffer
$stream = [System.IO.File]::Create($imagePath)
try {
    for ($i = 0; $i -lt $imageSizeMB; $i++) {
        $stream.Write($imageBytes, 0, $imageBytes.Length)
        if ($i % 16 -eq 0) {
            Write-Host "  Progression: $i/$imageSizeMB MB" -NoNewline -ForegroundColor Gray
            Write-Host "`r" -NoNewline
        }
    }
    $stream.Flush()
} finally {
    $stream.Close()
}

# Vérifier création
if (!(Test-Path $imagePath)) {
    Write-Host "  ✗ Échec création image!" -ForegroundColor Red
    exit 1
}

Write-Host "  ✓ Image $imageName créée ($imageSizeMB MB)          " -ForegroundColor Green

# Monter l'image comme disque virtuel
Write-Host "`n[3/5] Montage image comme disque virtuel..." -ForegroundColor Yellow

$mountResult = Mount-DiskImage -ImagePath $imagePath -PassThru
$diskNumber = ($mountResult | Get-DiskImage | Get-Disk).Number

Write-Host "  ✓ Image montée comme Disque $diskNumber" -ForegroundColor Green

try {
    # Initialiser le disque GPT
    Write-Host "`n[4/5] Initialisation disque GPT + partition FAT32..." -ForegroundColor Yellow
    
    Initialize-Disk -Number $diskNumber -PartitionStyle GPT -ErrorAction SilentlyContinue | Out-Null
    
    # Créer partition ESP (EFI System Partition) de 100MB
    $partition = New-Partition -DiskNumber $diskNumber -Size 100MB -GptType '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}'
    
    # Formater en FAT32
    $volume = Format-Volume -Partition $partition -FileSystem FAT32 -NewFileSystemLabel "LLM-BOOT" -Force
    
    # Assigner une lettre temporaire
    $driveLetter = $null
    $attempts = 0
    while ($driveLetter -eq $null -and $attempts -lt 10) {
        Start-Sleep -Milliseconds 500
        $vol = Get-Volume | Where-Object { $_.FileSystemLabel -eq "LLM-BOOT" -and $_.DriveLetter }
        if ($vol) {
            $driveLetter = $vol.DriveLetter
        }
        $attempts++
    }
    
    if ($driveLetter -eq $null) {
        # Assigner manuellement lettre disponible
        $availableLetters = 68..90 | ForEach-Object { [char]$_ } | Where-Object {
            (Get-PSDrive -PSProvider FileSystem).Name -notcontains $_
        }
        $driveLetter = $availableLetters[0]
        Add-PartitionAccessPath -DiskNumber $diskNumber -PartitionNumber $partition.PartitionNumber -AccessPath "${driveLetter}:"
        Start-Sleep -Seconds 1
    }
    
    Write-Host "  ✓ Partition ESP créée et formatée (Lettre: ${driveLetter}:)" -ForegroundColor Green
    
    # Copier fichiers
    Write-Host "`n[5/5] Copie fichiers sur l'image..." -ForegroundColor Yellow
    
    $destPath = "${driveLetter}:"
    
    # Créer structure EFI
    New-Item -Path "$destPath\EFI\BOOT" -ItemType Directory -Force | Out-Null
    Write-Host "  ✓ Structure EFI créée" -ForegroundColor Green
    
    # Copier BOOTX64.EFI
    Copy-Item "llama2.efi" -Destination "$destPath\EFI\BOOT\BOOTX64.EFI" -Force
    Write-Host "  ✓ BOOTX64.EFI copié" -ForegroundColor Green
    
    # Copier modèle et tokenizer
    Copy-Item "stories15M.bin" -Destination "$destPath\stories15M.bin" -Force
    Write-Host "  ✓ stories15M.bin copié" -ForegroundColor Green
    
    Copy-Item "tokenizer.bin" -Destination "$destPath\tokenizer.bin" -Force
    Write-Host "  ✓ tokenizer.bin copié" -ForegroundColor Green
    
    # Créer README
    $readme = @"
════════════════════════════════════════════════════════════
   LLM BARE-METAL v3.0 - IMAGE DISQUE BOOTABLE UEFI
   URS Extended + ML Training + Quality Improvements
════════════════════════════════════════════════════════════

📦 CONTENU IMAGE:
  • EFI/BOOT/BOOTX64.EFI - Bootloader UEFI avec URS v3.0
  • stories15M.bin       - Modèle LLaMA2 (15M params, 58MB)
  • tokenizer.bin        - Tokenizer BPE (32K vocab, 423KB)

💾 ÉCRITURE SUR USB:
  Windows:
    1. Télécharger Win32DiskImager ou Rufus (mode DD)
    2. Sélectionner $imageName
    3. Choisir clé USB cible
    4. Cliquer "Write"
  
  Linux/Mac:
    sudo dd if=$imageName of=/dev/sdX bs=4M status=progress
    (Remplacer sdX par votre clé USB!)

🚀 DÉMARRAGE:
  1. Brancher clé USB sur PC UEFI
  2. Redémarrer + F12/F11/ESC (menu boot)
  3. Sélectionner "UEFI: USB" (pas Legacy!)
  4. Profiter du LLM bare-metal!

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
  • Texte généré: "The first appreciate fly its its so it fly..."

📅 Date création: $(Get-Date -Format 'yyyy-MM-dd HH:mm')
🏷️ Version: v3.0 ML Training Edition
"@
    
    $readme | Out-File -FilePath "$destPath\README.txt" -Encoding UTF8 -Force
    Write-Host "  ✓ README.txt créé" -ForegroundColor Green
    
    # Forcer flush
    Write-Host "`n  Synchronisation disque..." -ForegroundColor Gray
    [System.IO.Directory]::GetFiles($destPath, "*", [System.IO.SearchOption]::AllDirectories) | Out-Null
    Start-Sleep -Seconds 2
    
} finally {
    # Démonter l'image
    Write-Host "`n  Démontage image..." -ForegroundColor Gray
    Dismount-DiskImage -ImagePath $imagePath | Out-Null
    Start-Sleep -Seconds 1
}

# Afficher résultat final
Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   IMAGE DISQUE CRÉÉE AVEC SUCCÈS!                  ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Green

$imageInfo = Get-Item $imageName
Write-Host "📀 FICHIER IMAGE:" -ForegroundColor Cyan
Write-Host "   Nom: $($imageInfo.Name)" -ForegroundColor White
Write-Host "   Taille: $([math]::Round($imageInfo.Length/1MB,1)) MB" -ForegroundColor White
Write-Host "   Chemin: $($imageInfo.FullName)`n" -ForegroundColor White

Write-Host "📝 PROCHAINES ÉTAPES:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  MÉTHODE 1: Win32DiskImager (Recommandé)" -ForegroundColor Cyan
Write-Host "  ─────────────────────────────────────────" -ForegroundColor Gray
Write-Host "    1. Télécharger: https://sourceforge.net/projects/win32diskimager/" -ForegroundColor White
Write-Host "    2. Installer et ouvrir Win32DiskImager" -ForegroundColor White
Write-Host "    3. Sélectionner: $imageName" -ForegroundColor Green
Write-Host "    4. Choisir Device: [Votre clé USB]" -ForegroundColor White
Write-Host "    5. Cliquer 'Write' → Confirmer" -ForegroundColor White
Write-Host "    6. Attendre fin (~2-3 min)" -ForegroundColor White
Write-Host ""
Write-Host "  MÉTHODE 2: Rufus (mode DD)" -ForegroundColor Cyan
Write-Host "  ─────────────────────────────" -ForegroundColor Gray
Write-Host "    1. Ouvrir Rufus" -ForegroundColor White
Write-Host "    2. Périphérique: [Votre clé USB]" -ForegroundColor White
Write-Host "    3. Sélectionner: $imageName" -ForegroundColor Green
Write-Host "    4. Mode: 'Écrire en mode Image DD'" -ForegroundColor White
Write-Host "    5. Cliquer 'DÉMARRER'" -ForegroundColor White
Write-Host ""
Write-Host "  MÉTHODE 3: PowerShell (Avancé)" -ForegroundColor Cyan
Write-Host "  ───────────────────────────────" -ForegroundColor Gray
Write-Host "    # Identifier numéro disque USB" -ForegroundColor White
Write-Host "    Get-Disk | Where-Object { `$_.BusType -eq 'USB' }" -ForegroundColor Gray
Write-Host "    " -ForegroundColor White
Write-Host "    # Écrire image (ATTENTION: remplacer X par numéro!)" -ForegroundColor White
Write-Host "    .\write-image-to-usb.ps1 -ImagePath $imageName -DiskNumber X" -ForegroundColor Gray
Write-Host ""

Write-Host "⚠️  ATTENTION:" -ForegroundColor Red
Write-Host "    • L'écriture EFFACE TOUT sur la clé USB!" -ForegroundColor Yellow
Write-Host "    • Vérifier 2x le bon périphérique avant d'écrire!" -ForegroundColor Yellow
Write-Host ""

Write-Host "💡 CONSEIL:" -ForegroundColor Cyan
Write-Host "    Win32DiskImager est le plus simple et sûr." -ForegroundColor White
Write-Host "    Interface graphique, confirmation avant écriture.`n" -ForegroundColor White

Write-Host "✅ IMAGE PRÊTE À ÉCRIRE SUR USB!" -ForegroundColor Green
