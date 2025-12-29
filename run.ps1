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
  "-m", "512M",
  "-cpu", "qemu64",
  "-serial", "stdio",
  "-monitor", "none"
)

Write-Host "🎮 QEMU running (Ctrl+C to stop)" -ForegroundColor Green
& $QEMU @args
