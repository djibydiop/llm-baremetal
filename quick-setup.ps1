#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Quick Setup Script for LLM Bare-Metal Integration

.DESCRIPTION
    Automates the setup process for integrating LLM Bare-Metal v5.0
    into any project. Handles dependencies, compilation, and testing.

.PARAMETER Action
    Setup action: install, build, test, deploy, clean

.EXAMPLE
    .\quick-setup.ps1 -Action install
    .\quick-setup.ps1 -Action build
    .\quick-setup.ps1 -Action deploy -Target D:

.NOTES
    Author: djibydiop
    Version: 5.0.0
#>

param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("install", "build", "test", "deploy", "clean", "all")]
    [string]$Action,
    
    [Parameter(Mandatory=$false)]
    [string]$Target = ""
)

# Colors
$ErrorColor = "Red"
$SuccessColor = "Green"
$InfoColor = "Cyan"
$WarningColor = "Yellow"

function Write-Step {
    param([string]$Message)
    Write-Host "`n[STEP] $Message" -ForegroundColor $InfoColor
}

function Write-Success {
    param([string]$Message)
    Write-Host "✓ $Message" -ForegroundColor $SuccessColor
}

function Write-Fail {
    param([string]$Message)
    Write-Host "✗ $Message" -ForegroundColor $ErrorColor
}

function Write-Warn {
    param([string]$Message)
    Write-Host "⚠ $Message" -ForegroundColor $WarningColor
}

function Test-WSL {
    Write-Step "Checking WSL installation..."
    $wsl = Get-Command wsl -ErrorAction SilentlyContinue
    if ($wsl) {
        Write-Success "WSL installed"
        return $true
    } else {
        Write-Fail "WSL not found"
        Write-Host "Install WSL: wsl --install" -ForegroundColor $WarningColor
        return $false
    }
}

function Install-Dependencies {
    Write-Step "Installing dependencies in WSL..."
    
    $commands = @(
        "sudo apt update",
        "sudo apt install -y build-essential",
        "sudo apt install -y gnu-efi",
        "sudo apt install -y qemu-system-x86"
    )
    
    foreach ($cmd in $commands) {
        Write-Host "Running: $cmd" -ForegroundColor Gray
        wsl bash -c $cmd
        if ($LASTEXITCODE -eq 0) {
            Write-Success "Command succeeded"
        } else {
            Write-Fail "Command failed: $cmd"
            return $false
        }
    }
    
    Write-Success "All dependencies installed"
    return $true
}

function Build-Project {
    Write-Step "Building LLM Bare-Metal..."
    
    # Check if Makefile exists
    if (-not (Test-Path "Makefile")) {
        Write-Fail "Makefile not found in current directory"
        return $false
    }
    
    # Clean previous build
    Write-Host "Cleaning previous build..." -ForegroundColor Gray
    wsl bash -c "make clean"
    
    # Build
    Write-Host "Compiling..." -ForegroundColor Gray
    wsl bash -c "make"
    
    if ($LASTEXITCODE -eq 0) {
        # Check output
        if (Test-Path "llama2.efi") {
            $size = (Get-Item "llama2.efi").Length
            Write-Success "Build successful! Binary size: $([math]::Round($size/1KB, 2)) KB"
            return $true
        } else {
            Write-Fail "Build completed but llama2.efi not found"
            return $false
        }
    } else {
        Write-Fail "Build failed"
        return $false
    }
}

function Test-Build {
    Write-Step "Testing build..."
    
    if (-not (Test-Path "llama2.efi")) {
        Write-Fail "llama2.efi not found. Build first."
        return $false
    }
    
    # Check required files
    $requiredFiles = @("llama2.efi", "stories110M.bin", "tokenizer.bin")
    $allPresent = $true
    
    foreach ($file in $requiredFiles) {
        if (Test-Path $file) {
            $size = (Get-Item $file).Length
            Write-Success "$file present ($([math]::Round($size/1MB, 2)) MB)"
        } else {
            Write-Fail "$file missing"
            $allPresent = $false
        }
    }
    
    if ($allPresent) {
        Write-Success "All required files present"
        
        # Test in QEMU (optional)
        Write-Host "`nTest in QEMU? (y/n): " -NoNewline -ForegroundColor $InfoColor
        $response = Read-Host
        if ($response -eq "y") {
            Write-Step "Starting QEMU test..."
            Write-Warn "Press Ctrl+C to exit QEMU"
            Start-Sleep -Seconds 2
            wsl bash -c "qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=fat:rw:. -m 2048 -serial stdio"
        }
        
        return $true
    } else {
        return $false
    }
}

