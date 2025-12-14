#!/usr/bin/env python3
"""
Create a test model file with LLaMA 3 format (including rope_theta field).
This uses the stories15M.bin weights but adds the rope_theta field.
"""

import struct
import sys
import os

def convert_to_llama3_format(input_file, output_file, rope_theta=500000.0):
    """Convert old format to new format with rope_theta."""
    
    print(f"🔄 Converting {input_file} to LLaMA 3 format...")
    print(f"   RoPE theta: {rope_theta}")
    
    with open(input_file, 'rb') as f_in:
        # Read old config (8 ints = 32 bytes)
        old_config = f_in.read(32)
        dim, hidden_dim, n_layers, n_heads, n_kv_heads, vocab_size, seq_len, model_type = struct.unpack('8i', old_config)
        
        print(f"\n📊 Original config:")
        print(f"   dim={dim}, n_layers={n_layers}, n_heads={n_heads}, n_kv_heads={n_kv_heads}")
        
        # Read all remaining data (weights)
        weights_data = f_in.read()
        
    print(f"   Weights size: {len(weights_data) / (1024*1024):.2f} MB")
    
    # Write new format
    with open(output_file, 'wb') as f_out:
        # Write new config (8 ints + 1 float = 36 bytes)
        f_out.write(struct.pack('8i', dim, hidden_dim, n_layers, n_heads, n_kv_heads, vocab_size, seq_len, model_type))
        f_out.write(struct.pack('f', rope_theta))  # NEW: rope_theta field
        
        # Write weights
        f_out.write(weights_data)
    
    output_size = os.path.getsize(output_file) / (1024 * 1024)
    print(f"\n✅ Created: {output_file} ({output_size:.2f} MB)")
    print(f"   Format: LLaMA 3 (with rope_theta={rope_theta})")
    
    return output_file

def main():
    # Test with stories15M (smallest model)
    input_file = "stories15M.bin"
    
    if not os.path.exists(input_file):
        print(f"❌ Error: {input_file} not found")
        print(f"   Available files:")
        for f in os.listdir('.'):
            if f.endswith('.bin'):
                print(f"     - {f}")
        sys.exit(1)
    
    # Create test models with different rope_theta values
    test_models = [
        ("stories15M_llama2.bin", 10000.0),    # LLaMA 2 style
        ("stories15M_llama3.bin", 500000.0),   # LLaMA 3 style
    ]
    
    for output_file, rope_theta in test_models:
        convert_to_llama3_format(input_file, output_file, rope_theta)
        print()
    
    print("🎯 Test models created!")
    print("\nNext steps:")
    print("  1. Test with: python test_llama3_support.py stories15M_llama3.bin")
    print("  2. Boot in QEMU with the new format")
    print("  3. Verify rope_theta is read correctly")

if __name__ == '__main__':
    main()
