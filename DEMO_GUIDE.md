# 🎬 Demo Recording Guide

## Quick Start

### Windows
```powershell
# Full demo (3 minutes)
.\record-demo.ps1

# Short demo (30 seconds)
.\record-demo.ps1 -Duration 30 -OutputFile "demo-quick.mp4"

# Custom duration (5 minutes)
.\record-demo.ps1 -Duration 300 -OutputFile "demo-full.mp4"
```

### Linux/macOS
```bash
# Full demo (3 minutes)
./record-demo.sh

# Short demo (30 seconds)
./record-demo.sh 30 demo-quick.mp4

# Custom duration (5 minutes)
./record-demo.sh 300 demo-full.mp4
```

## Prerequisites

### Install Recording Tools

#### Windows
```powershell
# Install ffmpeg
winget install ffmpeg

# Install asciinema (optional, for best quality)
pip install asciinema

# Install agg (optional, for video conversion)
cargo install agg
```

#### Linux/macOS
```bash
# Ubuntu/Debian
sudo apt-get install ffmpeg asciinema

# macOS
brew install ffmpeg asciinema

# agg (Rust-based converter)
cargo install agg
```

## Recording Methods

### Method 1: Asciinema (Recommended) ⭐

**Best for**: Terminal recordings, text accuracy, file size

```bash
# Install
pip install asciinema

# Record
asciinema rec demo.cast -c "./test-qemu.sh"

# Play locally
asciinema play demo.cast

# Upload & share
asciinema upload demo.cast
# Get shareable link: https://asciinema.org/a/xxxxx
```

**Advantages**:
- ✅ Perfect text rendering
- ✅ Small file size (~500KB for 3min)
- ✅ Can embed in websites
- ✅ Easy to share (asciinema.org)

### Method 2: Video Conversion

**Best for**: YouTube, social media, presentations

```bash
# Convert asciinema to GIF
agg demo.cast demo.gif

# Convert asciinema to MP4
agg demo.cast demo.mp4

# Or use asciicast2gif
asciicast2gif demo.cast demo.gif
```

**Advantages**:
- ✅ Works everywhere (YouTube, Twitter, etc.)
- ✅ No special player needed
- ✅ Can add audio commentary

### Method 3: Screen Recording

**Best for**: Live commentary, mouse interactions

**Windows**:
- Xbox Game Bar (Win+G)
- OBS Studio (free, professional)
- ShareX (lightweight)

**macOS**:
- QuickTime Player (Cmd+Shift+5)
- OBS Studio

**Linux**:
- SimpleScreenRecorder
- OBS Studio
- Kazam

## Demo Script Suggestions

### 30-Second Quick Demo
```
1. Boot (5s) - UEFI logo → Model loading
2. Menu (5s) - Show 6 categories
3. Generation (20s) - 1-2 prompts with output
```

### 3-Minute Full Demo
```
1. Boot sequence (30s)
   - UEFI initialization
   - Model loading progress (427MB)
   - Tokenizer loading

2. Category showcase (2min)
   - Stories: 1-2 prompts
   - Science: 1 prompt
   - Adventure: 1 prompt
   - Philosophy: 1 prompt (NEW!)
   - Technology: 1 prompt (NEW!)

3. Technical highlights (30s)
   - Show prompt encoding
   - Display generation speed
   - Mention optimizations (AVX2)
```

### 5-Minute Extended Demo
```
Full demo + hardware deployment demonstration
- Show USB deployment script
- Explain hardware requirements
- Compare QEMU vs real hardware performance
```

## Output Files

After recording, you'll find in `demo-recordings/`:

```
demo-recordings/
├── qemu-demo-TIMESTAMP.txt       # Raw text output
├── demo-TIMESTAMP.cast           # Asciinema recording
├── demo-llm-baremetal.mp4        # Video (if converted)
├── demo-viewer-TIMESTAMP.html    # Interactive HTML viewer
└── README.md                     # Recording metadata
```

## Sharing Your Demo

