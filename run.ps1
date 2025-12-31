param(
  [switch]$NewWindow,
  [switch]$Gui,
  # If set, returns QEMU's raw exit code instead of normalizing.
  [switch]$PassThroughExitCode
)

# Run QEMU (single entrypoint)
$ErrorActionPreference = 'Stop'

$QEMU = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$OVMF = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"
$IMAGE = Join-Path $PSScriptRoot "llm-baremetal-boot.img"

if (-not (Test-Path $QEMU)) { throw "QEMU not found: $QEMU" }
if (-not (Test-Path $OVMF)) { throw "OVMF not found: $OVMF" }
if (-not (Test-Path $IMAGE)) { throw "Image not found: $IMAGE (run .\build.ps1 first)" }

Write-Host "[Run] Launching QEMU" -ForegroundColor Cyan
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

Write-Host "[Run] QEMU running (Ctrl+C to stop)" -ForegroundColor Green
if ($NewWindow) {
  Write-Host "[Run] Launching QEMU in a new window" -ForegroundColor Yellow

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

  $code = $LASTEXITCODE

  if ($PassThroughExitCode) {
    exit $code
  }

  # Normalize common QEMU exit codes so scripts don't fail on expected exits.
  # QEMU often returns 1 for various benign termination paths (window close, firmware reset, etc.).
  if ($code -eq 0 -or $code -eq 1) {
    if ($code -ne 0) {
      Write-Host ("[Run] QEMU exited with code {0} (normalized to success)" -f $code) -ForegroundColor Yellow
    }
    exit 0
  }

  Write-Host ("[Run] QEMU exited with code {0}" -f $code) -ForegroundColor Red
  exit $code
}
