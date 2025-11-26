# Build script for LLM Bare-Metal v2.0 (Windows)
# Requires WSL with build tools installed

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "Building LLM Bare-Metal v2.0" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

# Check if we're in the right directory
if (-not (Test-Path "llama2_efi.c")) {
    Write-Host "Error: llama2_efi.c not found!" -ForegroundColor Red
    Write-Host "Please run this script from llm-baremetal directory" -ForegroundColor Yellow
    exit 1
}

# Convert Windows path to WSL path
$currentDir = (Get-Location).Path
$wslPath = $currentDir -replace '\\', '/' -replace 'C:', '/mnt/c'

Write-Host "Current directory: $currentDir" -ForegroundColor Gray
Write-Host "WSL path: $wslPath" -ForegroundColor Gray
Write-Host ""

# Clean previous build
Write-Host "Cleaning previous build..." -ForegroundColor Yellow
wsl bash -c "cd '$wslPath' && make clean 2>/dev/null || true"
Write-Host ""

# Compile
Write-Host "Compiling llama2_efi.c with AVX2/FMA optimizations..." -ForegroundColor Yellow
$buildResult = wsl bash -c "cd '$wslPath' && make llama2.efi 2>&1"

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "=============================================" -ForegroundColor Green
    Write-Host "✓ Build successful!" -ForegroundColor Green
    Write-Host "=============================================" -ForegroundColor Green
    Write-Host ""
    
    # Show file info
    if (Test-Path "llama2.efi") {
        $fileInfo = Get-Item "llama2.efi"
        $sizeMB = [math]::Round($fileInfo.Length / 1MB, 2)
        Write-Host "Output: llama2.efi ($sizeMB MB)" -ForegroundColor Green
        Write-Host ""
    }
    
    # Check for models
    Write-Host "Checking for model files..." -ForegroundColor Cyan
    $modelsFound = 0
    
    $models = @(
        @{Name="stories15M.bin"; Size="60MB"},
        @{Name="stories42M.bin"; Size="165MB"},
        @{Name="stories110M.bin"; Size="420MB"},
        @{Name="stories260M.bin"; Size="1GB"},
        @{Name="tinyllama_1b.bin"; Size="4.4GB"},
        @{Name="llama2_7b.bin"; Size="13GB"},
        @{Name="nanogpt.bin"; Size="48MB"}
    )
    
    foreach ($model in $models) {
        if (Test-Path $model.Name) {
            $fileSize = Get-Item $model.Name
            $sizeMB = [math]::Round($fileSize.Length / 1MB, 2)
            Write-Host "  ✓ $($model.Name) ($sizeMB MB)" -ForegroundColor Green
            $modelsFound++
        }
    }
    
    if ($modelsFound -eq 0) {
        Write-Host "  ⚠️  No model files found" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Download at least one model:" -ForegroundColor Yellow
        Write-Host "  Invoke-WebRequest -Uri 'https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin' -OutFile 'stories15M.bin'" -ForegroundColor Gray
        Write-Host "  Invoke-WebRequest -Uri 'https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin' -OutFile 'stories110M.bin'" -ForegroundColor Gray
    } else {
        Write-Host ""
        Write-Host "Found $modelsFound model(s)" -ForegroundColor Green
    }
    
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  1. Download models (if not done): See URLs above" -ForegroundColor White
    Write-Host "  2. Download tokenizer: Invoke-WebRequest -Uri 'https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin' -OutFile 'tokenizer.bin'" -ForegroundColor White
    Write-Host "  3. Create disk image: wsl bash -c 'cd `"$wslPath`" && make disk'" -ForegroundColor White
    Write-Host "  4. Test in QEMU: wsl qemu-system-x86_64 -cpu Haswell -drive if=pflash,format=raw,readonly=on,file=/mnt/c/.../OVMF.fd ..." -ForegroundColor White
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "=============================================" -ForegroundColor Red
    Write-Host "✗ Build failed!" -ForegroundColor Red
    Write-Host "=============================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Build output:" -ForegroundColor Yellow
    Write-Host $buildResult -ForegroundColor Gray
    exit 1
}
