# Test DRC v3.0 Multi-Expert
# Ce script lance QEMU et surveille les capacités multi-domaines

$ErrorActionPreference = "SilentlyContinue"

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        DRC v3.0 MULTI-EXPERT - Test & Monitoring             ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-Host "🎯 Capacités testées:" -ForegroundColor Yellow
Write-Host "  📚 Shakespeare Mode - Génération littéraire" -ForegroundColor White
Write-Host "  🔢 Mathematics Mode - Raisonnement logique" -ForegroundColor White
Write-Host "  🧠 Self-Awareness - Conscience de mission" -ForegroundColor White
Write-Host "  🌐 Network Learning - Apprentissage distribué" -ForegroundColor White
Write-Host "  🎯 Adaptive Strategy - 3 modes dynamiques`n" -ForegroundColor White

# Arrêter QEMU existant
Write-Host "⏹️  Arrêt des instances QEMU..." -ForegroundColor Yellow
Get-Process | Where-Object {$_.ProcessName -like "*qemu*"} | Stop-Process -Force 2>$null
Start-Sleep -Seconds 2

# Lancer QEMU en arrière-plan
Write-Host "🚀 Lancement de QEMU avec DRC v3.0...`n" -ForegroundColor Green

$qemuPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$biosPath = "C:\Users\djibi\Desktop\baremetal\llm-baremetal\OVMF.fd"
$imagePath = "C:\Users\djibi\Desktop\baremetal\llm-baremetal\llama2_efi.img"
$serialLog = "qemu-serial-output.txt"

# Supprimer ancien log
if (Test-Path $serialLog) {
    Remove-Item $serialLog -Force
}

# Lancer QEMU
$arguments = @(
    "-bios", $biosPath,
    "-drive", "file=$imagePath,format=raw",
    "-m", "2048M",
    "-cpu", "qemu64,+sse2",
    "-smp", "2",
    "-serial", "file:$serialLog"
)

Start-Process -FilePath $qemuPath -ArgumentList $arguments -WindowStyle Normal
Write-Host "✓ QEMU lancé avec logging série`n" -ForegroundColor Green

# Phase 1: Attendre le chargement du modèle ET la génération complète (20 minutes)
Write-Host "📥 Phase 1: Chargement du modèle et génération (20 min max)..." -ForegroundColor Cyan
Write-Host "⏰ Temps d'attente augmenté pour laisser DRC terminer complètement`n" -ForegroundColor Yellow

for ($i = 1; $i -le 120; $i++) {
    Start-Sleep -Seconds 10
    $size = (Get-Item $serialLog -ErrorAction SilentlyContinue).Length
    if ($size -eq $null) { $size = 0 }
    $pct = [math]::Min([math]::Round(($size / 425000) * 100, 1), 100)
    $minutes = [math]::Floor($i / 6)
    $seconds = ($i % 6) * 10
    
    $bar = "=" * [math]::Floor($pct / 5)
    $space = " " * (20 - [math]::Floor($pct / 5))
    Write-Host "  [$bar$space] $pct% ($([math]::Round($size/1KB, 0)) KB) - ${minutes}m ${seconds}s" -ForegroundColor Yellow
    
    # Vérifier si DRC a terminé (rapport final présent)
    $content = Get-Content $serialLog -ErrorAction SilentlyContinue
    $completed = $content | Select-String "TRAINING REPORT.*COMPLETE"
    
    if ($completed -and $size -gt 100000) {
        Write-Host "`n✓ DRC a terminé la génération!`n" -ForegroundColor Green
        break
    }
    
    if ($i -eq 120) {
        Write-Host "`n⏰ Timeout 20 minutes atteint`n" -ForegroundColor Yellow
    }
}

# Phase 2: Surveiller DRC v3.0
Write-Host "🔍 Phase 2: Surveillance DRC v3.0...`n" -ForegroundColor Cyan

Start-Sleep -Seconds 5

$content = Get-Content $serialLog -ErrorAction SilentlyContinue

# Vérifier activation
$v3Active = $content | Select-String "v3.0 MULTI-EXPERT"
if ($v3Active) {
    Write-Host "  ✅ DRC v3.0 ACTIVÉ" -ForegroundColor Green
} else {
    Write-Host "  ⚠️  DRC v3.0 non détecté" -ForegroundColor Red
}

# Vérifier expertise
$expertMode = $content | Select-String "EXPERT.*ACTIVE"
if ($expertMode) {
    Write-Host "  ✅ Multi-Expert System ACTIF" -ForegroundColor Green
    $expertMode | ForEach-Object { Write-Host "     $_" -ForegroundColor Gray }
}

# Vérifier domaines
$shakespeare = $content | Select-String "Shakespeare"
$math = $content | Select-String "Mathematics"
$mission = $content | Select-String "Mission"

if ($shakespeare) {
    Write-Host "  📚 Shakespeare Mode: DÉTECTÉ" -ForegroundColor Cyan
}
if ($math) {
    Write-Host "  🔢 Mathematics Mode: DÉTECTÉ" -ForegroundColor Cyan
}
if ($mission) {
    Write-Host "  🧠 Mission Clarity: ACTIF" -ForegroundColor Cyan
}

# Phase 3: Afficher premiers tokens
Write-Host "`n🎲 Phase 3: Premiers tokens générés...`n" -ForegroundColor Cyan

$tokens = $content | Select-String "pos=.*out=" | Select-Object -First 15
if ($tokens) {
    foreach ($token in $tokens) {
        $line = $token.Line
        if ($line -match "\[pos=(\d+).*out=(\d+)\]") {
            $pos = $matches[1]
            $out = $matches[2]
            Write-Host "  Token $pos → $out" -ForegroundColor White
        }
    }
    Write-Host ""
} else {
    Write-Host "  ⏳ Génération en cours, pas encore de tokens..." -ForegroundColor Yellow
}

# Phase 4: Résumé final
Write-Host "📊 Résumé:

" -ForegroundColor Cyan

$drcLines = $content | Select-String "DRC" | Measure-Object
$expertLines = $content | Select-String "EXPERT" | Measure-Object
$loopLines = $content | Select-String "LOOP" | Measure-Object

Write-Host "  Lignes DRC: $($drcLines.Count)" -ForegroundColor White
Write-Host "  Lignes EXPERT: $($expertLines.Count)" -ForegroundColor White
Write-Host "  Lignes LOOP debug: $($loopLines.Count)" -ForegroundColor White

Write-Host "`n✅ Test terminé! Vérifiez la fenêtre QEMU pour voir la génération complète.`n" -ForegroundColor Green
Write-Host "💡 Pour voir les logs complets: Get-Content qemu-serial-output.txt`n" -ForegroundColor Yellow
