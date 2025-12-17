# Script de test QEMU avec UEFI pour llm-baremetal
# Teste l'image USB dans un environnement virtuel avant le boot hardware

Write-Host "=== Test QEMU UEFI - llm-baremetal ===" -ForegroundColor Cyan
Write-Host ""

# Chemins
$qemuPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$imagePath = "llm-baremetal-usb.img"
$ovmfPath = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"

# Vérifications
if (-not (Test-Path $qemuPath)) {
    Write-Host "ERREUR: QEMU non trouvé à $qemuPath" -ForegroundColor Red
    Write-Host "Télécharger depuis: https://qemu.weilnetz.de/w64/" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $imagePath)) {
    Write-Host "ERREUR: Image $imagePath non trouvée" -ForegroundColor Red
    Write-Host "Exécutez d'abord: .\create-bootable-usb.ps1" -ForegroundColor Yellow
    exit 1
}

Write-Host "✓ QEMU trouvé: $qemuPath" -ForegroundColor Green
Write-Host "✓ Image trouvée: $imagePath ($(((Get-Item $imagePath).Length / 1MB).ToString('0.00')) MB)" -ForegroundColor Green

# Vérifier OVMF (UEFI firmware)
if (-not (Test-Path $ovmfPath)) {
    Write-Host "⚠ OVMF non trouvé, recherche alternative..." -ForegroundColor Yellow
    
    # Chercher OVMF dans les emplacements courants
    $ovmfLocations = @(
        "C:\Program Files\qemu\share\edk2-x86_64-code.fd",
        "C:\Program Files\qemu\share\OVMF_CODE.fd",
        "C:\Program Files\qemu\OVMF_CODE.fd",
        "C:\qemu\OVMF_CODE.fd"
    )
    
    $ovmfFound = $false
    foreach ($loc in $ovmfLocations) {
        if (Test-Path $loc) {
            $ovmfPath = $loc
            $ovmfFound = $true
            Write-Host "✓ OVMF trouvé: $ovmfPath" -ForegroundColor Green
            break
        }
    }
    
    if (-not $ovmfFound) {
        Write-Host "AVERTISSEMENT: OVMF (UEFI firmware) non trouvé" -ForegroundColor Yellow
        Write-Host "QEMU va démarrer en mode BIOS legacy (pas idéal mais fonctionnel)" -ForegroundColor Yellow
        $ovmfPath = $null
    }
} else {
    Write-Host "✓ OVMF trouvé: $ovmfPath" -ForegroundColor Green
}

Write-Host ""
Write-Host "Configuration QEMU:" -ForegroundColor Cyan
Write-Host "  - CPU: 1 core x86_64" -ForegroundColor Gray
Write-Host "  - RAM: 512 MB" -ForegroundColor Gray
Write-Host "  - Disque: $imagePath (format raw)" -ForegroundColor Gray
Write-Host "  - Boot: UEFI $(if($ovmfPath){'avec OVMF'}else{'legacy BIOS'})" -ForegroundColor Gray
Write-Host "  - Affichage: VGA standard" -ForegroundColor Gray
Write-Host "  - Réseau: user mode (pas de WiFi réel)" -ForegroundColor Gray
Write-Host ""

Write-Host "NOTE IMPORTANTE:" -ForegroundColor Yellow
Write-Host "  Le WiFi ne fonctionnera PAS dans QEMU car:" -ForegroundColor Yellow
Write-Host "  - QEMU n'émule pas de carte Intel AX200" -ForegroundColor Yellow
Write-Host "  - Ce test vérifie uniquement le BOOT UEFI" -ForegroundColor Yellow
Write-Host "  - Pour tester le WiFi: boot sur hardware réel" -ForegroundColor Yellow
Write-Host ""

Write-Host "Attendu dans QEMU:" -ForegroundColor Cyan
Write-Host "  1. Logo UEFI ou boot sequence" -ForegroundColor Gray
Write-Host "  2. 'WiFi device not found' (normal, pas d'Intel AX200 dans QEMU)" -ForegroundColor Gray
Write-Host "  3. Le programme doit s'arrêter proprement" -ForegroundColor Gray
Write-Host ""

Write-Host "Lancement de QEMU..." -ForegroundColor Green
Write-Host "Appuyez sur Ctrl+C pour arrêter QEMU" -ForegroundColor Yellow
Write-Host ""
Start-Sleep -Seconds 2

# Construction de la commande QEMU
$qemuArgs = @(
    "-m", "512",                          # 512 MB RAM
    "-cpu", "qemu64",                     # CPU x86_64 générique
    "-smp", "1",                          # 1 core
    "-drive", "file=$imagePath,format=raw,if=ide",  # Disque IDE (plus compatible)
    "-vga", "std",                        # VGA standard
    "-boot", "order=c",                   # Boot sur premier disque
    "-serial", "stdio",                   # Serial console sur stdout
    "-display", "sdl"                     # Affichage SDL
)

# Ajouter OVMF si disponible (utiliser -drive au lieu de -bios pour UEFI)
if ($ovmfPath) {
    $qemuArgs = @("-drive", "if=pflash,format=raw,readonly=on,file=$ovmfPath") + $qemuArgs
}

# Lancer QEMU
Write-Host "Commande: & '$qemuPath' $($qemuArgs -join ' ')" -ForegroundColor DarkGray
Write-Host ""
Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║           FENÊTRE QEMU VA S'OUVRIR                ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

try {
    & $qemuPath @qemuArgs
} catch {
    Write-Host ""
    Write-Host "ERREUR lors du lancement de QEMU:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== Test QEMU terminé ===" -ForegroundColor Green
Write-Host ""
Write-Host "Si vous avez vu 'WiFi device not found', c'est NORMAL!" -ForegroundColor Yellow
Write-Host "QEMU n'émule pas l'Intel AX200." -ForegroundColor Yellow
Write-Host ""
Write-Host "Prochaine étape: Tester sur HARDWARE RÉEL avec Intel AX200" -ForegroundColor Cyan
Write-Host "  1. Flasher l'USB avec balenaEtcher" -ForegroundColor Gray
Write-Host "  2. Désactiver Secure Boot dans BIOS" -ForegroundColor Gray
Write-Host "  3. Booter sur USB (F12)" -ForegroundColor Gray
Write-Host ""
