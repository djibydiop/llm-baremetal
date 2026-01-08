# Build + create bootable image using WSL (single entrypoint)
param(
	[ValidateSet('repl')]
	[string]$Target = 'repl',

	# Default unchanged (110M) unless overridden.
	[string]$ModelBin = 'stories110M.bin'
)

$ErrorActionPreference = 'Stop'

Write-Host "`n[Build] Build + Image (WSL)" -ForegroundColor Cyan
Write-Host "  Target: $Target" -ForegroundColor Gray
Write-Host "  Model:  $ModelBin" -ForegroundColor Gray

# Fail fast with a helpful message when weights are not present.
$modelHere = Join-Path $PSScriptRoot $ModelBin
$modelParent = Join-Path (Split-Path $PSScriptRoot -Parent) $ModelBin
if (-not (Test-Path $modelHere) -and -not (Test-Path $modelParent)) {
	Write-Host "" 
	Write-Host "❌ Missing model weights: $ModelBin" -ForegroundColor Red
	Write-Host "Place the model file in this folder or one level up, then re-run." -ForegroundColor Yellow
	Write-Host "Tip: download the model from GitHub Releases (recommended) instead of committing weights." -ForegroundColor Yellow
	throw "Missing model weights: $ModelBin"
}

function ConvertTo-WslPath([string]$winPath) {
	# Normalize to forward slashes first.
	$norm = ($winPath -replace '\\','/')
	try {
		$wsl = (wsl wslpath -a $norm 2>$null)
		if ($wsl) { return $wsl.Trim() }
	} catch {
		# fall through
	}

	# Fallback: C:/foo/bar -> /mnt/c/foo/bar
	if ($norm -match '^([A-Za-z]):/(.*)$') {
		$drive = $Matches[1].ToLowerInvariant()
		$rest = $Matches[2]
		return "/mnt/$drive/$rest"
	}
	throw "Failed to convert path to WSL path: $winPath"
}

$wslRepo = ConvertTo-WslPath $PSScriptRoot

$wslScript = "set -e; cd '$wslRepo'; chmod +x create-boot-mtools.sh; make clean; make repl; MODEL_BIN='$ModelBin' ./create-boot-mtools.sh"

wsl bash -lc "$wslScript"

if ($LASTEXITCODE -ne 0) {
	throw "WSL build failed with exit code $LASTEXITCODE"
}

$img = Get-ChildItem -Path $PSScriptRoot -Filter 'llm-baremetal-boot*.img' -ErrorAction SilentlyContinue |
	Sort-Object LastWriteTime -Descending |
	Select-Object -First 1

if ($img) {
	Write-Host "`n[OK] Done: $($img.Name)" -ForegroundColor Green
} else {
	Write-Host "`n[OK] Done" -ForegroundColor Green
}
