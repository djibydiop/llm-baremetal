#!/usr/bin/env pwsh
# Quick build and test script

Set-Location C:\Users\djibi\Desktop\baremetal\llm-baremetal

Write-Host "=== Building with DRC anti-token-3 fixes ===" -ForegroundColor Cyan

# Build via WSL
$result = wsl bash -c 'cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && make clean && make llama2.efi 2>&1'

Write-Host $result

# Check if successful
if (Test-Path "llama2.efi") {
    $size = (Get-Item "llama2.efi").Length / 1KB
    Write-Host "`n✓ Build OK: $([math]::Round($size, 2)) KB" -ForegroundColor Green
} else {
    Write-Host "`n✗ Build FAILED" -ForegroundColor Red
}
