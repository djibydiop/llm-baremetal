# Script PowerShell - Préparation Clé USB Boot
# LLM Bare-Metal - Boot USB automatique

Write-Host "=== LLM Bare-Metal - Création Clé USB Boot ===" -ForegroundColor Cyan
Write-Host ""

# Étape 1: Lister les disques
Write-Host "[1/5] Disques disponibles:" -ForegroundColor Yellow
Get-Disk | Format-Table Number, FriendlyName, Size, PartitionStyle

Write-Host ""
Write-Host "ATTENTION: La clé USB sera COMPLETEMENT EFFACEE!" -ForegroundColor Red
Write-Host ""

# Demander confirmation
$diskNumber = Read-Host "Entrez le numéro du disque USB (ex: 1)"
Write-Host ""

$confirmation = Read-Host "CONFIRMER l'effacement du disque $diskNumber ? (tapez 'OUI' en majuscules)"
if ($confirmation -ne "OUI") {
    Write-Host "Opération annulée." -ForegroundColor Red
    exit
}

Write-Host ""
Write-Host "[2/5] Nettoyage et formatage du disque $diskNumber..." -ForegroundColor Yellow

try {
    # Nettoyer le disque
    Clear-Disk -Number $diskNumber -RemoveData -Confirm:$false -ErrorAction Stop
    
    # Créer table GPT
    Initialize-Disk -Number $diskNumber -PartitionStyle GPT -ErrorAction Stop
    
    # Créer partition maximale
    $partition = New-Partition -DiskNumber $diskNumber -UseMaximumSize -AssignDriveLetter -ErrorAction Stop
    $driveLetter = $partition.DriveLetter
    
    Write-Host "Partition créée: $driveLetter`:" -ForegroundColor Green
    
    # Formater en FAT32
    Format-Volume -DriveLetter $driveLetter -FileSystem FAT32 -NewFileSystemLabel "LLM-BOOT" -Confirm:$false -ErrorAction Stop
    
    Write-Host "[3/5] Formatage terminé!" -ForegroundColor Green
    
} catch {
    Write-Host "ERREUR lors du formatage: $_" -ForegroundColor Red
    exit
}

Write-Host ""
Write-Host "[4/5] Copie des fichiers..." -ForegroundColor Yellow

try {
    # Créer structure EFI
    $efiPath = "$driveLetter`:\EFI\BOOT"
    New-Item -ItemType Directory -Path $efiPath -Force | Out-Null
    
    # Copier l'application UEFI (renommer en BOOTX64.EFI)
    if (Test-Path "llama2.efi") {
        Copy-Item "llama2.efi" "$efiPath\BOOTX64.EFI" -Force
        Write-Host "  ✓ llama2.efi → BOOTX64.EFI" -ForegroundColor Green
    } else {
        Write-Host "  ✗ llama2.efi introuvable!" -ForegroundColor Red
        exit
    }
    
    # Copier le modèle
    if (Test-Path "stories15M.bin") {
        Copy-Item "stories15M.bin" "$driveLetter`:\" -Force
        Write-Host "  ✓ stories15M.bin (58MB)" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ stories15M.bin introuvable, essai stories110M..." -ForegroundColor Yellow
        if (Test-Path "stories110M.bin") {
            Copy-Item "stories110M.bin" "$driveLetter`:\" -Force
            Write-Host "  ✓ stories110M.bin (420MB)" -ForegroundColor Green
        } else {
            Write-Host "  ✗ Aucun modèle trouvé!" -ForegroundColor Red
            exit
        }
    }
    
    # Copier le tokenizer
    if (Test-Path "tokenizer.bin") {
        Copy-Item "tokenizer.bin" "$driveLetter`:\" -Force
        Write-Host "  ✓ tokenizer.bin (424KB)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ tokenizer.bin introuvable!" -ForegroundColor Red
        exit
    }
    
} catch {
    Write-Host "ERREUR lors de la copie: $_" -ForegroundColor Red
    exit
}

Write-Host ""
Write-Host "[5/5] Vérification..." -ForegroundColor Yellow

# Lister les fichiers copiés
Write-Host ""
Write-Host "Contenu de la clé USB:" -ForegroundColor Cyan
Get-ChildItem "$driveLetter`:\" -Recurse | Select-Object FullName, Length | Format-Table

Write-Host ""
Write-Host "=== SUCCES! Clé USB prête ===" -ForegroundColor Green
Write-Host ""
Write-Host "Structure créée:" -ForegroundColor Cyan
Write-Host "  $driveLetter`:\EFI\BOOT\BOOTX64.EFI" -ForegroundColor White
Write-Host "  $driveLetter`:\stories15M.bin (ou stories110M.bin)" -ForegroundColor White
Write-Host "  $driveLetter`:\tokenizer.bin" -ForegroundColor White
Write-Host ""
Write-Host "Prochaines étapes:" -ForegroundColor Yellow
Write-Host "  1. Redémarrer le PC avec la clé USB insérée" -ForegroundColor White
Write-Host "  2. Appuyer sur F12/F9/ESC au démarrage" -ForegroundColor White
Write-Host "  3. Sélectionner 'UEFI: LLM-BOOT' dans le menu" -ForegroundColor White
Write-Host "  4. Observer la génération de texte!" -ForegroundColor White
Write-Host ""
Write-Host "NOTES IMPORTANTES:" -ForegroundColor Yellow
Write-Host "  - Secure Boot doit être DESACTIVE dans le BIOS" -ForegroundColor Red
Write-Host "  - Choisir UEFI boot (pas Legacy/CSM)" -ForegroundColor Red
Write-Host "  - Le chargement prend 5-20 secondes" -ForegroundColor White
Write-Host ""
Write-Host "Voir BOOT_USB_GUIDE.md pour plus de détails" -ForegroundColor Cyan
Write-Host ""
