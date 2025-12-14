#!/usr/bin/env python3
"""
Download and convert LLaMA 3.2-1B-Instruct to bare-metal format.
This script handles the complete pipeline:
1. Download from Hugging Face
2. Convert to bare-metal format with INT4 quantization
3. Validate the output
"""

import sys
import os
from pathlib import Path

print("=" * 70)
print("🚀 LLaMA 3.2-1B CONVERSION TO BARE-METAL FORMAT")
print("=" * 70)
print()

# Step 1: Check if model is already downloaded
model_name = "meta-llama/Llama-3.2-1B-Instruct"
output_file = "llama3_1b.bin"

print("📥 Step 1: Checking/Downloading LLaMA 3.2-1B-Instruct...")
print(f"   Model: {model_name}")
print(f"   Size: ~5 GB (FP32)")
print()

try:
    from transformers import LlamaForCausalLM, LlamaTokenizer
    from huggingface_hub import snapshot_download
    import torch
    
    # Try to load model (will download if needed)
    print("⏳ Loading model (this may take 10-30 minutes on first run)...")
    print("   Downloading from Hugging Face Hub...")
    print()
    
    model = LlamaForCausalLM.from_pretrained(
        model_name,
        torch_dtype=torch.float32,
        device_map="cpu",
        low_cpu_mem_usage=True
    )
    
    print("✅ Model loaded successfully!")
    print(f"   Parameters: {sum(p.numel() for p in model.parameters()) / 1e9:.2f}B")
    print(f"   Config: {model.config}")
    print()
    
    # Display model architecture
    config = model.config
    print("📊 Model Architecture:")
    print(f"   - dim: {config.hidden_size}")
    print(f"   - hidden_dim: {config.intermediate_size}")
    print(f"   - n_layers: {config.num_hidden_layers}")
    print(f"   - n_heads: {config.num_attention_heads}")
    print(f"   - n_kv_heads: {config.num_key_value_heads} ← GQA!")
    print(f"   - vocab_size: {config.vocab_size}")
    print(f"   - max_seq_len: {config.max_position_embeddings}")
    print(f"   - rope_theta: {getattr(config, 'rope_theta', 500000.0)}")
    print()
    
    # Calculate memory requirements
    total_params = sum(p.numel() for p in model.parameters())
    fp32_size = total_params * 4 / (1024**3)  # GB
    int4_size = total_params * 0.5 / (1024**3)  # GB (4 bits = 0.5 bytes)
    
    print("💾 Memory Requirements:")
    print(f"   - FP32: {fp32_size:.2f} GB")
    print(f"   - INT4: {int4_size:.2f} GB (8x compression)")
    print()
    
    # Step 2: Convert to bare-metal format
    print("🔄 Step 2: Converting to bare-metal format with INT4 quantization...")
    print()
    
    # Import conversion script
    sys.path.insert(0, 'scripts')
    from convert_llama3_to_baremetal import LLaMA3Converter
    
    converter = LLaMA3Converter(model_name, quantize="int4", group_size=32)
    converter.convert(output_file)
    
    print()
    print("✅ Conversion complete!")
    
    # Step 3: Validate output
    print()
    print("🔍 Step 3: Validating output file...")
    print()
    
    if os.path.exists(output_file):
        size_mb = os.path.getsize(output_file) / (1024 * 1024)
        print(f"✅ Output file: {output_file}")
        print(f"   Size: {size_mb:.2f} MB")
        print(f"   Expected: ~{int4_size * 1024:.0f} MB")
        
        # Validate format with our test script
        print()
        print("📊 Validating binary format...")
        os.system(f"python test_llama3_support.py {output_file}")
    else:
        print(f"❌ Error: Output file not created: {output_file}")
        sys.exit(1)
    
    print()
    print("=" * 70)
    print("🎉 SUCCESS - LLaMA 3.2-1B READY FOR BARE-METAL!")
    print("=" * 70)
    print()
    print("Next steps:")
    print("  1. Copy to qemu-test: cp llama3_1b.bin qemu-test/")
    print("  2. Test format: python test_llama3_support.py llama3_1b.bin")
    print("  3. Boot in QEMU or USB")
    print()
    
except ImportError as e:
    print(f"❌ Error: Missing dependency: {e}")
    print()
    print("Please install required packages:")
    print("  pip install -r requirements-conversion.txt")
    sys.exit(1)
    
except Exception as e:
    print(f"❌ Error during conversion: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