### 1. Asciinema.org (Easiest)
```bash
asciinema upload demo-recordings/demo-*.cast
# Copy the URL and share!
```

**Embed in GitHub README**:
```markdown
[![asciicast](https://asciinema.org/a/xxxxx.svg)](https://asciinema.org/a/xxxxx)
```

### 2. YouTube

1. Convert to MP4: `agg demo.cast demo.mp4`
2. Upload to YouTube
3. Add title: "LLM Bare-Metal: 110M params running on UEFI firmware"
4. Add description with GitHub link

**Suggested Tags**:
- Machine Learning
- Bare Metal
- UEFI
- System Programming
- LLM
- Embedded AI

### 3. Twitter/X

```
🚀 LLM running on BARE METAL! 

✨ 110M parameter model
🔥 No OS - just UEFI firmware
⚡ AVX2 optimized
📝 41 prompts, 6 categories

[Attach video/GIF]

GitHub: github.com/djibydiop/llm-baremetal
#AI #SystemsProgramming #BareMetal
```

### 4. LinkedIn

```
Excited to share my latest project: Bare-Metal LLM Inference!

Running a 110M parameter language model directly on UEFI firmware - no operating system required.

Key features:
• Complete transformer implementation in C
• BPE tokenization (32000 vocab)
• AVX2 optimizations
• Interactive menu with 41 prompts across 6 categories
• Boots on real hardware via USB

This demonstrates the possibility of AI inference in resource-constrained environments like embedded systems, IoT devices, and bootloaders.

Tech stack: C, UEFI, AVX2, GNU-EFI, QEMU

[Attach video]

#MachineLearning #SystemsProgramming #AI #EmbeddedSystems
GitHub: github.com/djibydiop/llm-baremetal
```

### 5. GitHub README

Add to your README.md:

```markdown
## 🎬 Demo

[![Demo Video](https://img.shields.io/badge/Demo-Watch-red?logo=youtube)](YOUR_YOUTUBE_LINK)
[![asciicast](https://asciinema.org/a/xxxxx.svg)](https://asciinema.org/a/xxxxx)

Watch the LLM boot and generate text in real-time!
```

### 6. Dev.to / Hashnode Blog Post

Write a blog post with:
- Embedded video/asciinema
- Technical deep-dive
- Code snippets
- Performance metrics
- Hardware boot guide

## Tips for Best Demo

### 1. Clean Terminal
```bash
clear
# Ensure terminal is 80x24 or larger
resize -s 24 80
```

### 2. Slow Down Output (Optional)
For presentations, you might want to slow down text:
```bash
# Add delays in script
echo "Loading model..."
sleep 1
```

### 3. Highlight Key Moments
Add visual markers in your recording:
```
=== ATTENTION: Model Loading ===
=== ATTENTION: First Generation ===
```

### 4. Add Captions (Post-Production)
- Use video editor to add text overlays
- Explain technical terms
- Highlight impressive moments

## Troubleshooting

### Recording stops too early
```bash
# Increase duration
./record-demo.sh 300  # 5 minutes
```

### No video output
```bash
# Install missing tools
pip install asciinema
cargo install agg
```

### Poor video quality
```bash
# Use asciinema for text-based recording
# Better rendering than screen capture
```

### File too large
```bash
# Asciinema cast files are tiny (~500KB)
# MP4 compression:
ffmpeg -i input.mp4 -vcodec libx264 -crf 28 output.mp4
```

## Next Steps

1. Record your demo: `./record-demo.ps1` or `./record-demo.sh`
2. Review output in `demo-recordings/`
3. Convert to desired format (MP4, GIF)
4. Share on your preferred platform
5. Add link to GitHub README

## Examples from Community

*Add your demo links here!*

- [Your Name] - [YouTube/Asciinema Link]

## Questions?

Open an issue on GitHub: https://github.com/djibydiop/llm-baremetal/issues

---

**Ready to record?** Run `./record-demo.ps1` (Windows) or `./record-demo.sh` (Linux/macOS)! 🎥
