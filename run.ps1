# Run QEMU (single entrypoint)
$ErrorActionPreference = 'Stop'

$QEMU = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$OVMF = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"
$IMAGE = Join-Path $PSScriptRoot "llm-baremetal-boot.img"

if (-not (Test-Path $QEMU)) { throw "QEMU not found: $QEMU" }
if (-not (Test-Path $OVMF)) { throw "OVMF not found: $OVMF" }
if (-not (Test-Path $IMAGE)) { throw "Image not found: $IMAGE (run .\build.ps1 first)" }

Write-Host "🚀 Launching QEMU" -ForegroundColor Cyan
Write-Host "  Image: $IMAGE" -ForegroundColor Gray

$args = @(
  "-drive", "if=pflash,format=raw,readonly=on,file=$OVMF",
  "-drive", "format=raw,file=$IMAGE",
  # Headless: keep everything in this terminal (avoids GTK warnings).
  "-display", "none",
  # 110M model needs more RAM; 15M will still work fine with this too.
  "-m", "2048M",
  "-cpu", "qemu64",
  "-serial", "stdio",
  "-monitor", "none"
)

Write-Host "🎮 QEMU running (Ctrl+C to stop)" -ForegroundColor Green
& $QEMU @args
