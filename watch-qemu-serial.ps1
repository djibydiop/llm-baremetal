# Script to monitor QEMU serial output via file redirection
# Usage: .\watch-qemu-serial.ps1

$serialLog = "qemu-serial-output.txt"

Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║         QEMU Serial Monitor - DRC Network Learning            ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "Watching for QEMU output in: $serialLog" -ForegroundColor Yellow
Write-Host "Press Ctrl+C to stop monitoring" -ForegroundColor Yellow
Write-Host ""

# Check if log file exists
if (Test-Path $serialLog) {
    Write-Host "Found existing log file, displaying contents..." -ForegroundColor Green
    Get-Content $serialLog
    Write-Host "`n--- LIVE UPDATES BELOW ---`n" -ForegroundColor Cyan
}

# Monitor for new content
Get-Content $serialLog -Wait -Tail 50
