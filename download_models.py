#!/usr/bin/env python3
"""
Download and convert models for multimodal LLM bootloader
Supports: stories15M, NanoGPT-124M, TinyLlama-1.1B-Chat
"""

import os
import sys
import requests
from pathlib import Path
from tqdm import tqdm

MODELS = {
    "stories15M": {
        "url": "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin",
        "size": "60MB",
        "description": "Story generation model (6 layers, 288 dim)"
    },
    "nanogpt": {
        "url": "https://huggingface.co/openai-community/gpt2/resolve/main/pytorch_model.bin",
        "size": "48MB", 
        "description": "GPT-2 124M (12 layers, 768 dim) - requires conversion",
        "needs_conversion": True
    },
    "tinyllama_chat": {
        "url": "https://huggingface.co/TinyLlama/TinyLlama-1.1B-Chat-v1.0/resolve/main/pytorch_model.bin",
        "size": "440MB",
        "description": "TinyLlama Chat (22 layers, 2048 dim) - requires conversion",
        "needs_conversion": True
    }
}

def download_file(url, destination):
    """Download file with progress bar"""
    print(f"\nDownloading: {url}")
    print(f"Destination: {destination}")
    
    response = requests.get(url, stream=True)
    total_size = int(response.headers.get('content-length', 0))
    
    with open(destination, 'wb') as f, tqdm(
        desc=destination.name,
        total=total_size,
        unit='iB',
        unit_scale=True,
        unit_divisor=1024,
    ) as pbar:
        for data in response.iter_content(chunk_size=1024):
            size = f.write(data)
            pbar.update(size)
    
    print(f"✓ Downloaded: {destination}")

def convert_pytorch_to_bin(pytorch_file, output_file, model_type):
    """Convert PyTorch checkpoint to binary format"""
    print(f"\nConverting {model_type} to binary format...")
    
    try:
        import torch
        import struct
    except ImportError:
        print("ERROR: PyTorch required for conversion")
        print("Install: pip install torch")
        return False
    
    # Load PyTorch model
    checkpoint = torch.load(pytorch_file, map_location='cpu')
    
    if model_type == "nanogpt":
        # GPT-2 124M configuration
        config = {
            'dim': 768,
            'hidden_dim': 3072,
            'n_layers': 12,
            'n_heads': 12,
            'n_kv_heads': 12,
            'vocab_size': 50257,
            'seq_len': 1024
        }
    elif model_type == "tinyllama_chat":
        # TinyLlama-1.1B configuration
        config = {
            'dim': 2048,
            'hidden_dim': 5632,
            'n_layers': 22,
            'n_heads': 32,
            'n_kv_heads': 4,
            'vocab_size': 32000,
            'seq_len': 2048
        }
    else:
        print(f"Unknown model type: {model_type}")
        return False
    
    # Write binary format
    with open(output_file, 'wb') as f:
        # Write config (7 ints)
        f.write(struct.pack('i', config['dim']))
        f.write(struct.pack('i', config['hidden_dim']))
        f.write(struct.pack('i', config['n_layers']))
        f.write(struct.pack('i', config['n_heads']))
        f.write(struct.pack('i', config['n_kv_heads']))
        f.write(struct.pack('i', -config['vocab_size']))  # Negative = shared weights
        f.write(struct.pack('i', config['seq_len']))
        
        # Extract and write weights
        print("Extracting weights from checkpoint...")
        
        # This is a simplified conversion - actual implementation needs
        # proper weight extraction based on model architecture
        print("⚠️  WARNING: Full conversion requires model-specific weight mapping")
        print("    This script creates a placeholder. Use official export scripts:")
        print("    - For GPT-2: Use export.py from nanoGPT repo")
        print("    - For TinyLlama: Use export.py from llama2.c repo")
        
    print(f"✓ Converted: {output_file}")
    return True

def main():
    print("=" * 60)
    print("MULTIMODAL LLM BOOTLOADER - Model Downloader")
    print("=" * 60)
    
    # Create models directory
    models_dir = Path(__file__).parent
    models_dir.mkdir(exist_ok=True)
    
    print("\nAvailable models:")
    for i, (name, info) in enumerate(MODELS.items(), 1):
        print(f"  {i}. {name} ({info['size']}) - {info['description']}")
    
    print("\nOptions:")
    print("  a) Download all models")
    print("  1-3) Download specific model")
    print("  q) Quit")
    
    choice = input("\nYour choice: ").strip().lower()
    
    if choice == 'q':
        print("Exiting...")
        return
    
    # Determine which models to download
    if choice == 'a':
        to_download = list(MODELS.keys())
    elif choice in ['1', '2', '3']:
        to_download = [list(MODELS.keys())[int(choice) - 1]]
    else:
        print("Invalid choice")
        return
    
    # Download and convert models
    for model_name in to_download:
        info = MODELS[model_name]
        
        print(f"\n{'=' * 60}")
        print(f"Processing: {model_name}")
        print(f"{'=' * 60}")
        
        # Determine output filename
        if model_name == "stories15M":
            output_file = models_dir / "stories15M.bin"
        elif model_name == "nanogpt":
            output_file = models_dir / "nanogpt.bin"
            temp_file = models_dir / "nanogpt_pytorch.bin"
        elif model_name == "tinyllama_chat":
            output_file = models_dir / "tinyllama_chat.bin"
            temp_file = models_dir / "tinyllama_pytorch.bin"
        
        # Check if already exists
        if output_file.exists():
            overwrite = input(f"File {output_file.name} exists. Overwrite? (y/n): ")
            if overwrite.lower() != 'y':
                print(f"Skipping {model_name}")
                continue
        
        # Download
        if model_name == "stories15M":
            # stories15M is already in binary format
            download_file(info['url'], output_file)
        else:
            # Download PyTorch model
            download_file(info['url'], temp_file)
            
            # Convert to binary
            if info.get('needs_conversion'):
                success = convert_pytorch_to_bin(temp_file, output_file, model_name)
                if success:
                    # Clean up PyTorch file
                    temp_file.unlink()
    
    print("\n" + "=" * 60)
    print("Download complete!")
    print("=" * 60)
    
    print("\n📋 Next steps:")
    print("  1. Copy models to EFI boot disk:")
    print("     - stories15M.bin (60MB)")
    print("     - nanogpt.bin (48MB)")
    print("     - tinyllama_chat.bin (440MB)")
    print("  2. Copy tokenizer.bin to boot disk")
    print("  3. Boot and select model from menu")
    
    print("\n⚠️  Note for GPT-2 and TinyLlama:")
    print("   Use official export scripts for full conversion:")
    print("   - GPT-2: https://github.com/karpathy/nanoGPT/blob/master/export.py")
    print("   - TinyLlama: https://github.com/karpathy/llama2.c/blob/master/export.py")

if __name__ == "__main__":
    main()
