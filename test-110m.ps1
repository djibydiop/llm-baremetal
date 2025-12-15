# Test stories110M.bin - Extended Model
Write-Host "=== DRC v6.0 - stories110M Test ===" -ForegroundColor Cyan
Write-Host "Model: stories110M.bin (418MB, 12 layers)" -ForegroundColor Green

$workDir = "C:\Users\djibi\Desktop\baremetal\llm-baremetal"
Set-Location $workDir

# Update image with latest binary
Write-Host "Deploying latest llama2.efi..." -ForegroundColor Yellow
$mountScript = @"
mkdir -p /tmp/efi_test
mount -o loop qemu-test.img /tmp/efi_test
cp llama2.efi /tmp/efi_test/EFI/BOOT/BOOTX64.EFI
cp stories110M.bin /tmp/efi_test/stories110M.bin 2>/dev/null || echo "stories110M already present"
umount /tmp/efi_test
echo "Image updated"
"@

wsl bash -c $mountScript

Write-Host "Launching QEMU with stories110M..." -ForegroundColor Green

$qemuPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$proc = Start-Process -FilePath $qemuPath `
    -ArgumentList "-bios", "$workDir\OVMF.fd", `
                  "-drive", "format=raw,file=$workDir\qemu-110m-test.img", `
                  "-m", "2048M", `
                  "-nographic" `
    -NoNewWindow `
    -PassThru `
    -RedirectStandardOutput "$workDir\qemu-110m.txt" `
    -RedirectStandardError "$workDir\qemu-110m-err.txt"

Write-Host "QEMU started (PID: $($proc.Id))" -ForegroundColor Green
Write-Host "Waiting 120 seconds for 110M model loading (417MB) and inference..." -ForegroundColor Yellow

Start-Sleep -Seconds 120

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

Write-Host "`n=== stories110M Output ===" -ForegroundColor Cyan
Get-Content "$workDir\qemu-110m.txt" -ErrorAction SilentlyContinue | Select-Object -Last 150

Write-Host "`n=== Test Complete ===" -ForegroundColor Green
