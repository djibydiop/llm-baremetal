#!/usr/bin/env python3
"""
Batch convert all downloaded PyTorch models
Loads PyTorch once and converts all models sequentially
"""

import sys
import time
from pathlib import Path

print("=" * 60)
print("Batch Model Converter")
print("=" * 60)

# Check if PyTorch models exist
models_to_convert = []

if Path("nanogpt_pytorch.bin").exists():
    models_to_convert.append(("nanogpt", "nanogpt_pytorch.bin", "nanogpt.bin"))
    print("✓ Found nanogpt_pytorch.bin (522 MB)")

if Path("tinyllama_pytorch.bin").exists() and Path("tinyllama_pytorch.bin").stat().st_size > 1_000_000:
    models_to_convert.append(("tinyllama_chat", "tinyllama_pytorch.bin", "tinyllama_chat.bin"))
    print("✓ Found tinyllama_pytorch.bin")

if not models_to_convert:
    print("\n❌ No PyTorch models found to convert")
    print("   Run: python download_models.py")
    sys.exit(1)

print(f"\n📦 Models to convert: {len(models_to_convert)}")
for model_type, input_file, output_file in models_to_convert:
    print(f"   - {model_type}: {input_file} → {output_file}")

input("\n⏳ Press Enter to start (PyTorch loading takes ~30 seconds)...")

# Import convert_models functions
print("\n🔄 Loading PyTorch (this takes time)...")
start_time = time.time()

from convert_models import export_nanogpt_124m, export_tinyllama_chat

load_time = time.time() - start_time
print(f"✓ PyTorch loaded in {load_time:.1f} seconds")

# Convert each model
print("\n" + "=" * 60)
print("Starting conversions...")
print("=" * 60)

for model_type, input_file, output_file in models_to_convert:
    print(f"\n📝 Converting {model_type}...")
    
    try:
        if model_type == "nanogpt":
            export_nanogpt_124m(input_file, output_file)
        elif model_type == "tinyllama_chat":
            export_tinyllama_chat(input_file, output_file)
        
        print(f"✅ {model_type} converted successfully!")
        
    except Exception as e:
        print(f"❌ Error converting {model_type}: {e}")
        continue

print("\n" + "=" * 60)
print("🎉 Batch conversion complete!")
print("=" * 60)

# Show results
print("\n📊 Results:")
for model_type, input_file, output_file in models_to_convert:
    output_path = Path(output_file)
    if output_path.exists():
        size_mb = output_path.stat().st_size / (1024 * 1024)
        print(f"   ✓ {output_file}: {size_mb:.1f} MB")
    else:
        print(f"   ✗ {output_file}: Not created")

print("\n📋 Next steps:")
print("   1. wsl make llama2-disk")
print("   2. wsl make run")
print("   3. Test model selection menu")
