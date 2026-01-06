param(
  [switch]$NewWindow,
  [switch]$Gui,
  # If set, returns QEMU's raw exit code instead of normalizing.
  [switch]$PassThroughExitCode,

  # If set, do not normalize common non-zero QEMU exit codes.
  [switch]$NoNormalizeExitCode,

  # QEMU accelerator: auto tries WHPX (Windows Hypervisor Platform) then falls back.
  [ValidateSet('auto','whpx','tcg','none')]
  [string]$Accel = 'auto',

  # CPU model override. 'auto' keeps the existing behavior (host on WHPX, qemu64 otherwise).
  [ValidateSet('auto','host','max','qemu64')]
  [string]$Cpu = 'auto',

  # When set, request AVX2/FMA exposure in the guest CPU model (best-effort; depends on accel/model).
  [switch]$ForceAvx2,

  # Machine type (chipset). q35 can behave better on some WHPX setups.
  [ValidateSet('pc','q35')]
  [string]$Machine = 'pc',

  # Guest RAM in MB (increase for larger models)
  [ValidateRange(512, 8192)]
  [int]$MemMB = 4096,

  # Optional overrides (useful if QEMU/OVMF aren't in the default locations)
  [string]$QemuPath,
  [string]$OvmfPath,
  [string]$ImagePath
)

# Run QEMU (single entrypoint)
$ErrorActionPreference = 'Stop'

function Resolve-FirstExistingPath([string[]]$paths) {
  foreach ($p in $paths) {
    if ($p -and (Test-Path $p)) { return $p }
  }
  return $null
}

function Resolve-QemuPath([string]$override) {
  if ($override) {
    if (-not (Test-Path $override)) { throw "QEMU not found at -QemuPath: $override" }
    return $override
  }

  $cmd = Get-Command qemu-system-x86_64.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }

  $common = @(
    "C:\\Program Files\\qemu\\qemu-system-x86_64.exe",
    "C:\\Program Files (x86)\\qemu\\qemu-system-x86_64.exe",
    "C:\\msys64\\mingw64\\bin\\qemu-system-x86_64.exe",
    "C:\\msys64\\usr\\bin\\qemu-system-x86_64.exe"
  )
  $found = Resolve-FirstExistingPath $common
  if ($found) { return $found }

  throw "QEMU not found. Provide -QemuPath or ensure qemu-system-x86_64.exe is on PATH."
}

function Resolve-OvmfPath([string]$override) {
  if ($override) {
    if (-not (Test-Path $override)) { throw "OVMF not found at -OvmfPath: $override" }
    return $override
  }

  $common = @(
    "C:\\Program Files\\qemu\\share\\edk2-x86_64-code.fd",
    "C:\\Program Files (x86)\\qemu\\share\\edk2-x86_64-code.fd",
    "C:\\msys64\\usr\\share\\edk2-ovmf\\x64\\OVMF_CODE.fd",
    "C:\\msys64\\usr\\share\\ovmf\\x64\\OVMF_CODE.fd"
  )
  $found = Resolve-FirstExistingPath $common
  if ($found) { return $found }

  throw "OVMF not found. Provide -OvmfPath (UEFI firmware .fd)."
}

function Resolve-ImagePath([string]$override) {
  if ($override) {
    if (-not (Test-Path $override)) { throw "Image not found at -ImagePath: $override" }
    return $override
  }

  # Prefer the most recently written image (supports timestamped images when an older one is locked by QEMU).
  $candidates = Get-ChildItem -Path $PSScriptRoot -Filter 'llm-baremetal-boot*.img' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending
  if ($candidates -and $candidates.Count -gt 0) {
    return $candidates[0].FullName
  }

  $fallback = (Join-Path $PSScriptRoot 'llm-baremetal-boot.img')
  throw "Image not found: $fallback (run .\\build.ps1 first)"
}

$QEMU = Resolve-QemuPath $QemuPath
$OVMF = Resolve-OvmfPath $OvmfPath
$IMAGE = Resolve-ImagePath $ImagePath

# Writable vars pflash (some OVMF builds behave better with a vars store)
$OVMF_VARS = Join-Path $PSScriptRoot "ovmf-vars-temp.fd"
if (-not (Test-Path $OVMF_VARS)) {
  $bytes = New-Object byte[] 131072
  [System.IO.File]::WriteAllBytes($OVMF_VARS, $bytes)
}

