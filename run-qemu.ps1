# Launch LLM Baremetal on QEMU
# Usage: .\run-qemu.ps1 [model]
# Examples:
#   .\run-qemu.ps1           # Use stories15M (default)
#   .\run-qemu.ps1 110M      # Use stories110M

param(
    [string]$Model = "15M"
)

$ErrorActionPreference = "Stop"

# Change to script directory
Set-Location $PSScriptRoot

Write-Host "`n╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║            LLM BAREMETAL - QEMU LAUNCHER                     ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Determine model file
$modelFile = if ($Model -eq "110M") { "stories110M.bin" } else { "stories15M.bin" }
$modelSize = if ($Model -eq "110M") { "420MB" } else { "58MB" }
$ramSize = if ($Model -eq "110M") { "2048M" } else { "1024M" }

Write-Host "[INFO] Configuration:" -ForegroundColor Yellow
Write-Host "  • Model:     $modelFile ($modelSize)" -ForegroundColor White
Write-Host "  • RAM:       $ramSize" -ForegroundColor White
Write-Host "  • CPU:       2 cores with AVX2 support" -ForegroundColor White
Write-Host "  • Display:   GTK window`n" -ForegroundColor White

# Check required files
Write-Host "[CHECK] Verifying files..." -ForegroundColor Cyan

$requiredFiles = @(
    @{Name="llama2.efi"; Path="llama2.efi"},
    @{Name=$modelFile; Path=$modelFile},
    @{Name="tokenizer.bin"; Path="tokenizer.bin"}
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
    Write-Host "`n[ERROR] Missing required files. Download them first:" -ForegroundColor Red
    Write-Host "  • stories15M.bin:  wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin" -ForegroundColor Yellow
    Write-Host "  • tokenizer.bin:   wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin`n" -ForegroundColor Yellow
    exit 1
}

# Check if disk image exists, create if not
if (-not (Test-Path "qemu-test.img")) {
    Write-Host "`n[INFO] Creating QEMU disk image..." -ForegroundColor Yellow
    Write-Host "  Running: make disk (via WSL)`n" -ForegroundColor White
    
    wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && make disk"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "`n[ERROR] Failed to create disk image" -ForegroundColor Red
        exit 1
    }
    Write-Host "`n  ✓ Disk image created successfully`n" -ForegroundColor Green
} else {
    Write-Host "  ✓ qemu-test.img (existing)`n" -ForegroundColor Green
}

# Launch QEMU
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor DarkGray
Write-Host "[LAUNCH] Starting QEMU..." -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════════════`n" -ForegroundColor DarkGray

Write-Host "Expected behavior:" -ForegroundColor Cyan
Write-Host "  1. UEFI BIOS will boot" -ForegroundColor White
Write-Host "  2. llama2.efi bootloader loads" -ForegroundColor White
Write-Host "  3. Model loads into memory (~10-30 seconds)" -ForegroundColor White
Write-Host "  4. Interactive prompt menu appears" -ForegroundColor White
Write-Host "  5. Demo runs automatically (keyboard input limited in QEMU)" -ForegroundColor White
Write-Host "`nControls:" -ForegroundColor Cyan
Write-Host "  • Ctrl+Alt+G:  Release mouse from QEMU window" -ForegroundColor Yellow
Write-Host "  • Ctrl+C:      Stop QEMU (in this terminal)`n" -ForegroundColor Yellow

Write-Host "────────────────────────────────────────────────────────────────" -ForegroundColor DarkGray

# Convert Windows path to WSL path
$wslPath = "/mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal"

# Launch QEMU via WSL with proper parameters (single line to avoid bash escaping issues)
# Note: Using -cpu host requires KVM. For WSL2, we use qemu64 without AVX2 (SSE fallback in code)
$qemuCmd = "qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=$wslPath/qemu-test.img,format=raw -m $ramSize -cpu qemu64 -smp 2 -display gtk -serial file:$wslPath/qemu-run.log -no-reboot -no-shutdown"

Write-Host "[QEMU] Launching...`n" -ForegroundColor Green

try {
    wsl bash -c "$qemuCmd"
} catch {
    Write-Host "`n[ERROR] QEMU launch failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`n════════════════════════════════════════════════════════════════" -ForegroundColor DarkGray
Write-Host "[DONE] QEMU session ended" -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor DarkGray

# Show log if exists
if (Test-Path "qemu-run.log") {
    $logSize = (Get-Item "qemu-run.log").Length
    if ($logSize -gt 0) {
        Write-Host "`n[LOG] Serial output saved to qemu-run.log ($logSize bytes)" -ForegroundColor Cyan
    }
}

Write-Host ""