function Deploy-ToUSB {
    param([string]$DriveLetter)
    
    Write-Step "Deploying to USB drive $DriveLetter..."
    
    # Validate drive
    if (-not (Test-Path "${DriveLetter}:\")) {
        Write-Fail "Drive ${DriveLetter}: not found"
        Write-Host "Available drives:" -ForegroundColor Gray
        Get-PSDrive -PSProvider FileSystem | Where-Object { $_.Used -gt 0 } | Select-Object Name, @{N="Size (GB)";E={[math]::Round($_.Used/1GB + $_.Free/1GB, 2)}} | Format-Table
        return $false
    }
    
    # Create EFI structure
    $efiPath = "${DriveLetter}:\EFI\BOOT"
    Write-Host "Creating directory structure..." -ForegroundColor Gray
    New-Item -ItemType Directory -Path $efiPath -Force | Out-Null
    
    # Copy files
    Write-Host "Copying llama2.efi -> BOOTX64.EFI..." -ForegroundColor Gray
    Copy-Item "llama2.efi" -Destination "${efiPath}\BOOTX64.EFI" -Force
    
    Write-Host "Copying models..." -ForegroundColor Gray
    Copy-Item "stories110M.bin" -Destination "${DriveLetter}:\" -Force
    Copy-Item "tokenizer.bin" -Destination "${DriveLetter}:\" -Force
    
    # Verify
    $bootFile = "${efiPath}\BOOTX64.EFI"
    if (Test-Path $bootFile) {
        $size = (Get-Item $bootFile).Length
        Write-Success "Deployment successful!"
        Write-Host "  BOOTX64.EFI: $([math]::Round($size/1KB, 2)) KB" -ForegroundColor Gray
        Write-Host "  stories110M.bin: Present" -ForegroundColor Gray
        Write-Host "  tokenizer.bin: Present" -ForegroundColor Gray
        Write-Host "`nUSB is ready to boot!" -ForegroundColor $SuccessColor
        return $true
    } else {
        Write-Fail "Deployment failed"
        return $false
    }
}

function Clean-Build {
    Write-Step "Cleaning build artifacts..."
    
    if (Test-Path "Makefile") {
        wsl bash -c "make clean"
        Write-Success "Build cleaned"
    }
    
    # Remove any temp files
    $tempFiles = @("*.o", "*.so", "*.efi")
    foreach ($pattern in $tempFiles) {
        Get-ChildItem -Filter $pattern | Remove-Item -Force
    }
    
    Write-Success "Cleanup complete"
}

function Show-Banner {
    Write-Host @"

╔═══════════════════════════════════════════════════════════╗
║         LLM BARE-METAL v5.0 - QUICK SETUP                 ║
║         Universal Integration Tool                         ║
╚═══════════════════════════════════════════════════════════╝

"@ -ForegroundColor Magenta
}

# Main execution
Show-Banner

switch ($Action) {
    "install" {
        if (Test-WSL) {
            Install-Dependencies
        }
    }
    
    "build" {
        if (Test-WSL) {
            Build-Project
        }
    }
    
    "test" {
        Test-Build
    }
    
    "deploy" {
        if (-not $Target) {
            Write-Host "Enter USB drive letter (e.g., D): " -NoNewline -ForegroundColor $InfoColor
            $Target = Read-Host
        }
        Deploy-ToUSB -DriveLetter $Target
    }
    
    "clean" {
        Clean-Build
    }
    
    "all" {
        Write-Step "Running complete setup..."
        
        if (-not (Test-WSL)) {
            Write-Fail "WSL required but not found"
            exit 1
        }
        
        if (-not (Install-Dependencies)) {
            Write-Fail "Dependency installation failed"
            exit 1
        }
        
        if (-not (Build-Project)) {
            Write-Fail "Build failed"
            exit 1
        }
        
        if (-not (Test-Build)) {
            Write-Fail "Testing failed"
            exit 1
        }
        
        Write-Host "`nDeploy to USB? (y/n): " -NoNewline -ForegroundColor $InfoColor
        $response = Read-Host
        if ($response -eq "y") {
            Write-Host "Enter USB drive letter (e.g., D): " -NoNewline -ForegroundColor $InfoColor
            $drive = Read-Host
            Deploy-ToUSB -DriveLetter $drive
        }
        
        Write-Host "`n╔═══════════════════════════════════════╗" -ForegroundColor $SuccessColor
        Write-Host "║   SETUP COMPLETE!                     ║" -ForegroundColor $SuccessColor
        Write-Host "╚═══════════════════════════════════════╝`n" -ForegroundColor $SuccessColor
    }
}

Write-Host "`nSetup script finished.`n" -ForegroundColor $InfoColor
