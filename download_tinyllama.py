#!/usr/bin/env python3
"""
Download TinyLlama-1.1B-Chat model properly
Uses HuggingFace Hub with proper authentication and chunked download
"""

import os
from pathlib import Path
from huggingface_hub import hf_hub_download
from tqdm import tqdm

print("=" * 60)
print("TinyLlama-1.1B-Chat Downloader")
print("=" * 60)

# Model info
repo_id = "TinyLlama/TinyLlama-1.1B-Chat-v1.0"
filename = "pytorch_model.bin"
output_file = "tinyllama_pytorch.bin"

print(f"\n📦 Model: {repo_id}")
print(f"📂 File: {filename}")
print(f"💾 Expected size: ~1.1 GB (2.2B parameters in fp16)")

# Check if file already exists and is complete
if Path(output_file).exists():
    size_mb = Path(output_file).stat().st_size / (1024 * 1024)
    if size_mb > 1000:  # Should be > 1GB
        print(f"\n✓ {output_file} already exists ({size_mb:.1f} MB)")
        response = input("Re-download? (y/n): ")
        if response.lower() != 'y':
            print("Skipping download.")
            exit(0)
    else:
        print(f"\n⚠️  Existing file is incomplete ({size_mb:.2f} MB)")
        print("Will re-download...")
        os.remove(output_file)

print("\n🔄 Starting download...")
print("   This will take several minutes depending on your connection")
print("   TinyLlama is ~1.1 GB")

try:
    # Download with progress bar
    downloaded_path = hf_hub_download(
        repo_id=repo_id,
        filename=filename,
        cache_dir="./.cache",
        resume_download=True  # Resume if interrupted
    )
    
    print(f"\n✓ Downloaded to cache: {downloaded_path}")
    
    # Copy to working directory
    import shutil
    print(f"\n📋 Copying to {output_file}...")
    shutil.copy2(downloaded_path, output_file)
    
    # Verify size
    final_size_mb = Path(output_file).stat().st_size / (1024 * 1024)
    print(f"\n✅ Download complete!")
    print(f"   File: {output_file}")
    print(f"   Size: {final_size_mb:.1f} MB")
    
    if final_size_mb < 1000:
        print(f"\n⚠️  WARNING: File seems small ({final_size_mb:.1f} MB)")
        print("   Expected size: ~1100 MB")
        print("   The download may be incomplete")
    else:
        print("\n📋 Next step: Convert to binary format")
        print("   Run: python convert_all.py")
        
except Exception as e:
    print(f"\n❌ Error downloading: {e}")
    print("\n💡 Troubleshooting:")
    print("   1. Check internet connection")
    print("   2. Try with HuggingFace token if needed:")
    print("      huggingface-cli login")
    print("   3. Or download manually from:")
    print(f"      https://huggingface.co/{repo_id}/tree/main")
    exit(1)
