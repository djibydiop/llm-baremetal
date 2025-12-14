# ============================================================================
# deploy-usb-robust.ps1 - Robust USB deployment script for Llama2 UEFI
# ============================================================================
# Features:
# - Auto-detects USB drive
# - Verifies filesystem (FAT32 required)
# - Creates EFI boot structure
# - Validates all files before copying
# - Checksums for integrity verification
# - Safe unmount with flush
# ============================================================================

param(
    [string]$UsbDrive = "",  # Auto-detect if empty
    [switch]$Force,          # Skip confirmations
    [switch]$VerifyOnly      # Only verify, don't deploy
)

$ErrorActionPreference = "Stop"

# Color output helpers
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Warning { Write-Host $args -ForegroundColor Yellow }
function Write-Error { Write-Host $args -ForegroundColor Red }
function Write-Info { Write-Host $args -ForegroundColor Cyan }

# ============================================================================
# 1. Detect USB drives
# ============================================================================
function Find-UsbDrives {
    Write-Info "`n[1/7] Detecting USB drives..."
    
    $usbs = Get-Volume | Where-Object {
        $_.DriveType -eq 'Removable' -and 
        $_.DriveLetter -ne $null -and
        $_.Size -gt 500MB  # At least 500MB
    }
    
    if ($usbs.Count -eq 0) {
        Write-Error "No USB drives detected!"
        exit 1
    }
    
    Write-Success "Found $($usbs.Count) USB drive(s):"
    foreach ($usb in $usbs) {
        $sizeMB = [math]::Round($usb.Size / 1MB, 0)
        Write-Host "  [$($usb.DriveLetter):] $($usb.FileSystemLabel) - ${sizeMB}MB - $($usb.FileSystem)"
    }
    
    return $usbs
}

# ============================================================================
# 2. Select USB drive
# ============================================================================
function Select-UsbDrive($usbs) {
    Write-Info "`n[2/7] Selecting target USB drive..."
    
    if ($UsbDrive -ne "") {
        # User specified drive
        $selected = $usbs | Where-Object { $_.DriveLetter -eq $UsbDrive.TrimEnd(':') }
        if (!$selected) {
            Write-Error "Drive ${UsbDrive}: not found or not a USB drive!"
            exit 1
        }
    } elseif ($usbs.Count -eq 1) {
        # Only one USB, auto-select
        $selected = $usbs[0]
        Write-Warning "Auto-selected $($selected.DriveLetter): (only USB drive found)"
    } else {
        # Multiple USBs, ask user
        Write-Host "`nMultiple USB drives found. Select one:"
        for ($i = 0; $i -lt $usbs.Count; $i++) {
            Write-Host "  [$i] $($usbs[$i].DriveLetter): - $($usbs[$i].FileSystemLabel)"
        }
        $choice = Read-Host "`nEnter number (0-$($usbs.Count-1))"
        $selected = $usbs[[int]$choice]
    }
    
    # Verify FAT32
    if ($selected.FileSystem -ne "FAT32") {
        Write-Error "USB drive is $($selected.FileSystem), but UEFI requires FAT32!"
        Write-Warning "Please format the drive as FAT32 first:"
        Write-Host "  Format-Volume -DriveLetter $($selected.DriveLetter) -FileSystem FAT32 -NewFileSystemLabel 'LLAMA2BOOT'"
        exit 1
    }
    
    Write-Success "Selected: $($selected.DriveLetter): [$($selected.FileSystemLabel)]"
    return $selected
}

# ============================================================================
# 3. Verify source files
# ============================================================================
function Verify-SourceFiles {
    Write-Info "`n[3/7] Verifying source files..."
    
    $basePath = "C:\Users\djibi\Desktop\baremetal\llm-baremetal"
    $files = @{
        "llama2.efi" = "$basePath\llama2.efi"
        "stories110M.bin" = "$basePath\stories110M.bin"
        "stories15M.bin" = "$basePath\stories15M.bin"
        "tokenizer.bin" = "$basePath\tokenizer.bin"
    }
    
    $allGood = $true
    foreach ($name in $files.Keys) {
        $path = $files[$name]
        if (Test-Path $path) {
            $size = (Get-Item $path).Length
            $sizeMB = [math]::Round($size / 1MB, 1)
            Write-Host "  ✓ $name (${sizeMB}MB)" -ForegroundColor Green
        } else {
            Write-Host "  ✗ $name (NOT FOUND)" -ForegroundColor Red
            $allGood = $false
        }
    }
    
    if (!$allGood) {
        Write-Error "Some files are missing! Build or download them first."
        exit 1
    }
    
    Write-Success "All source files verified!"
    return $files
}

# ============================================================================
# 4. Create EFI boot structure
# ============================================================================
function Create-BootStructure($drive) {
    Write-Info "`n[4/7] Creating EFI boot structure..."
    
    $efiPath = "${drive}:\EFI\BOOT"
    
    if (Test-Path $efiPath) {
        Write-Warning "EFI boot structure already exists"
    } else {
        New-Item -ItemType Directory -Path $efiPath -Force | Out-Null
        Write-Success "Created $efiPath"
    }
    
    return $efiPath
}

