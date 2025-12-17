#!/usr/bin/env pwsh
# Script de téléchargement du firmware Intel AX200
# Utilise plusieurs méthodes en cascade

Set-Location $PSScriptRoot

Write-Host "=== TÉLÉCHARGEMENT FIRMWARE INTEL AX200 ===" -ForegroundColor Cyan
Write-Host ""

$firmwareFile = "iwlwifi-cc-a0-72.ucode"

# Supprimer ancien fichier
if (Test-Path $firmwareFile) {
    Write-Host "Suppression ancien fichier..." -ForegroundColor Yellow
    Remove-Item $firmwareFile -Force
}

# MÉTHODE 1: WSL curl (le plus fiable)
Write-Host "Méthode 1: WSL curl..." -ForegroundColor Yellow
try {
    wsl bash -c "curl -L -o $firmwareFile 'https://github.com/CirrusLogic/linux-firmware/raw/main/iwlwifi-cc-a0-72.ucode'"
    Start-Sleep -Seconds 2
} catch {
    Write-Host "  Erreur: $_" -ForegroundColor Red
}

# Vérifier
if (Test-Path $firmwareFile) {
    $file = Get-Item $firmwareFile
    if ($file.Length -gt 400000) {
        Write-Host "`n✓✓✓ SUCCÈS!" -ForegroundColor Green
        Write-Host "  Fichier: $($file.Name)" -ForegroundColor Cyan
        Write-Host "  Taille: $([math]::Round($file.Length/1KB, 2)) KB" -ForegroundColor Cyan
        Write-Host "  Chemin: $($file.FullName)" -ForegroundColor Gray
        Write-Host "`nFirmware prêt pour utilisation!" -ForegroundColor Green
        exit 0
    }
}

# MÉTHODE 2: PowerShell Invoke-WebRequest
Write-Host "`nMéthode 2: PowerShell..." -ForegroundColor Yellow
try {
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri "https://github.com/CirrusLogic/linux-firmware/raw/main/iwlwifi-cc-a0-72.ucode" -OutFile $firmwareFile -UseBasicParsing
    Start-Sleep -Seconds 2
} catch {
    Write-Host "  Erreur: $_" -ForegroundColor Red
}

# Vérifier
if (Test-Path $firmwareFile) {
    $file = Get-Item $firmwareFile
    if ($file.Length -gt 400000) {
        Write-Host "`n✓✓✓ SUCCÈS!" -ForegroundColor Green
        Write-Host "  Fichier: $($file.Name)" -ForegroundColor Cyan
        Write-Host "  Taille: $([math]::Round($file.Length/1KB, 2)) KB" -ForegroundColor Cyan
        exit 0
    }
}

# MÉTHODE 3: WSL wget
Write-Host "`nMéthode 3: WSL wget..." -ForegroundColor Yellow
try {
    wsl wget -O $firmwareFile "https://github.com/CirrusLogic/linux-firmware/raw/main/iwlwifi-cc-a0-72.ucode"
    Start-Sleep -Seconds 2
} catch {
    Write-Host "  Erreur: $_" -ForegroundColor Red
}

# Vérification finale
if (Test-Path $firmwareFile) {
    $file = Get-Item $firmwareFile
    if ($file.Length -gt 400000) {
        Write-Host "`n✓✓✓ SUCCÈS!" -ForegroundColor Green
        Write-Host "  Taille: $([math]::Round($file.Length/1KB, 2)) KB" -ForegroundColor Cyan
        exit 0
    } else {
        Write-Host "`n⚠ Fichier téléchargé mais trop petit: $($file.Length) bytes" -ForegroundColor Yellow
    }
}

# ÉCHEC - Instructions manuelles
Write-Host "`n========================================" -ForegroundColor Red
Write-Host "✗ TÉLÉCHARGEMENT AUTOMATIQUE ÉCHOUÉ" -ForegroundColor Red
Write-Host "========================================" -ForegroundColor Red
Write-Host ""
Write-Host "TÉLÉCHARGEMENT MANUEL REQUIS:" -ForegroundColor Yellow
Write-Host ""
Write-Host "1. Ouvrez votre navigateur" -ForegroundColor White
Write-Host "2. Allez sur:" -ForegroundColor White
Write-Host "   https://github.com/CirrusLogic/linux-firmware/blob/main/iwlwifi-cc-a0-72.ucode" -ForegroundColor Cyan
Write-Host "3. Cliquez sur 'Download raw file'" -ForegroundColor White
Write-Host "4. Sauvegardez dans:" -ForegroundColor White
Write-Host "   $PSScriptRoot" -ForegroundColor Cyan
Write-Host ""
Write-Host "Le fichier doit faire ~464 KB" -ForegroundColor Yellow
Write-Host ""

exit 1
