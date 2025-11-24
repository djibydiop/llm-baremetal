#!/bin/bash
# Record QEMU Demo for LLM Bare-Metal
# Creates a video recording of QEMU boot and LLM inference

set -e

DURATION=${1:-180}  # 3 minutes default
OUTPUT_FILE=${2:-"demo-llm-baremetal.mp4"}

echo "=== LLM Bare-Metal Demo Recorder ==="
echo ""

# Check for dependencies
check_command() {
    if command -v "$1" &> /dev/null; then
        echo "[✓] $1 found"
        return 0
    else
        echo "[✗] $1 not found"
        return 1
    fi
}

echo "Checking dependencies..."
HAS_ASCIINEMA=false
HAS_AGG=false
HAS_FFMPEG=false

check_command asciinema && HAS_ASCIINEMA=true
check_command agg && HAS_AGG=true
check_command ffmpeg && HAS_FFMPEG=true

echo ""

if [ "$HAS_ASCIINEMA" = false ]; then
    echo "[WARNING] asciinema not found"
    echo "Install: pip install asciinema"
    echo "         or: apt-get install asciinema"
fi

if [ "$HAS_AGG" = false ]; then
    echo "[WARNING] agg not found (for video conversion)"
    echo "Install: cargo install agg"
fi

echo ""

# Build system
echo "[1/4] Building system..."
make clean && make && make disk
echo "  ✓ Build complete"

# Create output directory
OUTPUT_DIR="demo-recordings"
mkdir -p "$OUTPUT_DIR"

TIMESTAMP=$(date +%Y%m%d-%H%M%S)
TEXT_OUTPUT="$OUTPUT_DIR/qemu-demo-$TIMESTAMP.txt"
VIDEO_OUTPUT="$OUTPUT_DIR/$OUTPUT_FILE"
CAST_FILE="$OUTPUT_DIR/demo-$TIMESTAMP.cast"

echo ""
echo "[2/4] Starting QEMU recording..."
echo "  Duration: $DURATION seconds"
echo "  Output: $VIDEO_OUTPUT"
echo "  Text log: $TEXT_OUTPUT"
echo ""

if [ "$HAS_ASCIINEMA" = true ]; then
    echo "[INFO] Recording with asciinema (high quality)..."
    asciinema rec --overwrite \
        -t "LLM Bare-Metal Demo" \
        "$CAST_FILE" \
        -c "timeout $DURATION qemu-system-x86_64 \
            -bios /usr/share/ovmf/OVMF.fd \
            -drive file=qemu-test.img,format=raw \
            -m 4G \
            -cpu Haswell \
            -nographic 2>&1 | tee $TEXT_OUTPUT"
    
    echo ""
    echo "[3/4] Converting to video..."
    
    if [ "$HAS_AGG" = true ]; then
        agg "$CAST_FILE" "$VIDEO_OUTPUT"
        echo "  ✓ Video created with agg"
    else
        echo "[INFO] Asciinema recording saved as: $CAST_FILE"
        echo "       Play with: asciinema play $CAST_FILE"
        echo "       Upload to: asciinema.org"
        
        # Try to create GIF with asciicast2gif if available
        if command -v asciicast2gif &> /dev/null; then
            GIF_OUTPUT="$OUTPUT_DIR/demo-$TIMESTAMP.gif"
            asciicast2gif -w 100 -h 40 "$CAST_FILE" "$GIF_OUTPUT"
            echo "  ✓ GIF created: $GIF_OUTPUT"
        fi
    fi
else
    echo "[INFO] Recording terminal output to text (no asciinema)..."
    timeout $DURATION qemu-system-x86_64 \
        -bios /usr/share/ovmf/OVMF.fd \
        -drive file=qemu-test.img,format=raw \
        -m 4G \
        -cpu Haswell \
        -nographic 2>&1 | tee "$TEXT_OUTPUT"
    
    echo "  ✓ Terminal output saved"
fi

echo ""
echo "[4/4] Creating demo assets..."

# Create HTML viewer
HTML_OUTPUT="$OUTPUT_DIR/demo-viewer-$TIMESTAMP.html"
cat > "$HTML_OUTPUT" << 'EOF'
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
            fetch('qemu-demo-*.txt')
                .then(response => response.text())
                .then(text => {
                    document.getElementById('output').textContent = text;
                })
                .catch(err => {
                    document.getElementById('output').textContent = 'Error loading output';
                });
        </script>
    </div>
</body>
</html>
EOF

echo "  ✓ HTML viewer created: $HTML_OUTPUT"

# Create README
README_OUTPUT="$OUTPUT_DIR/README.md"
cat > "$README_OUTPUT" << EOF
# LLM Bare-Metal Demo Recording

Recorded: $(date '+%Y-%m-%d %H:%M:%S')

## Files

- **$TEXT_OUTPUT** - Raw terminal output
- **$HTML_OUTPUT** - HTML viewer
- **$CAST_FILE** - Asciinema recording (if available)
- **$VIDEO_OUTPUT** - Video (if converted)

## System Info

- **Model**: stories110M (110M parameters)
- **Architecture**: 12L/768D/12H
- **Tokenizer**: 32000 BPE tokens
- **Prompts**: 41 across 6 categories
- **Optimizations**: AVX2 + loop unrolling

## Viewing

### Terminal Output
\`\`\`bash
cat $TEXT_OUTPUT
# or open $HTML_OUTPUT in browser
\`\`\`

### Asciinema
\`\`\`bash
asciinema play $CAST_FILE
# or upload to asciinema.org
asciinema upload $CAST_FILE
\`\`\`

### Video
\`\`\`bash
# Play with any video player
vlc $VIDEO_OUTPUT
mpv $VIDEO_OUTPUT
\`\`\`

## Share

- YouTube: Upload $VIDEO_OUTPUT
- Twitter/X: Share recording
- asciinema.org: Upload .cast file
- GitHub: Embed in README

## GitHub

https://github.com/djibydiop/llm-baremetal
EOF

echo "  ✓ Demo README created: $README_OUTPUT"

echo ""
echo "=== Demo Recording Complete ==="
echo ""
echo "📁 Demo files in: $OUTPUT_DIR/"
echo ""
echo "📺 View recording:"
echo "   Browser: $HTML_OUTPUT"
[ -f "$CAST_FILE" ] && echo "   Asciinema: asciinema play $CAST_FILE"
[ -f "$VIDEO_OUTPUT" ] && echo "   Video: $VIDEO_OUTPUT"
echo "   Text: $TEXT_OUTPUT"
echo ""
echo "🎬 Share options:"
echo "   - asciinema.org: asciinema upload $CAST_FILE"
echo "   - YouTube: upload video"
echo "   - GitHub: embed in README"
echo ""
echo "✨ Quick view: open $HTML_OUTPUT"
