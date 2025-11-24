#!/bin/bash
# Prepare TinyStories dataset and train stories101M
# This script will:
# 1. Download TinyStories dataset
# 2. Prepare data for training
# 3. Launch training
# 4. Export to .bin format for bare-metal

set -e

echo "╔════════════════════════════════════════════╗"
echo "║  🚀 TRAINING STORIES101M - Setup          ║"
echo "╚════════════════════════════════════════════╝"

# Check if we're in the right directory
if [ ! -f "llama2_efi.c" ]; then
    echo "❌ Error: Run this script from llm-baremetal directory"
    exit 1
fi

# Check for GPU
if ! command -v nvidia-smi &> /dev/null; then
    echo "⚠️  Warning: No NVIDIA GPU detected"
    echo "   Training will be VERY slow on CPU"
    read -p "   Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    echo "✅ GPU detected:"
    nvidia-smi --query-gpu=name,memory.total --format=csv,noheader
fi

# Clone llama2.c if needed
if [ ! -d "llama2.c" ]; then
    echo "📥 Cloning llama2.c repository..."
    git clone https://github.com/karpathy/llama2.c.git
    echo "✅ Repository cloned"
else
    echo "✅ llama2.c repository exists"
fi

cd llama2.c

# Setup Python environment
echo ""
echo "🐍 Setting up Python environment..."
if [ ! -d "venv" ]; then
    python3 -m venv venv
    echo "✅ Virtual environment created"
fi

source venv/bin/activate

echo "📦 Installing dependencies..."
pip install --quiet --upgrade pip
pip install --quiet torch numpy tiktoken requests tqdm sentencepiece

echo "✅ Dependencies installed"

# Download and prepare TinyStories dataset
echo ""
echo "📚 Preparing TinyStories dataset..."
if [ ! -f "data/tinystories/train.bin" ]; then
    echo "   Downloading TinyStories (~500 MB)..."
    python tinystories.py download
    echo "   Tokenizing dataset..."
    python tinystories.py pretokenize
    echo "✅ Dataset prepared"
else
    echo "✅ Dataset already prepared"
fi

# Copy our training script
echo ""
echo "📋 Preparing training script..."
cp ../train_stories101m.py .

# Show training info
echo ""
echo "╔════════════════════════════════════════════╗"
echo "║  📊 TRAINING CONFIGURATION                ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "Model Architecture:"
echo "  • Parameters: ~101 million"
echo "  • Layers: 12 (vs 6 in stories15M)"
echo "  • Heads: 12 (vs 6 in stories15M)"
echo "  • Embedding: 768 (vs 288 in stories15M)"
echo "  • Vocab: 32,000 tokens"
echo ""
echo "Training Setup:"
echo "  • Dataset: TinyStories (complete)"
echo "  • Iterations: 100,000"
echo "  • Batch size: 32"
echo "  • Learning rate: 3e-4 → 3e-5"
echo "  • Expected time: 8-12 hours (GPU)"
echo ""
echo "Output:"
echo "  • Checkpoint: out-stories101m/ckpt.pt"
echo "  • Binary: stories101M.bin (~350 MB)"
echo ""

read -p "🚀 Ready to start training? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Training cancelled"
    exit 0
fi

# Start training
echo ""
echo "╔════════════════════════════════════════════╗"
echo "║  🎓 STARTING TRAINING                     ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "📝 Training log will be saved to: training.log"
echo "💡 You can monitor with: tail -f training.log"
echo ""

# Run training with logging
python train_stories101m.py 2>&1 | tee training.log

# Check if training succeeded
if [ $? -eq 0 ]; then
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║  ✅ TRAINING COMPLETE!                    ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    
    # Export to binary
    echo "🔧 Exporting to binary format..."
    
    # Create export script if needed
    cat > export_stories101m.py << 'EXPORT_EOF'
import struct
import sys
import torch
from pathlib import Path

def export(checkpoint_path, output_path):
    print(f"Loading checkpoint from {checkpoint_path}")
    checkpoint = torch.load(checkpoint_path, map_location='cpu')
    model = checkpoint['model']
    config = checkpoint['model_args']
    
    print(f"Exporting to {output_path}")
    
    with open(output_path, 'wb') as f:
        # Write config
        f.write(struct.pack('i', config['n_embd']))
        f.write(struct.pack('i', config['n_layer']))
        f.write(struct.pack('i', config['n_head']))
        f.write(struct.pack('i', config.get('n_kv_heads', config['n_head'])))
        f.write(struct.pack('i', config['vocab_size']))
        f.write(struct.pack('i', config['block_size']))
        
        # Write weights
        for k, v in model.items():
            if v.dtype != torch.float32:
                v = v.float()
            t = v.numpy().tobytes()
            f.write(t)
    
    print(f"✅ Export complete! Size: {Path(output_path).stat().st_size / 1024 / 1024:.1f} MB")

if __name__ == '__main__':
    export('out-stories101m/ckpt.pt', '../stories101M.bin')
EXPORT_EOF
    
    python export_stories101m.py
    
    cd ..
    
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║  🎉 STORIES101M READY!                    ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    echo "📦 Model file: stories101M.bin"
    ls -lh stories101M.bin 2>/dev/null || echo "   (will be created after export)"
    echo ""
    echo "🔧 Next steps:"
    echo "  1. Update llama2_efi.c to load stories101M"
    echo "  2. Recompile: make clean && make"
    echo "  3. Test: ./test-qemu.ps1"
    echo ""
    echo "💡 Expected improvements:"
    echo "  • Much more coherent stories"
    echo "  • Better grammar and structure"
    echo "  • Richer vocabulary"
    echo "  • More creative content"
    echo ""
else
    echo ""
    echo "❌ Training failed! Check training.log for errors"
    exit 1
fi
