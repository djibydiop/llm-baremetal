param(
    # Keep backward compatibility with previous usage.
    [switch]$Headless = $true,

    [ValidateSet('auto','whpx','tcg','none')]
    [string]$Accel = 'tcg',

    [ValidateRange(512, 8192)]
    [int]$MemMB = 4096,

    [ValidateRange(10, 600)]
    [int]$TimeoutSec = 90,

    [string]$ModelBin = 'stories15M.bin',
    [switch]$SkipBuild,

    [string]$QemuPath,
    [string]$OvmfPath,
    [string]$ImagePath,

    [switch]$ForceAvx2
)

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
        "C:\Program Files\qemu\qemu-system-x86_64.exe",
        "C:\Program Files (x86)\qemu\qemu-system-x86_64.exe",
        "C:\msys64\mingw64\bin\qemu-system-x86_64.exe",
        "C:\msys64\usr\bin\qemu-system-x86_64.exe"
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
        "C:\Program Files\qemu\share\edk2-x86_64-code.fd",
        "C:\Program Files (x86)\qemu\share\edk2-x86_64-code.fd",
        "C:\msys64\usr\share\edk2-ovmf\x64\OVMF_CODE.fd",
        "C:\msys64\usr\share\ovmf\x64\OVMF_CODE.fd"
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

    $candidates = Get-ChildItem -Path $PSScriptRoot -Filter 'llm-baremetal-boot*.img' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending
    if ($candidates -and $candidates.Count -gt 0) {
        return $candidates[0].FullName
    }

    throw "Image not found (run .\build.ps1 first)"
}

Write-Host "`n[Test] QEMU smoke test" -ForegroundColor Cyan

if (-not $SkipBuild) {
    Write-Host "[Test] Building + creating image..." -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'build.ps1') -ModelBin $ModelBin
}

$QEMU = Resolve-QemuPath $QemuPath
$OVMF = Resolve-OvmfPath $OvmfPath
$IMAGE = Resolve-ImagePath $ImagePath

$OVMF_VARS = Join-Path $PSScriptRoot 'ovmf-vars-temp.fd'
if (-not (Test-Path $OVMF_VARS)) {
    $bytes = New-Object byte[] 131072
    [System.IO.File]::WriteAllBytes($OVMF_VARS, $bytes)
}

$accelArg = $Accel
if ($accelArg -eq 'tcg') { $accelArg = 'tcg,thread=multi' }

$cpuModel = 'qemu64'
if ($Accel -eq 'whpx') { $cpuModel = 'host' }
if ($ForceAvx2) { $cpuModel = "$cpuModel,avx2=on,fma=on" }

$displayArgs = @('-display', 'none')
if (-not $Headless) { $displayArgs = @('-display', 'sdl') }

$args = @(
    '-serial', 'stdio',
    '-monitor', 'none',
    '-drive', "if=pflash,format=raw,readonly=on,file=$OVMF",
    '-drive', "if=pflash,format=raw,file=$OVMF_VARS",
    '-drive', "format=raw,file=$IMAGE",
    '-machine', 'pc',
    '-m', ("{0}M" -f $MemMB),
    '-cpu', $cpuModel,
    '-smp', '2'
)

if ($Accel -and $Accel -ne 'none') {
    $args = @('-accel', $accelArg) + $args
}

$args = $displayArgs + $args

Write-Host "  QEMU:    $QEMU" -ForegroundColor Gray
Write-Host "  OVMF:    $OVMF" -ForegroundColor Gray
Write-Host "  Image:   $IMAGE" -ForegroundColor Gray
Write-Host "  Accel:   $Accel" -ForegroundColor Gray
Write-Host "  Timeout: ${TimeoutSec}s" -ForegroundColor Gray

$needles = @('Entering chat loop', 'CHAT MODE ACTIVE')
$tmpOut = Join-Path $env:TEMP ("llm-baremetal-qemu-out-{0}.txt" -f ([Guid]::NewGuid().ToString('n')))
$tmpErr = Join-Path $env:TEMP ("llm-baremetal-qemu-err-{0}.txt" -f ([Guid]::NewGuid().ToString('n')))
$serialLog = Join-Path $env:TEMP ("llm-baremetal-serial-{0}.txt" -f ([Guid]::NewGuid().ToString('n')))

