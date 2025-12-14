# Écriture Image Disque sur Clé USB
# ATTENTION: EFFACE TOUT sur la clé USB!

param(
    [Parameter(Mandatory=$true)]
    [string]$ImagePath,
    
    [Parameter(Mandatory=$true)]
    [int]$DiskNumber
)

$ErrorActionPreference = "Stop"

# Vérifier droits admin
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (!$isAdmin) {
    Write-Host "✗ ERREUR: Droits administrateur requis!" -ForegroundColor Red
    Write-Host "  Relancer PowerShell en tant qu'administrateur" -ForegroundColor Yellow
    exit 1
}

# Vérifier image existe
if (!(Test-Path $ImagePath)) {
    Write-Host "✗ ERREUR: Image '$ImagePath' introuvable!" -ForegroundColor Red
    exit 1
}

# Obtenir infos disque
try {
    $disk = Get-Disk -Number $DiskNumber -ErrorAction Stop
} catch {
    Write-Host "✗ ERREUR: Disque $DiskNumber introuvable!" -ForegroundColor Red
    Write-Host "`nDisques USB disponibles:" -ForegroundColor Yellow
    Get-Disk | Where-Object { $_.BusType -eq 'USB' } | Format-Table Number, FriendlyName, Size, BusType
    exit 1
}

# Vérifier que c'est bien un USB
if ($disk.BusType -ne 'USB') {
    Write-Host "✗ ERREUR: Disque $DiskNumber n'est PAS un périphérique USB!" -ForegroundColor Red
    Write-Host "  Type: $($disk.BusType)" -ForegroundColor Yellow
    Write-Host "  DANGER: Risque d'effacer disque système!" -ForegroundColor Red
    exit 1
}

# Afficher infos
Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   ÉCRITURE IMAGE SUR CLÉ USB                       ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "📀 IMAGE SOURCE:" -ForegroundColor Yellow
$imageInfo = Get-Item $ImagePath
Write-Host "   Fichier: $($imageInfo.Name)" -ForegroundColor White
Write-Host "   Taille: $([math]::Round($imageInfo.Length/1MB,1)) MB`n" -ForegroundColor White

Write-Host "💾 DISQUE CIBLE:" -ForegroundColor Yellow
Write-Host "   Numéro: $DiskNumber" -ForegroundColor White
Write-Host "   Nom: $($disk.FriendlyName)" -ForegroundColor White
Write-Host "   Taille: $([math]::Round($disk.Size/1GB,1)) GB" -ForegroundColor White
Write-Host "   Type: $($disk.BusType)`n" -ForegroundColor White

# Confirmation DANGEREUSE
Write-Host "⚠️  ATTENTION CRITIQUE:" -ForegroundColor Red
Write-Host "   TOUTES LES DONNÉES SUR LE DISQUE $DiskNumber SERONT EFFACÉES!" -ForegroundColor Red
Write-Host "   Cette action est IRRÉVERSIBLE!`n" -ForegroundColor Yellow

$confirmation = Read-Host "Taper 'OUI EFFACER' (en majuscules) pour confirmer"
if ($confirmation -ne "OUI EFFACER") {
    Write-Host "`n✗ Annulé par l'utilisateur" -ForegroundColor Yellow
    exit 0
}

Write-Host "`n[1/4] Préparation disque..." -ForegroundColor Yellow

# Démonter toutes les partitions
Get-Partition -DiskNumber $DiskNumber -ErrorAction SilentlyContinue | ForEach-Object {
    if ($_.DriveLetter) {
        Remove-PartitionAccessPath -DiskNumber $DiskNumber -PartitionNumber $_.PartitionNumber -AccessPath "$($_.DriveLetter):" -ErrorAction SilentlyContinue
    }
}

# Mettre disque offline puis online pour reset
Set-Disk -Number $DiskNumber -IsOffline $true
Start-Sleep -Seconds 1
Set-Disk -Number $DiskNumber -IsOffline $false
Start-Sleep -Seconds 1

# Clear le disque
Clear-Disk -Number $DiskNumber -RemoveData -RemoveOEM -Confirm:$false
Write-Host "  ✓ Disque nettoyé" -ForegroundColor Green

Write-Host "`n[2/4] Ouverture accès direct disque..." -ForegroundColor Yellow

# Ouvrir handle disque physique
$diskPath = "\\.\PhysicalDrive$DiskNumber"
$handle = [System.IO.File]::Open(
    $diskPath,
    [System.IO.FileMode]::Open,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::None
)

try {
    Write-Host "  ✓ Accès disque physique obtenu" -ForegroundColor Green
    
    Write-Host "`n[3/4] Écriture image..." -ForegroundColor Yellow
    
    $imageStream = [System.IO.File]::OpenRead($ImagePath)
    try {
        $buffer = New-Object byte[] (1MB)
        $totalBytes = $imageStream.Length
        $writtenBytes = 0
        
        while ($true) {
            $read = $imageStream.Read($buffer, 0, $buffer.Length)
            if ($read -eq 0) { break }
            
            $handle.Write($buffer, 0, $read)
            $writtenBytes += $read
            
            $percent = [math]::Round(($writtenBytes / $totalBytes) * 100, 1)
            Write-Host "  Progression: $percent% ($([math]::Round($writtenBytes/1MB,1))/$([math]::Round($totalBytes/1MB,1)) MB)" -NoNewline -ForegroundColor Cyan
            Write-Host "`r" -NoNewline
        }
        
        Write-Host "  ✓ Image écrite: $([math]::Round($writtenBytes/1MB,1)) MB                    " -ForegroundColor Green
        
    } finally {
        $imageStream.Close()
    }
    
    # Flush
    Write-Host "`n[4/4] Synchronisation..." -ForegroundColor Yellow
    $handle.Flush()
    
} finally {
    $handle.Close()
}

# Rafraîchir le disque
Set-Disk -Number $DiskNumber -IsOffline $true
Start-Sleep -Seconds 1
Set-Disk -Number $DiskNumber -IsOffline $false
Start-Sleep -Seconds 2

Write-Host "  ✓ Synchronisation terminée" -ForegroundColor Green

Write-Host "`n╔════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║   ÉCRITURE TERMINÉE AVEC SUCCÈS!                   ║" -ForegroundColor Green
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Green

Write-Host "✅ Clé USB prête à booter!" -ForegroundColor Green
Write-Host "`n📝 PROCHAINES ÉTAPES:" -ForegroundColor Cyan
Write-Host "  1. Éjecter la clé USB proprement" -ForegroundColor White
Write-Host "  2. Brancher sur PC cible" -ForegroundColor White
Write-Host "  3. Redémarrer + F12/F11/ESC" -ForegroundColor White
Write-Host "  4. Sélectionner 'UEFI: USB'" -ForegroundColor White
Write-Host "  5. Profiter du LLM bare-metal! 🚀`n" -ForegroundColor White
