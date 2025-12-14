#!/usr/bin/env python3
"""
Test script to verify LLaMA 3 rope_theta support in the binary format.
This reads an existing .bin file and checks if it has the rope_theta field.
"""

import struct
import sys

def read_config(filepath):
    """Read config header from binary model file."""
    with open(filepath, 'rb') as f:
        # Old format: 8 ints (32 bytes)
        # New format: 8 ints + 1 float (36 bytes)
        config_bytes = f.read(36)
        
        if len(config_bytes) < 32:
            print(f"❌ Error: File too small ({len(config_bytes)} bytes)")
            return None
        
        # Try reading as old format (8 ints)
        dim, hidden_dim, n_layers, n_heads, n_kv_heads, vocab_size, seq_len, model_type = struct.unpack('8i', config_bytes[:32])
        
        print(f"\n📊 Model Configuration:")
        print(f"  dim: {dim}")
        print(f"  hidden_dim: {hidden_dim}")
        print(f"  n_layers: {n_layers}")
        print(f"  n_heads: {n_heads}")
        print(f"  n_kv_heads: {n_kv_heads}")
        print(f"  vocab_size: {vocab_size}")
        print(f"  seq_len: {seq_len}")
        print(f"  model_type: {model_type}")
        
        # Check if we have rope_theta (new format)
        if len(config_bytes) >= 36:
            rope_theta = struct.unpack('f', config_bytes[32:36])[0]
            print(f"  rope_theta: {rope_theta}")
            
            if rope_theta > 0:
                print(f"\n✅ This is a LLaMA 3 format file!")
                if rope_theta == 500000.0:
                    print(f"  → RoPE theta = 500K (LLaMA 3)")
                elif rope_theta == 10000.0:
                    print(f"  → RoPE theta = 10K (LLaMA 2)")
                else:
                    print(f"  → Custom RoPE theta: {rope_theta}")
            else:
                print(f"\n⚠️  Old format (no rope_theta field)")
        else:
            print(f"\n⚠️  Old format - file has only 32 bytes config")
        
        # Analyze attention type
        if n_kv_heads == n_heads:
            print(f"\n🧠 Attention: Multi-Head Attention (MHA)")
            print(f"   Each head has its own KV pair")
        else:
            kv_mul = n_heads // n_kv_heads
            print(f"\n🧠 Attention: Grouped Query Attention (GQA)")
            print(f"   {n_heads} query heads share {n_kv_heads} KV groups")
            print(f"   Grouping ratio: {kv_mul}x (each KV shared by {kv_mul} queries)")
            print(f"   KV cache reduction: {(1 - n_kv_heads/n_heads)*100:.1f}%")
        
        return {
            'dim': dim,
            'hidden_dim': hidden_dim,
            'n_layers': n_layers,
            'n_heads': n_heads,
            'n_kv_heads': n_kv_heads,
            'vocab_size': vocab_size,
            'seq_len': seq_len,
            'model_type': model_type,
            'rope_theta': rope_theta if len(config_bytes) >= 36 else 0.0
        }

def main():
    if len(sys.argv) < 2:
        print("Usage: python test_llama3_support.py <model.bin>")
        print("\nAvailable models:")
        import os
        for f in os.listdir('.'):
            if f.endswith('.bin'):
                size_mb = os.path.getsize(f) / (1024 * 1024)
                print(f"  - {f} ({size_mb:.2f} MB)")
        sys.exit(1)
    
    filepath = sys.argv[1]
    print(f"🔍 Reading model config from: {filepath}")
    
    config = read_config(filepath)
    
    if config:
        print(f"\n✅ Config read successfully!")
        print(f"\n💾 Binary format compatibility:")
        if config['rope_theta'] > 0:
            print(f"  ✅ Compatible with llama2.efi v5.1+ (LLaMA 3 support)")
        else:
            print(f"  ⚠️  Old format - needs conversion for llama2.efi v5.1+")
            print(f"  Tip: Use convert_llama3_to_baremetal.py to regenerate")

if __name__ == '__main__':
    main()