# Route serial console to a file so we can reliably scan boot output.
$args = $args | ForEach-Object { $_ }
for ($i = 0; $i -lt $args.Count; $i++) {
    if ($args[$i] -eq 'stdio' -and $i -gt 0 -and $args[$i-1] -eq '-serial') {
        $args[$i] = ("file:{0}" -f $serialLog)
    }
}

Write-Host "[Test] Starting QEMU..." -ForegroundColor Cyan

function Quote-CmdArg([string]$s) {
    if ($null -eq $s) { return '""' }
    # If it contains spaces or quotes, wrap it.
    if ($s -match '[\s"]') {
        return '"' + ($s -replace '"', '\\"') + '"'
    }
    return $s
}

$argString = ($args | ForEach-Object { Quote-CmdArg $_ }) -join ' '
$p = Start-Process -FilePath $QEMU -ArgumentList $argString -PassThru -NoNewWindow -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr

$sw = [System.Diagnostics.Stopwatch]::StartNew()
try {
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        if ($p.HasExited) {
            $tailOut = @()
            $tailErr = @()
            if (Test-Path $tmpOut) { $tailOut = Get-Content -Path $tmpOut -Tail 60 -ErrorAction SilentlyContinue }
            if (Test-Path $tmpErr) { $tailErr = Get-Content -Path $tmpErr -Tail 60 -ErrorAction SilentlyContinue }
            Write-Host "[Test] QEMU exited early with code $($p.ExitCode)" -ForegroundColor Red
            if ($tailErr.Count -gt 0) {
                Write-Host "[stderr tail]" -ForegroundColor Yellow
                $tailErr | ForEach-Object { Write-Host $_ }
            }
            if ($tailOut.Count -gt 0) {
                Write-Host "[stdout tail]" -ForegroundColor Yellow
                $tailOut | ForEach-Object { Write-Host $_ }
            }
            throw "QEMU exited early with code $($p.ExitCode)"
        }

        $tailSerial = @()
        $tailErr = @()
        if (Test-Path $serialLog) { $tailSerial = Get-Content -Path $serialLog -Tail 300 -ErrorAction SilentlyContinue }
        if (Test-Path $tmpErr) { $tailErr = Get-Content -Path $tmpErr -Tail 120 -ErrorAction SilentlyContinue }

        $joined = @($tailSerial + $tailErr) -join "`n"
        foreach ($n in $needles) {
            if ($joined -like ("*{0}*" -f $n)) {
                Write-Host "[OK] Boot reached REPL (saw: '$n')" -ForegroundColor Green
                exit 0
            }
        }

        Start-Sleep -Milliseconds 200
    }

    $tailSerial = @()
    $tailErr = @()
    if (Test-Path $serialLog) { $tailSerial = Get-Content -Path $serialLog -Tail 160 -ErrorAction SilentlyContinue }
    if (Test-Path $tmpErr) { $tailErr = Get-Content -Path $tmpErr -Tail 120 -ErrorAction SilentlyContinue }
    Write-Host "[Test] Timeout: did not reach REPL within ${TimeoutSec}s" -ForegroundColor Red
    if ($tailErr.Count -gt 0) {
        Write-Host "[stderr tail]" -ForegroundColor Yellow
        $tailErr | ForEach-Object { Write-Host $_ }
    }
    if ($tailSerial.Count -gt 0) {
        Write-Host "[serial tail]" -ForegroundColor Yellow
        $tailSerial | ForEach-Object { Write-Host $_ }
    }
    throw "Timeout: did not reach REPL within ${TimeoutSec}s"
} finally {
    if ($p -and -not $p.HasExited) {
        Write-Host "[Test] Stopping QEMU..." -ForegroundColor Yellow
        try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
    Remove-Item -Path $tmpOut, $tmpErr, $serialLog -Force -ErrorAction SilentlyContinue
}
