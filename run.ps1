param(
  [switch]$NewWindow,
  [switch]$Gui
)

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
  # 110M model needs more RAM; 15M will still work fine with this too.
  "-m", "2048M",
  "-cpu", "qemu64",
  "-monitor", "none"
)

if ($Gui) {
  # Graphical QEMU window (keyboard input works in the window).
  # Avoid GTK dependencies/warnings by using SDL.
  $args = @(
    "-display", "sdl"
  ) + $args
} else {
  # Headless: keep everything in this terminal (avoids GTK warnings).
  $args = @(
    "-display", "none",
    "-serial", "stdio"
  ) + $args
}

Write-Host "🎮 QEMU running (Ctrl+C to stop)" -ForegroundColor Green
if ($NewWindow) {
  Write-Host "🪟 Launching QEMU in a new window" -ForegroundColor Yellow

  function Quote-PS([string]$s) {
    if ($s -match '^[a-zA-Z0-9_\-\.:,=]+$') { return $s }
    return "'" + ($s -replace "'", "''") + "'"
  }

  if ($Gui) {
    # GUI mode doesn't require stdio; start QEMU directly.
    Start-Process -FilePath $QEMU -WorkingDirectory $PSScriptRoot -ArgumentList $args | Out-Null
  } else {
    $qemuCmd = "& " + (Quote-PS $QEMU) + " " + (($args | ForEach-Object { Quote-PS $_ }) -join ' ')
    # Use a new PowerShell console so QEMU can bind to stdio for REPL input/output.
    Start-Process -FilePath "powershell.exe" -WorkingDirectory $PSScriptRoot -ArgumentList @(
      "-NoExit",
      "-Command",
      $qemuCmd
    ) | Out-Null
  }
} else {
  & $QEMU @args
}
