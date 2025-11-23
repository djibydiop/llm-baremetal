# USB Boot Creator for LLM Bare-Metal
# Crée une clé USB bootable avec le système IA

Write-Host "`n╔═══════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  LLM BARE-METAL - USB BOOT CREATOR          ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Vérifier les prérequis
Write-Host "=== Vérification des fichiers ===" -ForegroundColor Yellow

if (-not (Test-Path "llama2-disk.img")) {
    Write-Host "[ERREUR] llama2-disk.img non trouvé!" -ForegroundColor Red
    Write-Host "Exécutez d'abord: wsl make llama2-disk" -ForegroundColor Yellow
    exit 1
}

$imgSize = (Get-Item "llama2-disk.img").Length / 1MB
Write-Host "[OK] llama2-disk.img trouvé ($([math]::Round($imgSize, 2)) MB)" -ForegroundColor Green

# Afficher les informations
Write-Host "`n=== Méthodes de création USB ===" -ForegroundColor Yellow
Write-Host "1. WSL + dd (Recommandé - Plus rapide)" -ForegroundColor White
Write-Host "2. Rufus (Windows natif)" -ForegroundColor White
Write-Host "3. Annuler" -ForegroundColor White

$choice = Read-Host "`nChoisissez une méthode (1-3)"

switch ($choice) {
    "1" {
        Write-Host "`n=== Méthode WSL + dd ===" -ForegroundColor Cyan
        
        # Lister les périphériques
        Write-Host "`nPériphériques disponibles:" -ForegroundColor Yellow
        wsl lsblk -o NAME,SIZE,TYPE,MOUNTPOINT | Select-String "disk|part"
        
        Write-Host "`nATTENTION: Toutes les données du périphérique seront EFFACÉES!" -ForegroundColor Red
        Write-Host "Vérifiez BIEN le nom du périphérique!" -ForegroundColor Red
        
        $device = Read-Host "`nEntrez le périphérique USB (ex: sdb, sdc)"
        
        if ($device -match "^sd[a-z]$") {
            $confirm = Read-Host "`nConfirmez l'écriture sur /dev/$device (oui/non)"
            
            if ($confirm -eq "oui") {
                Write-Host "`nCopie en cours... Cela peut prendre 5-10 minutes" -ForegroundColor Yellow
                Write-Host "Ne débranchez PAS l'USB pendant la copie!`n" -ForegroundColor Red
                
                # Copier l'image
                wsl sudo dd if=llama2-disk.img of=/dev/$device bs=4M status=progress
                
                if ($LASTEXITCODE -eq 0) {
                    Write-Host "`nSynchronisation..." -ForegroundColor Yellow
                    wsl sudo sync
                    
                    Write-Host "`n[SUCCÈS] USB créée avec succès!" -ForegroundColor Green
                    Write-Host "Vous pouvez maintenant éjecter l'USB en toute sécurité." -ForegroundColor Green
                    
                    # Éjecter
                    $eject = Read-Host "`nÉjecter automatiquement l'USB? (oui/non)"
                    if ($eject -eq "oui") {
                        wsl sudo eject /dev/$device
                        Write-Host "USB éjectée." -ForegroundColor Green
                    }
                } else {
                    Write-Host "`n[ERREUR] La copie a échoué!" -ForegroundColor Red
                    Write-Host "Vérifiez que vous avez les permissions sudo dans WSL" -ForegroundColor Yellow
                }
            } else {
                Write-Host "`nOpération annulée." -ForegroundColor Yellow
            }
        } else {
            Write-Host "`n[ERREUR] Périphérique invalide: $device" -ForegroundColor Red
            Write-Host "Format attendu: sdb, sdc, sdd, etc." -ForegroundColor Yellow
        }
    }
    
    "2" {
        Write-Host "`n=== Méthode Rufus ===" -ForegroundColor Cyan
        Write-Host "`nÉtapes à suivre:" -ForegroundColor Yellow
        Write-Host "1. Téléchargez Rufus: https://rufus.ie/" -ForegroundColor White
        Write-Host "2. Lancez Rufus en Administrateur" -ForegroundColor White
        Write-Host "3. Configuration:" -ForegroundColor White
        Write-Host "   - Périphérique: [Votre USB]" -ForegroundColor Gray
        Write-Host "   - Type de démarrage: Image disque ou ISO" -ForegroundColor Gray
        Write-Host "   - Cliquez SÉLECTION et choisissez:" -ForegroundColor Gray
        Write-Host "     $PWD\llama2-disk.img" -ForegroundColor Cyan
        Write-Host "   - Schéma de partition: GPT" -ForegroundColor Gray
        Write-Host "   - Système de destination: UEFI (non CSM)" -ForegroundColor Gray
        Write-Host "4. Cliquez DÉMARRER" -ForegroundColor White
        Write-Host "5. Attendez la fin (5-10 minutes)" -ForegroundColor White
        
        Write-Host "`nAppuyez sur Entrée pour ouvrir Rufus..." -ForegroundColor Yellow
        Read-Host
        
        # Essayer d'ouvrir Rufus si installé
        $rufusPath = "C:\Program Files\Rufus\rufus.exe"
        if (Test-Path $rufusPath) {
            Start-Process $rufusPath -Verb RunAs
        } else {
            Start-Process "https://rufus.ie/"
            Write-Host "Page de téléchargement Rufus ouverte dans le navigateur" -ForegroundColor Green
        }
    }
    
    "3" {
        Write-Host "`nOpération annulée." -ForegroundColor Yellow
        exit 0
    }
    
    default {
        Write-Host "`n[ERREUR] Choix invalide: $choice" -ForegroundColor Red
        exit 1
    }
}

Write-Host "`n=== Prochaines étapes ===" -ForegroundColor Cyan
Write-Host "1. Insérez l'USB dans le PC cible" -ForegroundColor White
Write-Host "2. Redémarrez et appuyez sur F12 (Boot Menu)" -ForegroundColor White
Write-Host "3. Sélectionnez 'UEFI: [Votre USB]'" -ForegroundColor White
Write-Host "4. Le bootloader IA démarrera automatiquement!" -ForegroundColor White
Write-Host "`nConsultez USB_BOOT_GUIDE.md pour plus de détails.`n" -ForegroundColor Yellow