Write-Host "[Run] Launching QEMU" -ForegroundColor Cyan
Write-Host "  QEMU:  $QEMU" -ForegroundColor Gray
Write-Host "  OVMF:  $OVMF" -ForegroundColor Gray
Write-Host "  Image: $IMAGE" -ForegroundColor Gray
Write-Host "  Accel: $Accel" -ForegroundColor Gray
function Build-QemuArgs([string]$accelMode) {
  # Map friendly accel names to QEMU accel strings.
  $accelArg = $accelMode
  if ($accelArg -eq 'tcg') { $accelArg = 'tcg,thread=multi' }

  # Pick a CPU model suitable for the accelerator.
  $cpuModel = 'qemu64'
  if ($accelMode -eq 'whpx') { $cpuModel = 'host' }

  if ($Cpu -ne 'auto') {
    $cpuModel = $Cpu
  }

  if ($ForceAvx2) {
    # Best-effort feature exposure. Note: with some accelerators/CPU models,
    # QEMU may ignore unsupported flags.
    if ($cpuModel -notmatch ',') {
      $cpuModel = "$cpuModel,avx2=on,fma=on"
    } else {
      $cpuModel = "$cpuModel,avx2=on,fma=on"
    }
  }

  $a = @(
    "-drive", "if=pflash,format=raw,readonly=on,file=$OVMF",
    "-drive", "if=pflash,format=raw,file=$OVMF_VARS",
    "-drive", "format=raw,file=$IMAGE",
    "-machine", $Machine,
    # Guest RAM
    "-m", ("{0}M" -f $MemMB),
    "-cpu", $cpuModel,
    # Use 2 vCPUs when accelerator is available.
    "-smp", "2",
    "-monitor", "none"
  )

  if ($accelArg -and $accelMode -ne 'none') {
    $a = @("-accel", $accelArg) + $a
  }

  if ($Gui) {
    # Graphical QEMU window (keyboard input works in the window).
    $a = @(
      "-display", "sdl",
      "-serial", "stdio"
    ) + $a
  } else {
    # Headless: keep everything in this terminal.
    $a = @(
      "-display", "none",
      "-serial", "stdio"
    ) + $a
  }

  return $a
}

$accelMode = $null
if ($Accel -ne 'none') { $accelMode = $Accel }
$args = Build-QemuArgs $accelMode

if ($Accel -eq 'auto') {
  # Prefer WHPX when available; fall back to no explicit accelerator.
  $args = Build-QemuArgs 'whpx'
}

function Invoke-QemuNewWindow([string[]]$argv) {
  Write-Host "[Run] Launching QEMU in a new window" -ForegroundColor Yellow

  function Quote-PS([string]$s) {
    if ($s -match '^[a-zA-Z0-9_\-\.:,=]+$') { return $s }
    return "'" + ($s -replace "'", "''") + "'"
  }

  $qemuCmd = "& " + (Quote-PS $QEMU) + " " + (($argv | ForEach-Object { Quote-PS $_ }) -join ' ')
  Start-Process -FilePath "powershell.exe" -WorkingDirectory $PSScriptRoot -ArgumentList @(
    "-NoExit",
    "-Command",
    $qemuCmd
  ) | Out-Null
}

Write-Host "[Run] QEMU running (Ctrl+C to stop)" -ForegroundColor Green

if ($NewWindow) {
  Invoke-QemuNewWindow $args
  exit 0
}

$prev = $ErrorActionPreference
try {
  # Native stderr can be surfaced as PowerShell error records; don't let that abort the script.
  $ErrorActionPreference = 'Continue'

  if ($Accel -eq 'auto') {
    try {
      & $QEMU @args
      $code = $LASTEXITCODE
    } catch [System.Management.Automation.PipelineStoppedException] {
      # Ctrl+C or pipeline stop: treat as user-initiated stop.
      $code = 0
    } catch [System.OperationCanceledException] {
      $code = 0
    }

    # If WHPX isn't available, QEMU typically exits immediately with an error.
    if ($code -ne 0) {
      Write-Host ("[Run] WHPX accel failed (exit {0}); retrying with TCG" -f $code) -ForegroundColor Yellow
      $args = Build-QemuArgs 'tcg'
      try {
        & $QEMU @args
        $code = $LASTEXITCODE
      } catch [System.Management.Automation.PipelineStoppedException] {
        $code = 0
      } catch [System.OperationCanceledException] {
        $code = 0
      }
    }
  } else {
    try {
      & $QEMU @args
      $code = $LASTEXITCODE
    } catch [System.Management.Automation.PipelineStoppedException] {
      $code = 0
    } catch [System.OperationCanceledException] {
      $code = 0
    }
  }
} finally {
  $ErrorActionPreference = $prev
}

# If WHPX was explicitly requested and it failed, retry with TCG so the workflow keeps working.
# (Unless the caller asked for raw exit codes.)
if (-not $PassThroughExitCode -and $Accel -eq 'whpx' -and $code -ne 0) {
  Write-Host ("[Run] WHPX failed (exit {0}); retrying with TCG" -f $code) -ForegroundColor Yellow
  $args = Build-QemuArgs 'tcg'
  try {
    & $QEMU @args
    $code = $LASTEXITCODE
  } catch [System.Management.Automation.PipelineStoppedException] {
    $code = 0
  } catch [System.OperationCanceledException] {
    $code = 0
  }
}

if ($PassThroughExitCode) {
  exit $code
}

if (-not $NoNormalizeExitCode) {
  # Normalize common "user exited" codes. (0, 1, 2 are common QEMU exits; 130 is SIGINT; 0xC000013A is Ctrl+C on Windows.)
  if ($code -eq 0 -or $code -eq 1 -or $code -eq 2 -or $code -eq 130 -or $code -eq 3221225786) {
    if ($code -ne 0) {
      Write-Host ("[Run] QEMU exited with code {0} (normalized to success)" -f $code) -ForegroundColor Yellow
    }
    exit 0
  }
}

Write-Host ("[Run] QEMU exited with code {0}" -f $code) -ForegroundColor Red
exit $code
