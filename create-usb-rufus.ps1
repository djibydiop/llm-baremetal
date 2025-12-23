# Création image USB bootable pour Rufus
# llm-baremetal v2.0 - WiFi Phase 2 + DRC v5.0 + Model Streaming

Write-Host "╔══════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  CRÉATION IMAGE USB BOOTABLE - Rufus                ║" -ForegroundColor Cyan
Write-Host "║  llm-baremetal v2.0 (713 KB)                        ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$imageName = "llm-baremetal-usb.img"
$imageSize = 200  # MB

# Vérifier que llama2.efi existe
if (-not (Test-Path "llama2.efi")) {
    Write-Host "❌ llama2.efi non trouvé" -ForegroundColor Red
    Write-Host "   Exécutez d'abord: wsl make" -ForegroundColor Yellow
    exit 1
}

$efiSize = (Get-Item "llama2.efi").Length
Write-Host "✓ llama2.efi trouvé ($([math]::Round($efiSize/1KB, 2)) KB)" -ForegroundColor Green

# Vérifier les fichiers du modèle
$hasModel = Test-Path "stories15M.bin"
$hasTokenizer = Test-Path "tokenizer.bin"

if ($hasModel) {
    $modelSize = (Get-Item "stories15M.bin").Length
    Write-Host "✓ stories15M.bin trouvé ($([math]::Round($modelSize/1MB, 2)) MB)" -ForegroundColor Green
} else {
    Write-Host "⚠ stories15M.bin non trouvé (optionnel)" -ForegroundColor Yellow
}

