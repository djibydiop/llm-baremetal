#!/bin/bash

echo "╔════════════════════════════════════════════╗"
echo "║  🚀 TRAINING STORIES260M                  ║"
echo "╚════════════════════════════════════════════╝"
echo ""

# Check for GPU
if command -v nvidia-smi &> /dev/null; then
    echo "✓ NVIDIA GPU detected"
    nvidia-smi --query-gpu=name,memory.total --format=csv,noheader
    echo ""
else
    echo "⚠️ Warning: No NVIDIA GPU detected"
    echo "Training will be VERY slow on CPU"
    echo "Recommended: Use Google Colab or Kaggle for GPU training"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Check if llama2.c repo exists
if [ ! -d "llama2.c" ]; then
    echo "📥 Cloning llama2.c repository..."
    git clone https://github.com/karpathy/llama2.c.git
fi

cd llama2.c

# Check if TinyStories data exists
if [ ! -f "data/TinyStories_all_data/TinyStories-train.txt" ]; then
    echo "📥 Downloading TinyStories dataset (~500MB)..."
    python tinystories.py download
fi

# Prepare data
if [ ! -f "data/tinystories/train.bin" ]; then
    echo "🔧 Preparing training data..."
    python tinystories.py pretokenize
fi

cd ..

# Create Python venv if needed
if [ ! -d "venv-train" ]; then
    echo "🔧 Creating Python virtual environment..."
    python3 -m venv venv-train
fi

echo "📦 Installing dependencies..."
source venv-train/bin/activate
pip install -q torch numpy tqdm

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║  🎯 STARTING TRAINING                     ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "Model: stories260M"
echo "Parameters: ~260 million"
echo "Architecture:"
echo "  • Embedding dimension: 1024"
echo "  • Layers: 16"
echo "  • Attention heads: 16"
echo "  • Context length: 256"
echo ""
echo "Training settings:"
echo "  • Batch size: 24"
echo "  • Gradient accumulation: 4 steps"
echo "  • Learning rate: 3e-4"
echo "  • Iterations: 150,000"
echo ""
echo "Expected time:"
echo "  • With GPU (A100/V100): ~24-36 hours"
echo "  • With GPU (T4/P100): ~48-72 hours"
echo "  • With CPU: Several days (not recommended)"
echo ""
echo "Output: out-stories260m/ckpt.pt"
echo ""

# Copy prepared data to correct location
if [ ! -d "data/tinystories" ]; then
    mkdir -p data
    cp -r llama2.c/data/tinystories data/
fi

# Run training
python train_stories260m.py

echo ""
echo "╔════════════════════════════════════════════╗"
echo "║  ✅ TRAINING COMPLETE                     ║"
echo "╚════════════════════════════════════════════╝"
echo ""

# Export to .bin format for bare-metal
if [ -f "out-stories260m/ckpt.pt" ]; then
    echo "📦 Exporting model to .bin format..."
    
    # Create export script
    cat > export_stories260m.py << 'EOF'
import torch
import struct
import numpy as np

# Load checkpoint
print("Loading checkpoint...")
checkpoint = torch.load('out-stories260m/ckpt.pt', map_location='cpu')
model_state = checkpoint['model']
config = checkpoint['model_args']

print(f"Model config: dim={config.n_embd}, layers={config.n_layer}, heads={config.n_head}")

# Export to binary format
with open('stories260M.bin', 'wb') as f:
    # Header
    f.write(struct.pack('i', config.n_embd))      # dim
    f.write(struct.pack('i', 4 * config.n_embd))  # hidden_dim
    f.write(struct.pack('i', config.n_layer))     # n_layers
    f.write(struct.pack('i', config.n_head))      # n_heads
    f.write(struct.pack('i', config.n_head))      # n_kv_heads
    f.write(struct.pack('i', config.vocab_size))  # vocab_size
    f.write(struct.pack('i', config.block_size))  # seq_len
    
    # Weights
    def write_fp32(tensor):
        t = tensor.detach().cpu().view(-1).numpy().astype(np.float32)
        f.write(t.tobytes())
    
    # Token embeddings
    write_fp32(model_state['transformer.wte.weight'])
    
    # Layer weights
    for i in range(config.n_layer):
        write_fp32(model_state[f'transformer.h.{i}.ln_1.weight'])
        
        # Attention weights (need to split combined qkv)
        qkv = model_state[f'transformer.h.{i}.attn.c_attn.weight']
        q, k, v = qkv.split(config.n_embd, dim=0)
        write_fp32(q)
        write_fp32(k)
        write_fp32(v)
        write_fp32(model_state[f'transformer.h.{i}.attn.c_proj.weight'])
        
        # FFN weights
        write_fp32(model_state[f'transformer.h.{i}.ln_2.weight'])
        write_fp32(model_state[f'transformer.h.{i}.mlp.c_fc.weight'])
        write_fp32(model_state[f'transformer.h.{i}.mlp.c_proj.weight'])
    
    # Final layer norm
    write_fp32(model_state['transformer.ln_f.weight'])

print("✅ Export complete: stories260M.bin")
print(f"File size: {os.path.getsize('stories260M.bin') / 1024 / 1024:.1f} MB")
EOF
    
    python export_stories260m.py
    
    echo ""
    echo "📊 Model exported successfully!"
    echo ""
    echo "Next steps:"
    echo "  1. Update llama2_efi.c to support stories260M"
    echo "  2. Rebuild: make clean && make"
    echo "  3. Test in QEMU with 512MB+ disk image"
    echo "  4. Create USB boot image (1GB+)"
    echo ""
fi
