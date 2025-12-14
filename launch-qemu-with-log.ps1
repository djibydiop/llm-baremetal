# Launch QEMU with serial output to file
# This allows monitoring in a separate terminal

$qemuPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$biosPath = "C:\Users\djibi\Desktop\baremetal\llm-baremetal\OVMF.fd"
$imagePath = "C:\Users\djibi\Desktop\baremetal\llm-baremetal\llama2_efi.img"
$serialLog = "qemu-serial-output.txt"

Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║    Launching QEMU with DRC Network Learning (with logging)    ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Stop any existing QEMU
Write-Host "Stopping any existing QEMU instances..." -ForegroundColor Yellow
Get-Process | Where-Object {$_.ProcessName -like "*qemu*"} | Stop-Process -Force 2>$null
Start-Sleep -Seconds 2

# Clear old log
if (Test-Path $serialLog) {
    Remove-Item $serialLog -Force
}

Write-Host "Starting QEMU with serial output to: $serialLog" -ForegroundColor Green
Write-Host ""
Write-Host "GUI window will show graphics output" -ForegroundColor Cyan
Write-Host "Run .\watch-qemu-serial.ps1 to monitor console output" -ForegroundColor Cyan
Write-Host ""

# Launch with serial to file
$arguments = @(
    "-bios", $biosPath,
    "-drive", "file=$imagePath,format=raw",
    "-m", "2048M",
    "-cpu", "qemu64,+sse2",
    "-smp", "2",
    "-serial", "file:$serialLog"
)

Start-Process -FilePath $qemuPath -ArgumentList $arguments -NoNewWindow
Start-Sleep -Seconds 3

Write-Host "✓ QEMU launched!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Watch GUI window for graphics output" -ForegroundColor White
Write-Host "  2. Run: .\watch-qemu-serial.ps1 to see console/DRC logs" -ForegroundColor White
Write-Host ""
