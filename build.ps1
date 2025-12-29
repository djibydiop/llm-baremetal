# Build + create bootable image using WSL (single entrypoint)
$ErrorActionPreference = 'Stop'

Write-Host "`n🔧 Build + Image (WSL)" -ForegroundColor Cyan

$winRepo = ($PSScriptRoot -replace '\\','/')
$wslRepo = (wsl wslpath -a $winRepo 2>$null)
if (-not $wslRepo) { throw "Failed to convert path to WSL path via wslpath: $PSScriptRoot" }
$wslRepo = $wslRepo.Trim()
$wslScript = "cd '$wslRepo' && chmod +x build-image-wsl.sh create-boot-mtools.sh && ./build-image-wsl.sh"

wsl bash -lc $wslScript

Write-Host "`n✅ Done: llm-baremetal-boot.img" -ForegroundColor Green
