#!/usr/bin/env python3
"""
Convert PyTorch models to bare-metal binary format
Based on llama2.c export.py by Andrej Karpathy
"""

import struct
import argparse
import torch
from pathlib import Path

def serialize_fp32(file, tensor):
    """Write float32 tensor to file"""
    d = tensor.detach().cpu().view(-1).to(torch.float32).numpy()
    b = struct.pack(f'{len(d)}f', *d)
    file.write(b)

def export_nanogpt_124m(pytorch_file, output_file):
    """
    Export GPT-2 124M to binary format
    Config: 12 layers, 768 dim, 50257 vocab
    """
    print(f"Loading GPT-2 model from {pytorch_file}...")
    
    # Load PyTorch checkpoint
    checkpoint = torch.load(pytorch_file, map_location='cpu')
    
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
    
    print(f"Configuration:")
    for k, v in config.items():
        print(f"  {k}: {v}")
    
    # Get model state dict
    if 'model' in checkpoint:
        state_dict = checkpoint['model']
    else:
        state_dict = checkpoint
    
    print(f"\nWriting binary to {output_file}...")
    
    with open(output_file, 'wb') as f:
        # Write header (7 ints)
        f.write(struct.pack('i', config['dim']))
        f.write(struct.pack('i', config['hidden_dim']))
        f.write(struct.pack('i', config['n_layers']))
        f.write(struct.pack('i', config['n_heads']))
        f.write(struct.pack('i', config['n_kv_heads']))
        f.write(struct.pack('i', -config['vocab_size']))  # Negative = shared weights
        f.write(struct.pack('i', config['seq_len']))
        
        # Write token embedding weights
        print("Writing token embeddings...")
        if 'wte.weight' in state_dict:
            serialize_fp32(f, state_dict['wte.weight'])
        elif 'transformer.wte.weight' in state_dict:
            serialize_fp32(f, state_dict['transformer.wte.weight'])
        else:
            print("WARNING: Token embedding not found")
        
        # Write layer weights
        for layer_idx in range(config['n_layers']):
            print(f"Writing layer {layer_idx + 1}/{config['n_layers']}...")
            
            # Attention weights
            # RMS norm
            key = f'h.{layer_idx}.ln_1.weight'
            if key in state_dict:
                serialize_fp32(f, state_dict[key])
            
            # Q, K, V projection
            key = f'h.{layer_idx}.attn.c_attn.weight'
            if key in state_dict:
                # GPT-2 has fused QKV
                qkv = state_dict[key]
                serialize_fp32(f, qkv)
            
            # Output projection
            key = f'h.{layer_idx}.attn.c_proj.weight'
            if key in state_dict:
                serialize_fp32(f, state_dict[key])
            
            # FFN weights
            # RMS norm
            key = f'h.{layer_idx}.ln_2.weight'
            if key in state_dict:
                serialize_fp32(f, state_dict[key])
            
            # Gate, Up, Down projections
            key = f'h.{layer_idx}.mlp.c_fc.weight'
            if key in state_dict:
                serialize_fp32(f, state_dict[key])
            
            key = f'h.{layer_idx}.mlp.c_proj.weight'
            if key in state_dict:
                serialize_fp32(f, state_dict[key])
        
        # Final layer norm
        if 'ln_f.weight' in state_dict:
            serialize_fp32(f, state_dict['ln_f.weight'])
        
        # RoPE frequencies (optional)
        print("Writing RoPE frequencies...")
        head_dim = config['dim'] // config['n_heads']
        freq_cis_real = torch.zeros(config['seq_len'], head_dim // 2)
        freq_cis_imag = torch.zeros(config['seq_len'], head_dim // 2)
        serialize_fp32(f, freq_cis_real)
        serialize_fp32(f, freq_cis_imag)
    
    file_size = Path(output_file).stat().st_size
    print(f"\n✓ Export complete: {output_file}")
    print(f"  File size: {file_size / (1024*1024):.2f} MB")
    print(f"  Estimated parameters: {file_size / 4 / 1_000_000:.1f}M")

def export_tinyllama_chat(pytorch_file, output_file):
    """
    Export TinyLlama-1.1B-Chat to binary format
    Config: 22 layers, 2048 dim, 32000 vocab
    Supports both PyTorch (.bin/.pt) and SafeTensors (.safetensors) formats
    """
    print(f"Loading TinyLlama model from {pytorch_file}...")
    
    # Load checkpoint - support both PyTorch and SafeTensors
    if pytorch_file.endswith('.safetensors'):
        from safetensors import safe_open
        state_dict = {}
        with safe_open(pytorch_file, framework="pt", device="cpu") as f:
            for key in f.keys():
                state_dict[key] = f.get_tensor(key)
        checkpoint = None
    else:
        checkpoint = torch.load(pytorch_file, map_location='cpu')
    
    # TinyLlama configuration
    config = {
        'dim': 2048,
        'hidden_dim': 5632,
        'n_layers': 22,
        'n_heads': 32,
        'n_kv_heads': 4,
        'vocab_size': 32000,
        'seq_len': 2048
    }
    
    print(f"Configuration:")
    for k, v in config.items():
        print(f"  {k}: {v}")
    
    # Get model state dict (if not already loaded from SafeTensors)
    if checkpoint is not None:
        if 'model' in checkpoint:
            state_dict = checkpoint['model']
        elif 'state_dict' in checkpoint:
            state_dict = checkpoint['state_dict']
        else:
            state_dict = checkpoint
    
    print(f"\nWriting binary to {output_file}...")
    
    with open(output_file, 'wb') as f:
        # Write header (7 ints)
        f.write(struct.pack('i', config['dim']))
        f.write(struct.pack('i', config['hidden_dim']))
        f.write(struct.pack('i', config['n_layers']))
        f.write(struct.pack('i', config['n_heads']))
        f.write(struct.pack('i', config['n_kv_heads']))
        f.write(struct.pack('i', -config['vocab_size']))  # Negative = shared weights
        f.write(struct.pack('i', config['seq_len']))
        
        # Write token embedding
        print("Writing token embeddings...")
        key = 'model.embed_tokens.weight' if 'model.embed_tokens.weight' in state_dict else 'tok_embeddings.weight'
        if key in state_dict:
            serialize_fp32(f, state_dict[key])
        else:
            print("WARNING: Token embedding not found")
            # Look for alternative keys
            print("Available keys:", [k for k in state_dict.keys() if 'embed' in k.lower()])
        
        # Write layer weights
        for layer_idx in range(config['n_layers']):
            print(f"Writing layer {layer_idx + 1}/{config['n_layers']}...")
            
            # Attention RMS norm
            key = f'model.layers.{layer_idx}.input_layernorm.weight'
            if key in state_dict:
                serialize_fp32(f, state_dict[key])
            
            # Q, K, V projections
            for proj in ['q_proj', 'k_proj', 'v_proj']:
                key = f'model.layers.{layer_idx}.self_attn.{proj}.weight'
                if key in state_dict:
                    serialize_fp32(f, state_dict[key])
            
            # Output projection
            key = f'model.layers.{layer_idx}.self_attn.o_proj.weight'
            if key in state_dict:
                serialize_fp32(f, state_dict[key])
            
            # FFN RMS norm
            key = f'model.layers.{layer_idx}.post_attention_layernorm.weight'
            if key in state_dict:
                serialize_fp32(f, state_dict[key])
            
            # Gate, Up, Down projections
            for proj in ['gate_proj', 'up_proj', 'down_proj']:
                key = f'model.layers.{layer_idx}.mlp.{proj}.weight'
                if key in state_dict:
                    serialize_fp32(f, state_dict[key])
        
        # Final RMS norm
        key = 'model.norm.weight' if 'model.norm.weight' in state_dict else 'norm.weight'
        if key in state_dict:
            serialize_fp32(f, state_dict[key])
        
        # Output head (lm_head)
        print("Writing output head...")
        key = 'lm_head.weight' if 'lm_head.weight' in state_dict else 'output.weight'
        if key in state_dict:
            serialize_fp32(f, state_dict[key])
        
        # RoPE frequencies
        print("Writing RoPE frequencies...")
        head_dim = config['dim'] // config['n_heads']
        freq_cis_real = torch.zeros(config['seq_len'], head_dim // 2)
        freq_cis_imag = torch.zeros(config['seq_len'], head_dim // 2)
        serialize_fp32(f, freq_cis_real)
        serialize_fp32(f, freq_cis_imag)
    
    file_size = Path(output_file).stat().st_size
    print(f"\n✓ Export complete: {output_file}")
    print(f"  File size: {file_size / (1024*1024):.2f} MB")
    print(f"  Estimated parameters: {file_size / 4 / 1_000_000:.1f}M")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--model', type=str, required=True, choices=['nanogpt', 'tinyllama_chat'],
                       help='Model to convert')
    parser.add_argument('--input', type=str, required=True,
                       help='Input PyTorch checkpoint (.bin or .pt)')
    parser.add_argument('--output', type=str, required=True,
                       help='Output binary file (.bin)')
    
    args = parser.parse_args()
    
    print("=" * 60)
    print("PyTorch to Bare-Metal Binary Converter")
    print("=" * 60)
    
    if args.model == 'nanogpt':
        export_nanogpt_124m(args.input, args.output)
    elif args.model == 'tinyllama_chat':
        export_tinyllama_chat(args.input, args.output)
    
    print("\n✓ Conversion complete!")
    print(f"  Model ready: {args.output}")
    print("\n📋 Next steps:")
    print("  1. Copy to boot disk with tokenizer.bin")
    print("  2. Rebuild bootloader: make llama2-disk")
    print("  3. Test: make run")

if __name__ == "__main__":
    main()
