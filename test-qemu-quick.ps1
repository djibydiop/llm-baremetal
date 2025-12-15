# Quick QEMU Test Script for DRC v6.0
# Tests all 9 phases + CWEB + GGUF support

Write-Host "=== DRC v6.0 QEMU Test ===" -ForegroundColor Cyan
Write-Host "Binary: llama2.efi (684KB with GGUF support)" -ForegroundColor Green
Write-Host "Starting test in 3 seconds..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

$qemuPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$workDir = "C:\Users\djibi\Desktop\baremetal\llm-baremetal"

# Check if QEMU exists
if (-not (Test-Path $qemuPath)) {
    Write-Host "QEMU not found at: $qemuPath" -ForegroundColor Red
    Write-Host "Trying WSL QEMU..." -ForegroundColor Yellow
    
    # Use WSL QEMU with background process
    Set-Location $workDir
    
    $proc = Start-Process -FilePath "wsl" `
        -ArgumentList "bash", "-c", "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && qemu-system-x86_64 -bios OVMF.fd -drive format=raw,file=qemu-test.img -m 512M -nographic" `
        -NoNewWindow `
        -PassThru `
        -RedirectStandardOutput "qemu-output.txt" `
        -RedirectStandardError "qemu-error.txt"
    
    Write-Host "QEMU started (PID: $($proc.Id))" -ForegroundColor Green
    Write-Host "Waiting 60 seconds for boot, loading, and inference..." -ForegroundColor Yellow
    
    Start-Sleep -Seconds 60
    
    # Kill QEMU
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    
    Write-Host "`n=== QEMU Output ===" -ForegroundColor Cyan
    if (Test-Path "qemu-output.txt") {
        Get-Content "qemu-output.txt" | Select-Object -Last 100
    }
    
    Write-Host "`n=== QEMU Errors ===" -ForegroundColor Cyan
    if (Test-Path "qemu-error.txt") {
        Get-Content "qemu-error.txt" | Select-Object -Last 20
    }
    
} else {
    Write-Host "Using Windows QEMU: $qemuPath" -ForegroundColor Green
    
    $proc = Start-Process -FilePath $qemuPath `
        -ArgumentList "-bios", "$workDir\OVMF.fd", `
                      "-drive", "format=raw,file=$workDir\qemu-test.img", `
                      "-m", "512M", `
                      "-nographic" `
        -NoNewWindow `
        -PassThru `
        -RedirectStandardOutput "$workDir\qemu-output.txt" `
        -RedirectStandardError "$workDir\qemu-error.txt"
    
    Write-Host "QEMU started (PID: $($proc.Id))" -ForegroundColor Green
    Write-Host "Waiting 60 seconds for complete test..." -ForegroundColor Yellow
    
    Start-Sleep -Seconds 60
    
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    
    Write-Host "`n=== Output ===" -ForegroundColor Cyan
    Get-Content "$workDir\qemu-output.txt" -ErrorAction SilentlyContinue | Select-Object -Last 100
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Green
