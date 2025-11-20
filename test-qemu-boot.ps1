#!/usr/bin/env pwsh
# Test QEMU Boot - Simplified
# Tests if we can boot the EFI at all (without waiting for full inference)

Write-Host "🎬 QEMU Boot Test for LLM Bare-Metal" -ForegroundColor Cyan
Write-Host "=" * 60

# Check prerequisites
$qemuPath = "qemu-system-x86_64"
$ovmfPath = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"

Write-Host "`n📋 Checking prerequisites..." -ForegroundColor Yellow

# Check QEMU
try {
    $qemuVersion = & $qemuPath --version 2>&1 | Select-Object -First 1
    Write-Host "✅ QEMU found: $qemuVersion" -ForegroundColor Green
} catch {
    Write-Host "❌ QEMU not found. Install from: https://www.qemu.org/download/" -ForegroundColor Red
    exit 1
}

# Check OVMF
if (Test-Path $ovmfPath) {
    Write-Host "✅ OVMF firmware found: $ovmfPath" -ForegroundColor Green
} else {
    Write-Host "⚠️  Default OVMF not found, trying alternative paths..." -ForegroundColor Yellow
    
    # Try WSL path
    $wslOvmf = wsl wslpath -w "/usr/share/OVMF/OVMF_CODE.fd" 2>$null
    if ($wslOvmf -and (Test-Path $wslOvmf)) {
        $ovmfPath = $wslOvmf
        Write-Host "✅ OVMF found in WSL: $ovmfPath" -ForegroundColor Green
    } else {
        Write-Host "❌ OVMF firmware not found." -ForegroundColor Red
        Write-Host "   Install: sudo apt install ovmf (in WSL)" -ForegroundColor Yellow
        exit 1
    }
}

# Check disk image
if (Test-Path "llm-disk.img") {
    $diskSize = (Get-Item "llm-disk.img").Length / 1MB
    Write-Host "✅ Disk image found: llm-disk.img ($($diskSize)MB)" -ForegroundColor Green
} else {
    Write-Host "❌ Disk image not found. Run: wsl make" -ForegroundColor Red
    exit 1
}

Write-Host "`n🚀 Launching QEMU..." -ForegroundColor Cyan
Write-Host "=" * 60

Write-Host @"

📝 Instructions:
   1. QEMU window will open
   2. Wait for UEFI boot (~10-30 seconds)
   3. EFI Shell should appear
   4. Our chatbot.efi should auto-run
   5. Watch for text generation!

⚠️  Known Issues:
   - Boot can be slow (be patient)
   - May timeout (UEFI is slow in VM)
   - Generation takes ~50-100ms per token
   - Press Ctrl+C to stop if hung

🎥 Recording Tips:
   - Use OBS Studio or Windows Game Bar (Win+G)
   - Record from the moment QEMU window opens
   - Show the full boot process
   - Even if it fails, it shows the attempt!

"@ -ForegroundColor Yellow

Write-Host "`nStarting in 3 seconds..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

# QEMU arguments
$qemuArgs = @(
    "-bios", $ovmfPath,                          # UEFI firmware
    "-drive", "format=raw,file=llm-disk.img",    # Our disk image
    "-m", "256M",                                 # 256MB RAM (enough for our model)
    "-serial", "stdio",                           # Serial output to console
    "-vga", "std",                                # Standard VGA
    "-display", "gtk"                             # GTK window (better than SDL)
)

Write-Host "`n🎮 QEMU Command:" -ForegroundColor Cyan
Write-Host "$qemuPath $($qemuArgs -join ' ')" -ForegroundColor Gray

Write-Host "`n🔥 Launching..." -ForegroundColor Green
Write-Host "=" * 60

try {
    # Launch QEMU (will open new window)
    & $qemuPath @qemuArgs
    
    Write-Host "`n✅ QEMU exited normally" -ForegroundColor Green
} catch {
    Write-Host "`n❌ QEMU launch failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host "`n📊 Post-Test Info:" -ForegroundColor Cyan
Write-Host "=" * 60

Write-Host @"

If boot worked:
✅ You should have seen UEFI logo
✅ EFI Shell prompt appeared
✅ chatbot.efi started running
✅ Text generation appeared (even if slow/incomplete)

If boot failed:
❌ Hung at UEFI logo → Increase timeout or try real hardware
❌ No output → Check serial console (-serial stdio)
❌ Black screen → VGA issue, try -vga cirrus
❌ Crash → EFI binary might be corrupted, rebuild with 'wsl make'

Next steps:
1. If worked: Record screencast with OBS
2. If failed: Try simplified "Hello World" EFI first
3. Alternative: Boot from USB on real hardware

"@ -ForegroundColor Yellow

Write-Host "`n💡 Tip: For screencast, use:" -ForegroundColor Cyan
Write-Host "   OBS Studio (Free): https://obsproject.com/" -ForegroundColor Gray
Write-Host "   Windows Game Bar: Win+G → Record" -ForegroundColor Gray
Write-Host "   ShareX: https://getsharex.com/" -ForegroundColor Gray
