# Build + create bootable image using WSL (single entrypoint)
$ErrorActionPreference = 'Stop'

Write-Host "`n[Build] Build + Image (WSL)" -ForegroundColor Cyan

$winRepo = ($PSScriptRoot -replace '\\','/')
$wslRepo = (wsl wslpath -a $winRepo 2>$null)
if (-not $wslRepo) { throw "Failed to convert path to WSL path via wslpath: $PSScriptRoot" }
$wslRepo = $wslRepo.Trim()
$wslScript = "set -e; cd '$wslRepo'; chmod +x create-boot-mtools.sh; make clean; make; MODEL_BIN=stories110M.bin ./create-boot-mtools.sh"

wsl bash -lc "$wslScript"

if ($LASTEXITCODE -ne 0) {
	throw "WSL build failed with exit code $LASTEXITCODE"
}

Write-Host "`n[OK] Done: llm-baremetal-boot.img" -ForegroundColor Green
