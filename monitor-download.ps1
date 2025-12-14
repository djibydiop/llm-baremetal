#!/usr/bin/env pwsh
# Script de monitoring du téléchargement

Write-Host "=" -NoNewline -ForegroundColor Cyan
Write-Host "=" * 68 -ForegroundColor Cyan
Write-Host "📊 MONITORING - Conversion TinyLlama 1.1B" -ForegroundColor Cyan
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""

$outputFile = "tinyllama_1b_gqa.bin"
$expectedSize = 600  # MB (INT4 quantized)
$interval = 5  # seconds

Write-Host "Modèle: TinyLlama/TinyLlama-1.1B-Chat-v1.0" -ForegroundColor White
Write-Host "Output: $outputFile" -ForegroundColor White
Write-Host "Taille attendue: ~$expectedSize MB (INT4)" -ForegroundColor White
Write-Host ""
Write-Host "⏳ Téléchargement en cours..." -ForegroundColor Yellow
Write-Host ""

$iteration = 0
while ($true) {
    $iteration++
    
    if (Test-Path $outputFile) {
        $size = (Get-Item $outputFile).Length / 1MB
        $progress = [math]::Min(100, ($size / $expectedSize) * 100)
        
        # Barre de progression
        $barLength = 50
        $filled = [math]::Floor($progress / 100 * $barLength)
        $bar = "█" * $filled + "░" * ($barLength - $filled)
        
        Write-Host "`r[$bar] $([math]::Round($progress,1))% - $([math]::Round($size,2)) MB / $expectedSize MB" -NoNewline -ForegroundColor Green
        
        if ($size -ge ($expectedSize * 0.95)) {
            Write-Host ""
            Write-Host ""
            Write-Host "✅ Téléchargement terminé!" -ForegroundColor Green
            Write-Host ""
            Write-Host "Taille finale: $([math]::Round($size,2)) MB" -ForegroundColor White
            break
        }
    } else {
        # Animation d'attente
        $spinner = @('⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏')
        $char = $spinner[$iteration % $spinner.Length]
        Write-Host "`r$char Téléchargement depuis Hugging Face... (tentative $iteration)" -NoNewline -ForegroundColor Yellow
    }
    
    Start-Sleep -Seconds $interval
}

Write-Host ""
Write-Host "🔍 Validation du fichier..." -ForegroundColor Cyan
Write-Host ""

# Valider le format
if (Test-Path "test_llama3_support.py") {
    python test_llama3_support.py $outputFile
} else {
    Write-Host "⚠️  Script de validation non trouvé" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "✅ CONVERSION TERMINÉE" -ForegroundColor Green
Write-Host "=" * 70 -ForegroundColor Cyan
