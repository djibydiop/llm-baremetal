#!/bin/bash
# Train and prepare models for multimodal bootloader

set -e

echo "================================================================"
echo "MULTIMODAL LLM BOOTLOADER - Model Training & Preparation"
echo "================================================================"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check dependencies
echo -e "\n${YELLOW}Checking dependencies...${NC}"
MISSING_DEPS=()

if ! command_exists python3; then
    MISSING_DEPS+=("python3")
fi

if ! command_exists pip3; then
    MISSING_DEPS+=("pip3")
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${RED}Missing dependencies: ${MISSING_DEPS[*]}${NC}"
    echo "Install with: sudo apt install python3 python3-pip"
    exit 1
fi

echo -e "${GREEN}✓ All dependencies installed${NC}"

# Create virtual environment
echo -e "\n${YELLOW}Setting up Python environment...${NC}"
if [ ! -d "venv" ]; then
    python3 -m venv venv
    echo -e "${GREEN}✓ Virtual environment created${NC}"
else
    echo -e "${GREEN}✓ Virtual environment exists${NC}"
fi

# Activate venv
source venv/bin/activate

# Install Python packages
echo -e "\n${YELLOW}Installing Python packages...${NC}"
pip install --upgrade pip
pip install torch numpy requests tqdm sentencepiece

echo -e "${GREEN}✓ Packages installed${NC}"

# Model preparation options
echo -e "\n================================================================"
echo "Model Preparation Options:"
echo "================================================================"
echo "1. Download pre-trained stories15M (60MB) ✓ Ready to use"
echo "2. Download pre-trained NanoGPT-124M (48MB) - Requires conversion"
echo "3. Download pre-trained TinyLlama-Chat (440MB) - Requires conversion"
echo "4. Train custom stories15M from scratch (~1 hour on GPU)"
echo "5. Fine-tune NanoGPT for bootloader (~3 hours on GPU)"
echo "6. Download all pre-trained models"
echo ""

read -p "Select option (1-6): " option

case $option in
    1)
        echo -e "\n${YELLOW}Downloading stories15M...${NC}"
        wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin
        wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin
        echo -e "${GREEN}✓ stories15M ready!${NC}"
        ;;
    2)
        echo -e "\n${YELLOW}Downloading NanoGPT-124M...${NC}"
        echo "Cloning nanoGPT repo for export script..."
        if [ ! -d "nanoGPT" ]; then
            git clone https://github.com/karpathy/nanoGPT.git
        fi
        cd nanoGPT
        python3 train.py --device=cpu --compile=False --eval_only=True --init_from=gpt2
        python3 export.py gpt2 --output=../nanogpt.bin
        cd ..
        echo -e "${GREEN}✓ NanoGPT converted to nanogpt.bin${NC}"
        ;;
    3)
        echo -e "\n${YELLOW}Downloading TinyLlama-Chat...${NC}"
        echo "Cloning llama2.c repo for export script..."
        if [ ! -d "llama2.c" ]; then
            git clone https://github.com/karpathy/llama2.c.git
        fi
        cd llama2.c
        python3 export.py TinyLlama/TinyLlama-1.1B-Chat-v1.0 --output=../tinyllama_chat.bin
        cd ..
        echo -e "${GREEN}✓ TinyLlama-Chat converted to tinyllama_chat.bin${NC}"
        ;;
    4)
        echo -e "\n${YELLOW}Training custom stories15M...${NC}"
        if [ ! -d "llama2.c" ]; then
            git clone https://github.com/karpathy/llama2.c.git
        fi
        cd llama2.c
        
        # Download TinyStories dataset
        python3 tinystories.py download
        python3 tinystories.py pretokenize
        
        # Train model
        python3 train.py \
            --vocab_size=32000 \
            --dim=288 \
            --n_layers=6 \
            --n_heads=6 \
            --n_kv_heads=6 \
            --multiple_of=32 \
            --max_seq_len=256 \
            --batch_size=128 \
            --learning_rate=1e-3 \
            --max_iters=100000
        
        # Export to binary
        python3 export.py stories15M.pt --output=../stories15M_custom.bin
        cd ..
        echo -e "${GREEN}✓ Custom stories15M trained!${NC}"
        ;;
    5)
        echo -e "\n${YELLOW}Fine-tuning NanoGPT for bootloader...${NC}"
        if [ ! -d "nanoGPT" ]; then
            git clone https://github.com/karpathy/nanoGPT.git
        fi
        cd nanoGPT
        
        # Prepare bootloader-specific dataset
        cat > prepare_bootloader.py << 'EOF'
import os
import numpy as np

# Create bootloader conversation dataset
conversations = [
    "User: What is UEFI? Assistant: UEFI is Unified Extensible Firmware Interface.",
    "User: How do I boot from USB? Assistant: Enter BIOS, change boot order.",
    "User: What is a bootloader? Assistant: Software that loads the operating system.",
    # Add more conversations...
]

with open('bootloader_train.txt', 'w') as f:
    f.write('\n'.join(conversations))

os.system('python3 prepare.py')
EOF
        
        python3 prepare_bootloader.py
        
        # Fine-tune
        python3 train.py \
            --init_from=gpt2 \
            --dataset=bootloader \
            --n_layer=12 \
            --n_head=12 \
            --n_embd=768 \
            --batch_size=12 \
            --learning_rate=3e-4 \
            --max_iters=10000
        
        python3 export.py ckpt.pt --output=../nanogpt_bootloader.bin
        cd ..
        echo -e "${GREEN}✓ NanoGPT fine-tuned!${NC}"
        ;;
    6)
        echo -e "\n${YELLOW}Downloading all pre-trained models...${NC}"
        python3 download_models.py
        echo -e "${GREEN}✓ All models downloaded!${NC}"
        ;;
    *)
        echo -e "${RED}Invalid option${NC}"
        exit 1
        ;;
esac

# Copy to bootloader directory
echo -e "\n${YELLOW}Copying models to bootloader directory...${NC}"
for model in stories15M.bin nanogpt.bin tinyllama_chat.bin; do
    if [ -f "$model" ]; then
        cp "$model" .
        echo -e "${GREEN}✓ Copied $model${NC}"
    fi
done

# Rebuild disk image
echo -e "\n${YELLOW}Rebuilding bootloader disk image...${NC}"
make clean
make
make llama2-disk

echo -e "\n================================================================"
echo -e "${GREEN}✓ Model preparation complete!${NC}"
echo "================================================================"
echo ""
echo "Available models in current directory:"
ls -lh *.bin 2>/dev/null || echo "No .bin files found"
echo ""
echo "📋 Next steps:"
echo "  1. Test in QEMU: make test-qemu"
echo "  2. Write to USB: sudo dd if=llama2-disk.img of=/dev/sdX bs=4M"
echo "  3. Boot from USB on real hardware"
echo ""
echo "🎯 Bootloader will auto-detect available models!"
