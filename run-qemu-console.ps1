# Launch LLM Baremetal on QEMU (Console Mode - No GUI)
# This version displays output directly in terminal instead of GTK window

param(
    [string]$Model = "15M"
)

$ErrorActionPreference = "Stop"

# Change to script directory
Set-Location $PSScriptRoot

Write-Host "`n╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        LLM BAREMETAL - QEMU LAUNCHER (CONSOLE MODE)         ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Determine model file (auto-detect from disk image)
$modelFile = if ($Model -eq "110M" -or $Model -eq "auto") { "stories110M.bin" } else { "stories15M.bin" }
$modelSize = if ($modelFile -eq "stories110M.bin") { "420MB" } else { "58MB" }
$ramSize = if ($modelFile -eq "stories110M.bin") { "2048M" } else { "1024M" }

Write-Host "[INFO] Configuration:" -ForegroundColor Yellow
Write-Host "  • Model:     $modelFile ($modelSize)" -ForegroundColor White
Write-Host "  • RAM:       $ramSize" -ForegroundColor White
Write-Host "  • CPU:       qemu64 (SSE2 compatible)" -ForegroundColor White
Write-Host "  • Display:   Console mode (no GUI)`n" -ForegroundColor White

# Check required files
Write-Host "[CHECK] Verifying files..." -ForegroundColor Cyan

$requiredFiles = @(
    @{Name="llama2.efi"; Path="llama2.efi"},
    @{Name=$modelFile; Path=$modelFile},
    @{Name="tokenizer.bin"; Path="tokenizer.bin"},
    @{Name="qemu-test.img"; Path="qemu-test.img"}
)

$allFilesPresent = $true
foreach ($file in $requiredFiles) {
    if (Test-Path $file.Path) {
        $size = (Get-Item $file.Path).Length
        $sizeStr = if ($size -gt 1MB) { "{0:N1} MB" -f ($size / 1MB) } else { "{0:N1} KB" -f ($size / 1KB) }
        Write-Host "  ✓ $($file.Name) ($sizeStr)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $($file.Name) NOT FOUND" -ForegroundColor Red
        $allFilesPresent = $false
    }
}

if (-not $allFilesPresent) {
    Write-Host "`n[ERROR] Missing required files." -ForegroundColor Red
    exit 1
}

Write-Host "`n════════════════════════════════════════════════════════════════" -ForegroundColor DarkGray
Write-Host "[LAUNCH] Starting QEMU (Console Mode)..." -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════════════`n" -ForegroundColor DarkGray

Write-Host "Expected behavior:" -ForegroundColor Cyan
Write-Host "  1. UEFI BIOS boot messages will appear below" -ForegroundColor White
Write-Host "  2. llama2.efi will load the model" -ForegroundColor White
Write-Host "  3. Text generation will display in this terminal" -ForegroundColor White
Write-Host "`nControls:" -ForegroundColor Cyan
Write-Host "  • Ctrl+C:  Stop QEMU`n" -ForegroundColor Yellow

Write-Host "────────────────────────────────────────────────────────────────" -ForegroundColor DarkGray

# Convert Windows path to WSL path
$wslPath = "/mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal"

# Launch QEMU in console mode (nographic + serial stdio)
$qemuCmd = "qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=$wslPath/qemu-test.img,format=raw -m $ramSize -cpu qemu64 -smp 2 -nographic -serial mon:stdio -no-reboot"

Write-Host "[QEMU] Launching in console mode...`n" -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════════════`n" -ForegroundColor DarkGray

try {
    wsl bash -c $qemuCmd
} catch {
    Write-Host "`n[ERROR] QEMU launch failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`n════════════════════════════════════════════════════════════════" -ForegroundColor DarkGray
Write-Host "[DONE] QEMU session ended" -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════════════`n" -ForegroundColor DarkGray