if ($hasTokenizer) {
    Write-Host "✓ tokenizer.bin trouvé" -ForegroundColor Green
} else {
    Write-Host "⚠ tokenizer.bin non trouvé (optionnel)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "CRÉATION DE L'IMAGE..." -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan

# Créer image avec WSL
Write-Host ""
Write-Host "Étape 1: Création de l'image $imageName ($imageSize MB)..." -ForegroundColor Yellow

$wslCmd = @"
cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && \
rm -f $imageName && \
dd if=/dev/zero of=$imageName bs=1M count=$imageSize status=progress && \
mkfs.fat -F 32 $imageName && \
echo 'Image créée'
"@

wsl bash -c $wslCmd

if (-not (Test-Path $imageName)) {
    Write-Host "❌ Échec création image" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Image créée: $imageName" -ForegroundColor Green

# Monter et copier fichiers
Write-Host ""
Write-Host "Étape 2: Copie des fichiers UEFI..." -ForegroundColor Yellow

$mountCmd = @"
cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && \
mkdir -p /tmp/usb-mount && \
sudo mount -o loop $imageName /tmp/usb-mount && \
sudo mkdir -p /tmp/usb-mount/EFI/BOOT && \
sudo cp llama2.efi /tmp/usb-mount/EFI/BOOT/BOOTX64.EFI && \
$(if ($hasModel) { "sudo cp stories15M.bin /tmp/usb-mount/ &&" } else { "" }) \
$(if ($hasTokenizer) { "sudo cp tokenizer.bin /tmp/usb-mount/ &&" } else { "" }) \
sudo umount /tmp/usb-mount && \
echo 'Fichiers copiés'
"@

wsl bash -c $mountCmd

Write-Host "✓ Structure UEFI créée (EFI/BOOT/BOOTX64.EFI)" -ForegroundColor Green
if ($hasModel) { Write-Host "✓ Modèle copié (stories15M.bin)" -ForegroundColor Green }
if ($hasTokenizer) { Write-Host "✓ Tokenizer copié (tokenizer.bin)" -ForegroundColor Green }

# Informations finales
Write-Host ""
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "IMAGE PRÊTE POUR RUFUS" -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host ""

$imgPath = (Get-Item $imageName).FullName
$imgSize = [math]::Round((Get-Item $imageName).Length / 1MB, 2)

Write-Host "Fichier: $imgPath" -ForegroundColor White
Write-Host "Taille: $imgSize MB" -ForegroundColor White
Write-Host ""

Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "INSTRUCTIONS RUFUS:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host ""
Write-Host "1. Insérer une clé USB (min 256 MB)" -ForegroundColor Yellow
Write-Host "   ⚠ TOUTES LES DONNÉES SERONT EFFACÉES!" -ForegroundColor Red
Write-Host ""
Write-Host "2. Lancer Rufus (https://rufus.ie si pas installé)" -ForegroundColor Yellow
Write-Host ""
Write-Host "3. Configurer Rufus:" -ForegroundColor Yellow
Write-Host "   • Périphérique: [Votre clé USB]" -ForegroundColor White
Write-Host "   • Type de démarrage: 'Image disque ou ISO'" -ForegroundColor White
Write-Host "   • Cliquer 'SÉLECTION' et choisir:" -ForegroundColor White
Write-Host "     $imgPath" -ForegroundColor Cyan
Write-Host "   • Schéma de partition: GPT" -ForegroundColor White
Write-Host "   • Système de destination: UEFI (non CSM)" -ForegroundColor White
Write-Host "   • Nom de volume: LLM-BAREMETAL" -ForegroundColor White
Write-Host ""
Write-Host "4. Cliquer 'DÉMARRER'" -ForegroundColor Yellow
Write-Host "   • Laisser les options par défaut" -ForegroundColor White
Write-Host "   • Confirmer la destruction des données" -ForegroundColor White
Write-Host "   • Attendre la fin (~30 secondes)" -ForegroundColor White
Write-Host ""

Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "BOOT SUR MATÉRIEL RÉEL:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host ""
Write-Host "1. Redémarrer le PC avec la clé USB branchée" -ForegroundColor Yellow
Write-Host ""
Write-Host "2. Entrer dans le BIOS/UEFI (F2 / DEL / F12)" -ForegroundColor Yellow
Write-Host "   • Désactiver Secure Boot" -ForegroundColor White
Write-Host "   • S'assurer que Fast Boot est OFF" -ForegroundColor White
Write-Host "   • Mode de boot: UEFI (pas Legacy)" -ForegroundColor White
Write-Host ""
Write-Host "3. Sélectionner boot depuis USB (F12 menu boot)" -ForegroundColor Yellow
Write-Host ""
Write-Host "4. Observer la sortie:" -ForegroundColor Yellow
Write-Host "   ✓ DRC Consensus: Tentative connexion validators" -ForegroundColor Green
Write-Host "   ✓ WiFi Phase 2: Détection Intel AX200" -ForegroundColor Green
Write-Host "   ✓ Firmware upload: iwlwifi-cc-a0-77.ucode" -ForegroundColor Green
Write-Host "   ✓ RF Control: Radio ON" -ForegroundColor Green
Write-Host "   ✓ Scan 802.11: Beacons détectés" -ForegroundColor Green
Write-Host "   ✓ Model Streaming: Cache ready" -ForegroundColor Green
Write-Host "   ✓ DRC v5.0: Attention mechanism actif" -ForegroundColor Green
Write-Host ""

Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "MATÉRIEL REQUIS:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host ""
Write-Host "ESSENTIEL:" -ForegroundColor Yellow
Write-Host "  • Intel AX200 WiFi 6 (ou AX201/AX210)" -ForegroundColor White
Write-Host "  • 512 MB RAM minimum" -ForegroundColor White
Write-Host "  • CPU x86_64 avec UEFI" -ForegroundColor White
Write-Host ""
Write-Host "OPTIONNEL (pour tests complets):" -ForegroundColor Yellow
Write-Host "  • Connexion Ethernet (pour DRC Consensus)" -ForegroundColor White
Write-Host "  • Serveur avec validators en cours (.\start-validators.ps1)" -ForegroundColor White
Write-Host ""

Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "DÉPANNAGE:" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host ""
Write-Host "Problème: Écran noir au boot" -ForegroundColor Yellow
Write-Host "  → Vérifier que Secure Boot est OFF dans le BIOS" -ForegroundColor White
Write-Host "  → Essayer un autre port USB" -ForegroundColor White
Write-Host ""
Write-Host "Problème: 'WiFi device not found'" -ForegroundColor Yellow
Write-Host "  → Vérifier que l'Intel AX200 est bien installée" -ForegroundColor White
Write-Host "  → Vérifier dans BIOS que WiFi est activé" -ForegroundColor White
Write-Host "  → Essayer lspci dans un Linux live pour confirmer PCI ID" -ForegroundColor White
Write-Host ""
Write-Host "Problème: 'Consensus timeout'" -ForegroundColor Yellow
Write-Host "  → Normal si pas de connexion Ethernet" -ForegroundColor White
Write-Host "  → Normal si validators pas démarrés" -ForegroundColor White
Write-Host "  → Le programme continue en mode OFFLINE" -ForegroundColor White
Write-Host ""

Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "✅ IMAGE USB PRÊTE!" -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host ""
Write-Host "Prochaines étapes:" -ForegroundColor Cyan
Write-Host "  1. Utiliser Rufus pour flasher la clé USB" -ForegroundColor White
Write-Host "  2. (Optionnel) Démarrer les validators:" -ForegroundColor White
Write-Host "     .\start-validators.ps1" -ForegroundColor Gray
Write-Host "  3. Booter sur la clé USB" -ForegroundColor White
Write-Host "  4. Observer les tests WiFi Phase 2" -ForegroundColor White
Write-Host ""
Write-Host "Image: $imgPath" -ForegroundColor Cyan
Write-Host ""
