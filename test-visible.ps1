#!/usr/bin/env pwsh
# Test QEMU simple avec fenêtre visible

Write-Host "`n🎯 Test QEMU - Mode Visible" -ForegroundColor Cyan
Write-Host "============================`n" -ForegroundColor Cyan

Write-Host "Ce test va:" -ForegroundColor Yellow
Write-Host "  1. Lancer QEMU en arrière-plan" -ForegroundColor Gray
Write-Host "  2. Attendre 20 secondes" -ForegroundColor Gray
Write-Host "  3. Vous demander si vous voyez la fenêtre`n" -ForegroundColor Gray

Write-Host "📺 Cherchez une fenêtre QEMU qui s'ouvre!" -ForegroundColor Cyan
Write-Host "   Elle peut apparaître derrière vos autres fenêtres`n" -ForegroundColor Yellow

Read-Host "Appuyez sur Entrée pour lancer"

Write-Host "`n⏳ Lancement QEMU (20 secondes)...`n" -ForegroundColor Green

# Launch QEMU in background with timeout
$job = Start-Job -ScriptBlock {
    wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && DISPLAY=:0 timeout 20 qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=test-minimal.img -m 512M 2>&1'
}

Write-Host "🟢 QEMU lancé!" -ForegroundColor Green
Write-Host "   Job ID: $($job.Id)" -ForegroundColor Gray
Write-Host "`n⏰ Attente 20 secondes...`n" -ForegroundColor Yellow

# Wait with progress
for ($i = 1; $i -le 20; $i++) {
    Write-Host "  [$i/20] " -NoNewline -ForegroundColor Gray
    if ($i % 5 -eq 0) { Write-Host "" }
    Start-Sleep -Seconds 1
}

Write-Host "`n`n✅ Temps écoulé!" -ForegroundColor Green

# Get job output
Write-Host "`n📋 Sortie QEMU:" -ForegroundColor Cyan
$output = Receive-Job -Job $job
if ($output) {
    Write-Host $output -ForegroundColor Gray
} else {
    Write-Host "  (aucune sortie dans le terminal)" -ForegroundColor DarkGray
}

Remove-Job -Job $job -Force

Write-Host "`n❓ Avez-vous vu une fenêtre QEMU s'ouvrir?" -ForegroundColor Yellow
Write-Host "   A) Oui, j'ai vu la fenêtre" -ForegroundColor Green
Write-Host "   B) Non, aucune fenêtre visible`n" -ForegroundColor Red

$response = Read-Host "Entrez A ou B"

if ($response -eq "A" -or $response -eq "a") {
    Write-Host "`n🎉 Parfait! QEMU fonctionne!" -ForegroundColor Green
    Write-Host "`n❓ Qu'avez-vous vu dans la fenêtre?" -ForegroundColor Yellow
    Write-Host "   1) Écran UEFI avec texte (TianoCore)" -ForegroundColor Cyan
    Write-Host "   2) Messages de test (✅ ou ❌)" -ForegroundColor Cyan
    Write-Host "   3) Écran noir complet" -ForegroundColor Cyan
    Write-Host "   4) Autre chose`n" -ForegroundColor Cyan
    
    $seen = Read-Host "Entrez 1, 2, 3 ou 4"
    
    Write-Host ""
    if ($seen -eq "1") {
        Write-Host "✅ UEFI boot fonctionne!" -ForegroundColor Green
        Write-Host "   Mais l'EFI n'a peut-être pas démarré" -ForegroundColor Yellow
    } elseif ($seen -eq "2") {
        Write-Host "🎊 EXCELLENT! Le test fonctionne!" -ForegroundColor Green
        Write-Host "   Relancez test-complet.ps1 et lisez la sortie!" -ForegroundColor Cyan
    } elseif ($seen -eq "3") {
        Write-Host "⚠️  Fenêtre visible mais écran noir" -ForegroundColor Yellow
        Write-Host "   Problème possible avec l'image disque" -ForegroundColor Gray
    } else {
        Write-Host "📝 Décrivez ce que vous avez vu:" -ForegroundColor Cyan
        $description = Read-Host
        Write-Host "`nMerci! Cette info aide au debug." -ForegroundColor Green
    }
} else {
    Write-Host "`n❌ Pas de fenêtre visible" -ForegroundColor Red
    Write-Host "`nPossibilités:" -ForegroundColor Yellow
    Write-Host "  1. WSLg pas configuré correctement" -ForegroundColor Gray
    Write-Host "  2. Fenêtre derrière d'autres apps" -ForegroundColor Gray
    Write-Host "  3. Problème avec X11 forwarding`n" -ForegroundColor Gray
    
    Write-Host "💡 Solution alternative:" -ForegroundColor Cyan
    Write-Host "   Utiliser VNC pour voir QEMU à distance`n" -ForegroundColor Green
}

Write-Host "`n✅ Test terminé!`n" -ForegroundColor Green