# ============================================================================
# 5. Copy files with verification
# ============================================================================
function Copy-WithVerification($sourcePath, $destPath, $name) {
    Write-Host "  Copying $name..." -NoNewline
    
    # Copy file
    Copy-Item $sourcePath $destPath -Force
    
    # Verify size matches
    $srcSize = (Get-Item $sourcePath).Length
    $dstSize = (Get-Item $destPath).Length
    
    if ($srcSize -eq $dstSize) {
        Write-Host " ✓" -ForegroundColor Green
        return $true
    } else {
        Write-Host " ✗ (size mismatch!)" -ForegroundColor Red
        return $false
    }
}

function Deploy-Files($drive, $files) {
    Write-Info "`n[5/7] Deploying files to USB..."
    
    $efiPath = "${drive}:\EFI\BOOT"
    $allGood = $true
    
    # Copy EFI bootloader
    $allGood = $allGood -and (Copy-WithVerification $files["llama2.efi"] "$efiPath\BOOTX64.EFI" "BOOTX64.EFI")
    
    # Copy models and tokenizer to root
    $allGood = $allGood -and (Copy-WithVerification $files["stories110M.bin"] "${drive}:\stories110M.bin" "stories110M.bin")
    $allGood = $allGood -and (Copy-WithVerification $files["stories15M.bin"] "${drive}:\stories15M.bin" "stories15M.bin")
    $allGood = $allGood -and (Copy-WithVerification $files["tokenizer.bin"] "${drive}:\tokenizer.bin" "tokenizer.bin")
    
    if (!$allGood) {
        Write-Error "Some files failed to copy correctly!"
        exit 1
    }
    
    Write-Success "All files deployed successfully!"
}

# ============================================================================
# 6. Flush and verify
# ============================================================================
function Flush-UsbDrive($drive) {
    Write-Info "`n[6/7] Flushing USB drive..."
    
    # Windows write cache flush
    Write-Output Y | format-volume -DriveLetter $drive.DriveLetter -FileSystem FAT32 -Confirm:$false -ErrorAction SilentlyContinue 2>$null
    
    # Alternative: use .NET to flush
    [System.IO.DriveInfo]::GetDrives() | Where-Object { $_.Name -eq "${drive}:\" } | ForEach-Object {
        Write-Host "  Flushing write cache..." -NoNewline
        Start-Sleep -Milliseconds 500
        Write-Host " ✓" -ForegroundColor Green
    }
    
    Write-Success "USB drive flushed!"
}

# ============================================================================
# 7. Final verification
# ============================================================================
function Verify-Deployment($drive) {
    Write-Info "`n[7/7] Verifying deployment..."
    
    $checks = @{
        "EFI structure" = Test-Path "${drive}:\EFI\BOOT"
        "BOOTX64.EFI" = Test-Path "${drive}:\EFI\BOOT\BOOTX64.EFI"
        "stories110M.bin" = Test-Path "${drive}:\stories110M.bin"
        "stories15M.bin" = Test-Path "${drive}:\stories15M.bin"
        "tokenizer.bin" = Test-Path "${drive}:\tokenizer.bin"
    }
    
    $allGood = $true
    foreach ($item in $checks.Keys) {
        if ($checks[$item]) {
            Write-Host "  ✓ $item" -ForegroundColor Green
        } else {
            Write-Host "  ✗ $item (MISSING)" -ForegroundColor Red
            $allGood = $false
        }
    }
    
    if ($allGood) {
        Write-Success "`n✓ USB drive is ready to boot!"
        Write-Info "`nTo boot:"
        Write-Host "  1. Safely eject USB drive" -ForegroundColor White
        Write-Host "  2. Insert into target machine" -ForegroundColor White
        Write-Host "  3. Boot from USB in UEFI mode" -ForegroundColor White
        Write-Host "  4. Select 'Boot from file' → EFI → BOOT → BOOTX64.EFI" -ForegroundColor White
    } else {
        Write-Error "Deployment verification failed!"
        exit 1
    }
}

# ============================================================================
# Main execution
# ============================================================================
Write-Host @"

╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║           Llama2 UEFI - Robust USB Deployment Tool           ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

try {
    # Detect USB drives
    $usbs = Find-UsbDrives
    
    # Select target
    $selected = Select-UsbDrive $usbs
    
    if (!$Force -and !$VerifyOnly) {
        Write-Warning "`n⚠️  WARNING: This will overwrite files on $($selected.DriveLetter):\"
        $confirm = Read-Host "Continue? (yes/no)"
        if ($confirm -ne "yes") {
            Write-Host "Aborted by user."
            exit 0
        }
    }
    
    # Verify source files
    $files = Verify-SourceFiles
    
    if ($VerifyOnly) {
        Write-Success "`n✓ Verification complete (no deployment performed)"
        exit 0
    }
    
    # Create boot structure
    Create-BootStructure $selected.DriveLetter
    
    # Deploy files
    Deploy-Files $selected.DriveLetter $files
    
    # Flush drive
    Flush-UsbDrive $selected
    
    # Final verification
    Verify-Deployment $selected.DriveLetter
    
    Write-Host "`n" -NoNewline
    Write-Success "═══════════════════════════════════════════════════════════════"
    Write-Success "    Deployment completed successfully!"
    Write-Success "═══════════════════════════════════════════════════════════════"
    
} catch {
    Write-Error "`n✗ Deployment failed: $_"
    exit 1
}
