# Build + create bootable image using WSL (single entrypoint)
param(
	[ValidateSet('repl', 'llmkernel')]
	[string]$Target = 'repl',

	# Keep default unchanged (110M) unless overridden.
	[string]$ModelBin = 'stories110M.bin'
)

$ErrorActionPreference = 'Stop'

Write-Host "`n[Build] Build + Image (WSL)" -ForegroundColor Cyan
Write-Host "  Target: $Target" -ForegroundColor Gray
Write-Host "  Model:  $ModelBin" -ForegroundColor Gray

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

$makeTarget = if ($Target -eq 'llmkernel') { 'llmkernel' } else { 'repl' }
$efiEnv = if ($Target -eq 'llmkernel') { 'EFI_BIN=llmkernel.efi' } else { '' }

$wslScript = "set -e; cd '$wslRepo'; chmod +x create-boot-mtools.sh; make clean; make $makeTarget; $efiEnv MODEL_BIN='$ModelBin' ./create-boot-mtools.sh"

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
