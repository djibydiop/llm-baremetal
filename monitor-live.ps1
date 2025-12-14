# Real-time DRC v3.0 Monitor
# Shows live progress of token generation

$ErrorActionPreference = "SilentlyContinue"

Write-Host "`n╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║       DRC v3.0 LIVE MONITOR - Real-time Progress         ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$lastSize = 0
$startTime = Get-Date

Write-Host "⏰ Started at: $(Get-Date -Format 'HH:mm:ss')`n" -ForegroundColor Yellow

for ($i = 1; $i -le 60; $i++) {
    Start-Sleep -Seconds 5
    
    if (-not (Test-Path qemu-serial-output.txt)) {
        Write-Host "  [${i}] Waiting for log file..." -ForegroundColor Gray
        continue
    }
    
    $size = (Get-Item qemu-serial-output.txt).Length
    $content = Get-Content qemu-serial-output.txt -ErrorAction SilentlyContinue
    
    # Extract metrics
    $maxPos = ($content | Select-String "\[FORWARD-ENTRY\] pos=(\d+)" | ForEach-Object { 
        if ($_ -match "pos=(\d+)") { [int]$matches[1] } 
    } | Measure-Object -Maximum).Maximum
    
    $drcLines = ($content | Select-String "DRC").Count
    $expertLines = ($content | Select-String "EXPERT").Count
    $complete = $content | Select-String "Generation complete"
    
    $elapsed = ((Get-Date) - $startTime).TotalSeconds
    $tokensPerSec = if ($maxPos -gt 0) { [math]::Round($maxPos / $elapsed, 2) } else { 0 }
    
    # Calculate progress bar
    $progress = [math]::Min([math]::Floor(($maxPos / 150.0) * 20), 20)
    $bar = "=" * $progress + " " * (20 - $progress)
    
    # Status color
    $color = "White"
    if ($maxPos -gt 100) { $color = "Green" }
    elseif ($maxPos -gt 50) { $color = "Yellow" }
    
    # Display
    Clear-Host
    Write-Host "`n╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║       DRC v3.0 LIVE MONITOR - Real-time Progress         ║" -ForegroundColor Cyan
    Write-Host "╚═══════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
    
    Write-Host "⏰ Elapsed: $([math]::Floor($elapsed))s | Iteration: $i/60`n" -ForegroundColor Yellow
    
    Write-Host "📊 PROGRESS:" -ForegroundColor Cyan
    Write-Host "  [$bar] $maxPos/150 tokens" -ForegroundColor $color
    Write-Host "  Speed: $tokensPerSec tokens/sec`n" -ForegroundColor White
    
    Write-Host "📈 ACTIVITY:" -ForegroundColor Cyan
    Write-Host "  DRC Messages: $drcLines" -ForegroundColor White
    Write-Host "  EXPERT Actions: $expertLines" -ForegroundColor White
    Write-Host "  Log Size: $([math]::Round($size/1KB, 1)) KB`n" -ForegroundColor White
    
    if ($complete) {
        Write-Host "✅ GÉNÉRATION COMPLÈTE!" -ForegroundColor Green
        Write-Host "`nAppuyez sur une touche pour quitter..." -ForegroundColor Yellow
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        break
    }
    
    if ($maxPos -eq $lastSize -and $i -gt 10) {
        Write-Host "⚠️  WARNING: Position n'a pas changé depuis 5s" -ForegroundColor Yellow
    }
    
    $lastSize = $maxPos
}

Write-Host "`n✓ Monitoring terminé`n" -ForegroundColor Green
Write-Host "📋 Pour voir les logs: Get-Content qemu-serial-output.txt | Select-String DRC`n" -ForegroundColor Cyan
