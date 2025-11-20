#!/usr/bin/env pwsh
# Test complet: minimal puis LLaMA2

Write-Host "`n╔════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  LLaMA2 Bare-Metal - Tests Complets   ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Test 1: Minimal (vérifie file system)
Write-Host "📋 Test 1/2: Minimal EFI (file system check)" -ForegroundColor Yellow
Write-Host "─────────────────────────────────────────────`n" -ForegroundColor DarkGray

Write-Host "Ce test vérifie:" -ForegroundColor White
Write-Host "  ✓ Boot UEFI fonctionne" -ForegroundColor Green
Write-Host "  ✓ File system accessible" -ForegroundColor Green
Write-Host "  ✓ stories15M.bin présent et lisible`n" -ForegroundColor Green

Write-Host "📺 Regardez la fenêtre QEMU qui va s'ouvrir!" -ForegroundColor Cyan
Write-Host "⏱️  Durée: 15 secondes`n" -ForegroundColor Gray

Read-Host "Appuyez sur Entrée pour lancer Test 1"

wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && timeout 15 qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=test-minimal.img -m 512M 2>&1' | Out-Null

Write-Host "`n✅ Test 1 terminé!`n" -ForegroundColor Green

# Demander résultat Test 1
Write-Host "❓ Résultat du Test 1:" -ForegroundColor Yellow
Write-Host "   A) Tout vert (✅) - File system OK" -ForegroundColor Green
Write-Host "   B) Erreur rouge (❌) - Problème détecté" -ForegroundColor Red
Write-Host "   C) Écran noir - Pas de sortie`n" -ForegroundColor DarkGray

$result1 = Read-Host "Entrez A, B ou C"

if ($result1 -eq "A" -or $result1 -eq "a") {
    Write-Host "`n🎉 Excellent! File system fonctionne!" -ForegroundColor Green
    Write-Host "   → On passe au test complet LLaMA2`n" -ForegroundColor Cyan
} elseif ($result1 -eq "B" -or $result1 -eq "b") {
    Write-Host "`n⚠️  Problème détecté dans Test 1" -ForegroundColor Yellow
    Write-Host "   → Test 2 risque d'échouer aussi" -ForegroundColor Yellow
    Write-Host "   → Mais essayons quand même!`n" -ForegroundColor Cyan
} else {
    Write-Host "`n❌ Écran noir = problème de boot UEFI" -ForegroundColor Red
    Write-Host "   → Vérifiez OVMF.fd path" -ForegroundColor Yellow
    Write-Host "   → Test 2 va probablement échouer`n" -ForegroundColor Yellow
}

Write-Host "───────────────────────────────────────────────`n" -ForegroundColor DarkGray

# Test 2: LLaMA2 complet
Write-Host "🚀 Test 2/2: LLaMA2 Full (15M params)" -ForegroundColor Yellow
Write-Host "─────────────────────────────────────────────`n" -ForegroundColor DarkGray

Write-Host "Ce test va:" -ForegroundColor White
Write-Host "  1. Charger le modèle stories15M.bin (60 MB)" -ForegroundColor Gray
Write-Host "  2. Initialiser le transformer (15M params)" -ForegroundColor Gray
Write-Host "  3. Exécuter forward pass" -ForegroundColor Gray
Write-Host "  4. Générer 20 tokens`n" -ForegroundColor Gray

Write-Host "⚡ Attendez-vous à:" -ForegroundColor Cyan
Write-Host "   • Chargement modèle: ~3 secondes" -ForegroundColor Gray
Write-Host "   • Forward pass: ~10 secondes" -ForegroundColor Gray
Write-Host "   • Génération: ~2 secondes`n" -ForegroundColor Gray

Write-Host "📺 IMPORTANT: Regardez la fenêtre QEMU!" -ForegroundColor Cyan
Write-Host "⏱️  Durée: 30 secondes (ou jusqu'à complétion)`n" -ForegroundColor Gray

Read-Host "Appuyez sur Entrée pour lancer Test 2"

wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && timeout 30 qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=llama2-disk.img -m 512M 2>&1' | Out-Null

Write-Host "`n✅ Test 2 terminé!`n" -ForegroundColor Green

# Demander résultat Test 2
Write-Host "❓ Résultat du Test 2:" -ForegroundColor Yellow
Write-Host "   A) [SUCCESS] Generation complete! 🎉" -ForegroundColor Green
Write-Host "   B) [ERROR] quelque chose... ❌" -ForegroundColor Red
Write-Host "   C) S'est arrêté à un certain [DEBUG]..." -ForegroundColor Yellow
Write-Host "   D) Écran noir ou pas de sortie`n" -ForegroundColor DarkGray

$result2 = Read-Host "Entrez A, B, C ou D"

Write-Host ""
Write-Host "╔════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║           RÉSUMÉ DES TESTS             ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "Test 1 (Minimal): " -NoNewline -ForegroundColor White
if ($result1 -eq "A" -or $result1 -eq "a") {
    Write-Host "✅ SUCCÈS" -ForegroundColor Green
} else {
    Write-Host "❌ ÉCHEC" -ForegroundColor Red
}

Write-Host "Test 2 (LLaMA2): " -NoNewline -ForegroundColor White
if ($result2 -eq "A" -or $result2 -eq "a") {
    Write-Host "🎉 SUCCÈS COMPLET!" -ForegroundColor Green
    Write-Host "`n🎊 FÉLICITATIONS! 🎊" -ForegroundColor Cyan
    Write-Host "───────────────────" -ForegroundColor Cyan
    Write-Host "Le modèle LLaMA2 15M fonctionne en bare-metal!" -ForegroundColor Green
    Write-Host "Prochaine étape: Implémenter le tokenizer BPE complet`n" -ForegroundColor Yellow
} elseif ($result2 -eq "B" -or $result2 -eq "b") {
    Write-Host "❌ ERREUR" -ForegroundColor Red
    Write-Host "`nQuelle était l'erreur exacte?" -ForegroundColor Yellow
    $error = Read-Host "Tapez le message [ERROR]"
    Write-Host "`n💡 Consultez QEMU_INTERPRETATION_GUIDE.md pour l'analyse`n" -ForegroundColor Cyan
} elseif ($result2 -eq "C" -or $result2 -eq "c") {
    Write-Host "⚠️  BLOCAGE PARTIEL" -ForegroundColor Yellow
    Write-Host "`nÀ quel [DEBUG] s'est-il arrêté?" -ForegroundColor Yellow
    $debug = Read-Host "Ex: 'Loading model', 'Forward pass', etc."
    Write-Host "`n💡 Cela indique où ajouter plus de diagnostics`n" -ForegroundColor Cyan
} else {
    Write-Host "❌ PAS DE SORTIE" -ForegroundColor Red
    Write-Host "`n💡 Problème probable: boot UEFI ou mémoire`n" -ForegroundColor Yellow
}

Write-Host "───────────────────────────────────────────────`n" -ForegroundColor DarkGray
Write-Host "📚 Documentation:" -ForegroundColor Cyan
Write-Host "   • QEMU_INTERPRETATION_GUIDE.md - Guide d'interprétation" -ForegroundColor Gray
Write-Host "   • README_LLAMA2.md - Documentation complète" -ForegroundColor Gray
Write-Host "   • LLAMA2_PORT_COMPLETE.md - Rapport technique`n" -ForegroundColor Gray
