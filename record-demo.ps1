#!/usr/bin/env pwsh
# Record QEMU Demo for LLM Bare-Metal
# Creates a video recording of QEMU boot and LLM inference

param(
    [int]$Duration = 180,  # 3 minutes by default
    [string]$OutputFile = "demo-llm-baremetal.mp4"
)

$ErrorActionPreference = "Stop"

Write-Host "=== LLM Bare-Metal Demo Recorder ===" -ForegroundColor Cyan
Write-Host ""

# Check if ffmpeg is installed
$ffmpegPath = Get-Command ffmpeg -ErrorAction SilentlyContinue
if (-not $ffmpegPath) {
    Write-Host "[ERROR] ffmpeg not found!" -ForegroundColor Red
    Write-Host "Install with: winget install ffmpeg" -ForegroundColor Yellow
    Write-Host "Or download from: https://ffmpeg.org/download.html" -ForegroundColor Yellow
    exit 1
}

Write-Host "[INFO] ffmpeg found: $($ffmpegPath.Source)" -ForegroundColor Green

# Check if asciinema is installed (for terminal recording)
$asciinemaPath = Get-Command asciinema -ErrorAction SilentlyContinue
if ($asciinemaPath) {
    Write-Host "[INFO] asciinema found - will record terminal" -ForegroundColor Green
    $useAsciinema = $true
} else {
    Write-Host "[WARNING] asciinema not found - using simple capture" -ForegroundColor Yellow
    Write-Host "Install with: pip install asciinema" -ForegroundColor Yellow
    $useAsciinema = $false
}

# Ensure system is built
Write-Host ""
Write-Host "[1/4] Building system..." -ForegroundColor Cyan
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && make clean && make && make disk"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit 1
}
Write-Host "  ✓ Build complete" -ForegroundColor Green

# Create output directory
$outputDir = "demo-recordings"
New-Item -Path $outputDir -ItemType Directory -Force | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$textOutput = "$outputDir/qemu-demo-$timestamp.txt"
$videoOutput = "$outputDir/$OutputFile"

Write-Host ""
Write-Host "[2/4] Starting QEMU recording..." -ForegroundColor Cyan
Write-Host "  Duration: $Duration seconds" -ForegroundColor White
Write-Host "  Output: $videoOutput" -ForegroundColor White
Write-Host "  Text log: $textOutput" -ForegroundColor White

# Method 1: Using asciinema (best quality)
if ($useAsciinema) {
    Write-Host ""
    Write-Host "[INFO] Recording with asciinema..." -ForegroundColor Cyan
    $asciinemaFile = "$outputDir/demo-$timestamp.cast"
    
    # Start QEMU in WSL and record with asciinema
    wsl bash -c @"
cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal
asciinema rec --overwrite -t 'LLM Bare-Metal Demo' $asciinemaFile -c "timeout $Duration qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=qemu-test.img,format=raw -m 4G -cpu Haswell -nographic 2>&1 | tee $textOutput"
"@
    
    Write-Host ""
    Write-Host "[3/4] Converting to video..." -ForegroundColor Cyan
    # Convert asciinema to gif/mp4 using agg or asciicast2gif
    if (Get-Command agg -ErrorAction SilentlyContinue) {
        wsl bash -c "agg $asciinemaFile $videoOutput"
        Write-Host "  ✓ Video created with agg" -ForegroundColor Green
    } else {
        Write-Host "[WARNING] agg not found. Install with: cargo install agg" -ForegroundColor Yellow
        Write-Host "[INFO] Asciinema recording saved as: $asciinemaFile" -ForegroundColor Cyan
        Write-Host "       Play with: asciinema play $asciinemaFile" -ForegroundColor Cyan
    }
} else {
    # Method 2: Simple text capture + convert to video
    Write-Host ""
    Write-Host "[INFO] Recording terminal output to text..." -ForegroundColor Cyan
    
    wsl bash -c @"
cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal
timeout $Duration qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=qemu-test.img,format=raw -m 4G -cpu Haswell -nographic 2>&1 | tee $textOutput
"@
    
    Write-Host "  ✓ Terminal output saved" -ForegroundColor Green
}

Write-Host ""
Write-Host "[4/4] Creating demo assets..." -ForegroundColor Cyan

