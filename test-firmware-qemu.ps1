#!/usr/bin/env pwsh
# Test rapide du firmware dans QEMU

Write-Host "`n╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        TEST FIRMWARE INTEL AX200 DANS QEMU              ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Vérifier que le firmware existe
if (-not (Test-Path iwlwifi-cc-a0-72.ucode)) {
    Write-Host "✗ Firmware manquant!" -ForegroundColor Red
    exit 1
}

$fw = Get-Item iwlwifi-cc-a0-72.ucode
Write-Host "✓ Firmware trouvé: $($fw.Name) ($([math]::Round($fw.Length/1MB, 2)) MB)`n" -ForegroundColor Green

# Créer une image de test simple avec le firmware
Write-Host "Création d'une image de test..." -ForegroundColor Yellow

wsl bash -c @"
# Créer image de 64 MB
dd if=/dev/zero of=test-fw.img bs=1M count=64 2>&1 | grep -v records

# GPT + partition EFI
sgdisk -Z test-fw.img >/dev/null 2>&1
sgdisk -n 1:2048:0 -t 1:EF00 test-fw.img >/dev/null 2>&1

# Installer mtools si nécessaire
if ! which mformat >/dev/null 2>&1; then
    sudo apt-get install -y mtools >/dev/null 2>&1
fi

# Formater et copier avec mtools (pas besoin de sudo)
OFFSET=\$((2048 * 512))
mformat -i test-fw.img@@\$OFFSET -F -v TESTFW ::
mmd -i test-fw.img@@\$OFFSET ::/EFI
mmd -i test-fw.img@@\$OFFSET ::/EFI/BOOT
mcopy -i test-fw.img@@\$OFFSET llama2.efi ::/EFI/BOOT/BOOTX64.EFI
mcopy -i test-fw.img@@\$OFFSET iwlwifi-cc-a0-72.ucode ::/

echo ""
echo "=== Contenu de l'image ==="
mdir -i test-fw.img@@\$OFFSET ::/
mdir -i test-fw.img@@\$OFFSET ::/EFI/BOOT/
"@

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n✓ Image de test créée!" -ForegroundColor Green
    
    Write-Host "`nLancement de QEMU (appuyer sur Ctrl+C pour quitter)..." -ForegroundColor Yellow
    Write-Host "Cherchez ces lignes dans la sortie:" -ForegroundColor Cyan
    Write-Host "  - 'WiFi device detected'" -ForegroundColor Gray
    Write-Host "  - 'Loading firmware: iwlwifi-cc-a0-72.ucode'" -ForegroundColor Gray
    Write-Host "  - 'Firmware loaded successfully'" -ForegroundColor Gray
    Write-Host ""
    
    # Lancer QEMU avec sortie série
    if (Test-Path "C:\Program Files\qemu\qemu-system-x86_64.exe") {
        & "C:\Program Files\qemu\qemu-system-x86_64.exe" `
            -bios OVMF.fd `
            -drive format=raw,file=test-fw.img `
            -m 512M `
            -serial file:qemu-fw-test.log `
            -display none `
            -no-reboot
        
        Write-Host "`n=== Log QEMU ===" -ForegroundColor Cyan
        if (Test-Path qemu-fw-test.log) {
            Get-Content qemu-fw-test.log | Select-String "WIFI|firmware|Firmware|FIRMWARE" | ForEach-Object { $_.Line }
        }
    } else {
        Write-Host "QEMU non trouvé. Installez depuis https://www.qemu.org/download/" -ForegroundColor Yellow
        Write-Host "Ou testez sur vrai hardware avec Rufus + USB" -ForegroundColor Yellow
    }
} else {
    Write-Host "✗ Erreur lors de la création de l'image" -ForegroundColor Red
}
