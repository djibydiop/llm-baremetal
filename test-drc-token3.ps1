#!/usr/bin/env pwsh
# Test DRC token generation improvements

Set-Location C:\Users\djibi\Desktop\baremetal\llm-baremetal

Write-Host "=== Testing DRC anti-token-3 improvements ===" -ForegroundColor Cyan
Write-Host "Creating test disk with updated binary..." -ForegroundColor Yellow

# Create fresh disk image
$diskResult = wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && make disk 2>&1'

if (Test-Path "qemu-test.img") {
    Write-Host "✓ Disk image ready" -ForegroundColor Green
    
    Write-Host "`nLaunching QEMU test (30 seconds)..." -ForegroundColor Yellow
    
    # Remove old serial output
    if (Test-Path "qemu-serial.txt") { Remove-Item "qemu-serial.txt" }
    
    # Launch QEMU with serial output
    $qemu = Start-Process -FilePath "C:\Program Files\qemu\qemu-system-x86_64.exe" `
        -ArgumentList "-bios","OVMF.fd","-drive","format=raw,file=qemu-test.img","-m","512M","-serial","file:qemu-serial.txt","-nographic" `
        -NoNewWindow -PassThru
    
    # Wait 30 seconds
    Start-Sleep -Seconds 30
    
    # Kill QEMU
    Stop-Process -Id $qemu.Id -Force -ErrorAction SilentlyContinue
    
    # Show results
    Write-Host "`n=== GENERATION RESULTS ===" -ForegroundColor Green
    
    if (Test-Path "qemu-serial.txt") {
        # Extract token generation lines
        $content = Get-Content "qemu-serial.txt" -Raw
        
        # Look for DRC status and token patterns
        $drcLines = $content -split "`n" | Select-String -Pattern "(DRC|token|Token|generation|Ghost)"
        
        if ($drcLines) {
            Write-Host "`nDRC Activity:" -ForegroundColor Cyan
            $drcLines | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
        }
        
        # Check for token 3 repetition
        $token3Count = ($content -split "`n" | Select-String -Pattern "token.*3" -AllMatches).Count
        
        Write-Host "`n--- Token 3 Analysis ---" -ForegroundColor Yellow
        if ($token3Count -gt 5) {
            Write-Host "  ⚠ Token 3 still appearing: $token3Count times" -ForegroundColor Red
        } elseif ($token3Count -gt 0) {
            Write-Host "  ⚠ Token 3 mentioned: $token3Count times (investigating)" -ForegroundColor Yellow
        } else {
            Write-Host "  ✓ Token 3 successfully suppressed!" -ForegroundColor Green
        }
        
        # Show last 30 lines of output
        Write-Host "`n--- Last 30 lines ---" -ForegroundColor Cyan
        Get-Content "qemu-serial.txt" | Select-Object -Last 30
        
    } else {
        Write-Host "No serial output captured" -ForegroundColor Red
    }
    
} else {
    Write-Host "✗ Disk creation failed" -ForegroundColor Red
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Cyan
