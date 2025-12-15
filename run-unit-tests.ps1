# Script to run all DRC unit tests
# Each test runs in QEMU with a fresh disk image

$tests = @(
    @{Name="Performance Monitoring"; File="test_perf.efi"; Img="test-perf.img"},
    @{Name="Configuration System"; File="test_config.efi"; Img="test-config.img"},
    @{Name="Decision Trace"; File="test_trace.efi"; Img="test-trace.img"},
    @{Name="Auto-Moderation (UAM)"; File="test_uam.efi"; Img="test-uam.img"},
    @{Name="Plausibility Checking (UPE)"; File="test_upe.efi"; Img="test-upe.img"},
    @{Name="Intention & Values (UIV)"; File="test_uiv.efi"; Img="test-uiv.img"}
)

Write-Host "`n╔══════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  DRC v5.1 - Unit Test Runner               ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════╝`n" -ForegroundColor Cyan

foreach ($test in $tests) {
    Write-Host "[$($test.Name)]" -ForegroundColor Yellow
    Write-Host "  Creating test image..." -NoNewline
    
    # Create image in WSL
    wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal/drc && mkfs.vfat -F 32 -C $($test.Img) 10240 2>/dev/null && mmd -i $($test.Img) ::EFI 2>/dev/null && mmd -i $($test.Img) ::EFI/BOOT 2>/dev/null && mcopy -oi $($test.Img) $($test.File) ::EFI/BOOT/BOOTX64.EFI 2>/dev/null" | Out-Null
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host " ✓" -ForegroundColor Green
        Write-Host "  Launching QEMU (press any key to continue to next test)..." -ForegroundColor Gray
        
        # Launch QEMU
        & 'C:\Program Files\qemu\qemu-system-x86_64.exe' `
            -bios C:\Users\djibi\Desktop\baremetal\llm-baremetal\OVMF.fd `
            -drive "file=C:\Users\djibi\Desktop\baremetal\llm-baremetal\drc\$($test.Img),format=raw" `
            -m 256
        
        Write-Host "`n  Test completed.`n" -ForegroundColor Green
    } else {
        Write-Host " ✗ Failed to create image" -ForegroundColor Red
    }
}

Write-Host "`n╔══════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  All Tests Completed!                      ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════╝`n" -ForegroundColor Cyan
