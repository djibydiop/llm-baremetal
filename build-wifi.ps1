# Build script for WiFi firmware implementation

Write-Host "=== Building llm-baremetal with WiFi firmware ===" -ForegroundColor Cyan

# Clean
Write-Host "`nCleaning..." -ForegroundColor Yellow
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && make clean"

# Build
Write-Host "`nBuilding..." -ForegroundColor Yellow
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && make llama2.efi 2>&1" | Tee-Object -Variable buildOutput

# Check result
if (Test-Path "llama2.efi") {
    $size = (Get-Item "llama2.efi").Length
    Write-Host "`n✓ Build successful!" -ForegroundColor Green
    Write-Host "  Size: $([math]::Round($size/1KB, 2)) KB" -ForegroundColor Green
    
    # Show warnings/errors
    $errors = $buildOutput | Select-String "error:"
    $warnings = $buildOutput | Select-String "warning:"
    
    if ($errors) {
        Write-Host "`nErrors:" -ForegroundColor Red
        $errors | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    }
    
    if ($warnings) {
        Write-Host "`nWarnings: $($warnings.Count)" -ForegroundColor Yellow
    }
} else {
    Write-Host "`n✗ Build failed!" -ForegroundColor Red
    Write-Host "`nLast 20 lines:" -ForegroundColor Yellow
    $buildOutput | Select-Object -Last 20
}