# Create a simple HTML viewer for the demo
$htmlOutput = "$outputDir/demo-viewer-$timestamp.html"
$htmlContent = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>LLM Bare-Metal Demo</title>
    <style>
        body {
            background: #1e1e1e;
            color: #d4d4d4;
            font-family: 'Consolas', 'Monaco', monospace;
            padding: 20px;
            line-height: 1.6;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        h1 {
            color: #4ec9b0;
            border-bottom: 2px solid #4ec9b0;
            padding-bottom: 10px;
        }
        .output {
            background: #252526;
            border: 1px solid #3e3e42;
            border-radius: 8px;
            padding: 20px;
            white-space: pre-wrap;
            font-size: 14px;
            overflow-x: auto;
            max-height: 600px;
            overflow-y: auto;
        }
        .highlight {
            color: #ce9178;
        }
        .success {
            color: #4ec9b0;
        }
        .info {
            color: #569cd6;
        }
        .stats {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin: 20px 0;
        }
        .stat-card {
            background: #252526;
            border: 1px solid #3e3e42;
            border-radius: 8px;
            padding: 15px;
        }
        .stat-label {
            color: #858585;
            font-size: 12px;
        }
        .stat-value {
            color: #4ec9b0;
            font-size: 24px;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 LLM Bare-Metal UEFI Demo</h1>
        
        <div class="stats">
            <div class="stat-card">
                <div class="stat-label">Model</div>
                <div class="stat-value">stories110M</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Parameters</div>
                <div class="stat-value">110M</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Prompts</div>
                <div class="stat-value">41</div>
            </div>
            <div class="stat-card">
                <div class="stat-label">Categories</div>
                <div class="stat-value">6</div>
            </div>
        </div>
        
        <h2>Terminal Output</h2>
        <div class="output" id="output">Loading...</div>
        
        <script>
            fetch('$textOutput')
                .then(response => response.text())
                .then(text => {
                    document.getElementById('output').textContent = text;
                    
                    // Colorize special keywords
                    let html = text
                        .replace(/(\[SUCCESS\])/g, '<span class="success">$1</span>')
                        .replace(/(\[INFO\])/g, '<span class="info">$1</span>')
                        .replace(/(\[ERROR\])/g, '<span style="color:#f48771">$1</span>')
                        .replace(/(stories110M|tokenizer|prompt|category)/gi, '<span class="highlight">$0</span>');
                    
                    document.getElementById('output').innerHTML = html;
                })
                .catch(err => {
                    document.getElementById('output').textContent = 'Error loading output: ' + err;
                });
        </script>
    </div>
</body>
</html>
"@

Set-Content -Path $htmlOutput -Value $htmlContent -Encoding UTF8
Write-Host "  ✓ HTML viewer created: $htmlOutput" -ForegroundColor Green

# Create README for demo
$demoReadme = "$outputDir/README.md"
$readmeContent = @"
# LLM Bare-Metal Demo Recording

Recorded: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")

## Files

- **$textOutput** - Raw terminal output
- **$htmlOutput** - HTML viewer for terminal output
- **$videoOutput** - Video recording (if available)

## System Info

- **Model**: stories110M (110M parameters)
- **Architecture**: 12 layers, 768 dimensions, 12 attention heads
- **Tokenizer**: 32000 BPE tokens
- **Prompts**: 41 across 6 categories
- **Optimizations**: AVX2 + loop unrolling

## Categories

1. **Stories** (7 prompts) - Fairy tales, dragons, princesses
2. **Science** (7 prompts) - Physics, biology, astronomy
3. **Adventure** (7 prompts) - Knights, explorers, pirates
4. **Philosophy** (5 prompts) - Life, wisdom, happiness
5. **History** (5 prompts) - Civilizations, inventions
6. **Technology** (5 prompts) - Computers, AI, robots

## Viewing

### Terminal Output
Open **$htmlOutput** in a browser

### Asciinema (if recorded)
\`\`\`bash
asciinema play demo-$timestamp.cast
\`\`\`

### Video (if converted)
Open **$videoOutput** in any video player

## GitHub

https://github.com/djibydiop/llm-baremetal
"@

Set-Content -Path $demoReadme -Value $readmeContent -Encoding UTF8
Write-Host "  ✓ Demo README created: $demoReadme" -ForegroundColor Green

Write-Host ""
Write-Host "=== Demo Recording Complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "📁 Demo files saved in: $outputDir/" -ForegroundColor Cyan
Write-Host ""
Write-Host "📺 View recording:" -ForegroundColor Yellow
Write-Host "   Browser: $htmlOutput" -ForegroundColor White
if (Test-Path $videoOutput) {
    Write-Host "   Video: $videoOutput" -ForegroundColor White
}
Write-Host "   Text: $textOutput" -ForegroundColor White
Write-Host ""
Write-Host "🎬 Share your demo on:" -ForegroundColor Yellow
Write-Host "   - YouTube (upload $videoOutput)" -ForegroundColor White
Write-Host "   - Twitter/X (screen recording)" -ForegroundColor White
Write-Host "   - LinkedIn (professional demo)" -ForegroundColor White
Write-Host "   - GitHub README (embed video)" -ForegroundColor White
Write-Host ""
Write-Host "✨ Tip: Open $htmlOutput for an interactive view!" -ForegroundColor Cyan
