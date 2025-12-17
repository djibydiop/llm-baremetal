#!/usr/bin/env pwsh
# Extended DRC test - wait for full generation

Set-Location C:\Users\djibi\Desktop\baremetal\llm-baremetal

Write-Host "=== DRC Extended Generation Test ===" -ForegroundColor Cyan
Write-Host "This will take 120 seconds to capture full generation..." -ForegroundColor Yellow

if (Test-Path "qemu-serial.txt") { Remove-Item "qemu-serial.txt" }

Write-Host "`nLaunching QEMU..." -ForegroundColor Cyan

$qemu = Start-Process -FilePath "C:\Program Files\qemu\qemu-system-x86_64.exe" `
    -ArgumentList "-bios","OVMF.fd","-drive","format=raw,file=qemu-test.img","-m","512M","-serial","file:qemu-serial.txt","-nographic" `
    -NoNewWindow -PassThru

Write-Host "Waiting 120 seconds for generation..." -ForegroundColor Yellow

# Progress bar
for ($i = 1; $i -le 120; $i++) {
    Start-Sleep -Seconds 1
    if ($i % 10 -eq 0) {
        Write-Host "  $i seconds..." -ForegroundColor Gray
    }
}

Write-Host "`nStopping QEMU..." -ForegroundColor Cyan
Stop-Process -Id $qemu.Id -Force -ErrorAction SilentlyContinue

Start-Sleep -Seconds 2

Write-Host "`n=== ANALYSIS ===" -ForegroundColor Green

if (Test-Path "qemu-serial.txt") {
    $content = Get-Content "qemu-serial.txt" -Raw
    
    # Look for generated story text
    Write-Host "`n--- Story Generation ---" -ForegroundColor Cyan
    $storyLines = $content -split "`n" | Select-String -Pattern "^[A-Z][a-z]" | Select-Object -First 20
    if ($storyLines) {
        $storyLines | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
    } else {
        Write-Host "  No story text found yet" -ForegroundColor Yellow
    }
    
    # Token analysis
    Write-Host "`n--- Token Statistics ---" -ForegroundColor Cyan
    
    # Count mentions of specific tokens
    $token0 = ($content | Select-String -Pattern "token.*0" -AllMatches).Matches.Count
    $token1 = ($content | Select-String -Pattern "token.*1" -AllMatches).Matches.Count
    $token2 = ($content | Select-String -Pattern "token.*2" -AllMatches).Matches.Count
    $token3 = ($content | Select-String -Pattern "token.*3" -AllMatches).Matches.Count
    
    Write-Host "  Token 0 mentions: $token0" -ForegroundColor $(if ($token0 -gt 5) {"Red"} else {"Green"})
    Write-Host "  Token 1 mentions: $token1" -ForegroundColor $(if ($token1 -gt 5) {"Red"} else {"Green"})
    Write-Host "  Token 2 mentions: $token2" -ForegroundColor $(if ($token2 -gt 5) {"Red"} else {"Green"})
    Write-Host "  Token 3 mentions: $token3" -ForegroundColor $(if ($token3 -gt 5) {"Red"} else {"Green"})
    
    # DRC intervention stats
    Write-Host "`n--- DRC Interventions ---" -ForegroundColor Cyan
    $drcStats = $content -split "`n" | Select-String -Pattern "(blacklist|emergency|escape|penalty)"
    if ($drcStats) {
        $drcStats | Select-Object -First 10 | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
    }
    
    Write-Host "`n--- Full Output (last 50 lines) ---" -ForegroundColor Gray
    Get-Content "qemu-serial.txt" | Select-Object -Last 50
    
} else {
    Write-Host "No output captured!" -ForegroundColor Red
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Cyan
