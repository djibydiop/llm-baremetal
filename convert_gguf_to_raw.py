#!/usr/bin/env python3
"""
Convert GGUF format to raw binary format compatible with llama2_efi.c

GGUF format: https://github.com/ggerganov/ggml/blob/master/docs/gguf.md
Target format: Same as stories110M.bin (Karpathy's llama2.c format)
"""

import sys
import struct
import numpy as np
from pathlib import Path

# GGUF magic number
GGUF_MAGIC = 0x46554747  # "GGUF" in little-endian

def read_gguf_header(f):
    """Read GGUF file header"""
    magic = struct.unpack('<I', f.read(4))[0]
    if magic != GGUF_MAGIC:
        raise ValueError(f"Not a GGUF file. Magic: 0x{magic:08x}")
    
    version = struct.unpack('<I', f.read(4))[0]
    print(f"GGUF version: {version}")
    
    tensor_count = struct.unpack('<Q', f.read(8))[0]
    metadata_kv_count = struct.unpack('<Q', f.read(8))[0]
    
    print(f"Tensors: {tensor_count}, Metadata KVs: {metadata_kv_count}")
    return version, tensor_count, metadata_kv_count

def read_gguf_string(f):
    """Read GGUF string (length-prefixed)"""
    length = struct.unpack('<Q', f.read(8))[0]
    return f.read(length).decode('utf-8')

def read_gguf_metadata(f, kv_count):
    """Read GGUF metadata key-value pairs"""
    metadata = {}
    
    for _ in range(kv_count):
        key = read_gguf_string(f)
        value_type = struct.unpack('<I', f.read(4))[0]
        
        # Type mapping (simplified)
        if value_type == 4:  # UINT32
            value = struct.unpack('<I', f.read(4))[0]
        elif value_type == 5:  # INT32
            value = struct.unpack('<i', f.read(4))[0]
        elif value_type == 6:  # FLOAT32
            value = struct.unpack('<f', f.read(4))[0]
        elif value_type == 8:  # STRING
            value = read_gguf_string(f)
        elif value_type == 9:  # ARRAY
            array_type = struct.unpack('<I', f.read(4))[0]
            array_len = struct.unpack('<Q', f.read(8))[0]
            # Skip array data for now
            if array_type == 4:  # UINT32 array
                f.read(array_len * 4)
            value = f"<array of {array_len} items>"
        else:
            print(f"Unknown type {value_type} for key {key}, skipping")
            continue
        
        metadata[key] = value
        print(f"  {key}: {value}")
    
    return metadata

def convert_gguf_to_raw(gguf_path, output_path):
    """Convert GGUF to raw binary format"""
    print(f"Converting {gguf_path} to {output_path}")
    
    with open(gguf_path, 'rb') as f:
        # Read header
        version, tensor_count, metadata_kv_count = read_gguf_header(f)
        
        # Read metadata
        metadata = read_gguf_metadata(f, metadata_kv_count)
        
        # Extract model config
        config = {
            'dim': metadata.get('llama.embedding_length', 2048),
            'hidden_dim': metadata.get('llama.feed_forward_length', 5632),
            'n_layers': metadata.get('llama.block_count', 22),
            'n_heads': metadata.get('llama.attention.head_count', 32),
            'n_kv_heads': metadata.get('llama.attention.head_count_kv', 4),
            'vocab_size': metadata.get('llama.vocab_size', 32000),
            'seq_len': metadata.get('llama.context_length', 2048),
        }
        
        print("\nModel configuration:")
        for k, v in config.items():
            print(f"  {k}: {v}")
        
        # Write header (Karpathy format)
        with open(output_path, 'wb') as out:
            out.write(struct.pack('i', config['dim']))
            out.write(struct.pack('i', config['hidden_dim']))
            out.write(struct.pack('i', config['n_layers']))
            out.write(struct.pack('i', config['n_heads']))
            out.write(struct.pack('i', config['n_kv_heads']))
            out.write(struct.pack('i', config['vocab_size']))
            out.write(struct.pack('i', config['seq_len']))
            
            print(f"\nWriting tensors (this may take several minutes)...")
            
            # Read tensor info
            tensors = []
            for i in range(tensor_count):
                name = read_gguf_string(f)
                n_dims = struct.unpack('<I', f.read(4))[0]
                dims = struct.unpack(f'<{n_dims}Q', f.read(8 * n_dims))
                dtype = struct.unpack('<I', f.read(4))[0]
                offset = struct.unpack('<Q', f.read(8))[0]
                
                tensors.append({
                    'name': name,
                    'dims': dims,
                    'dtype': dtype,
                    'offset': offset
                })
            
            # Align to tensor data section (typically aligned to 32 bytes)
            current_pos = f.tell()
            alignment = 32
            aligned_pos = (current_pos + alignment - 1) // alignment * alignment
            f.seek(aligned_pos)
            
            tensor_data_start = f.tell()
            
            # Read and write tensors in expected order
            tensor_map = {t['name']: t for t in tensors}
            
            # Order matters! Must match llama2_efi.c expectations
            # For now, just dump all tensor data
            print(f"Total tensors: {len(tensors)}")
            
            for i, tensor in enumerate(tensors):
                f.seek(tensor_data_start + tensor['offset'])
                
                # Calculate tensor size
                size = 1
                for dim in tensor['dims']:
                    size *= dim
                
                # Read based on dtype
                if tensor['dtype'] == 0:  # F32
                    data = np.frombuffer(f.read(size * 4), dtype=np.float32)
                elif tensor['dtype'] == 1:  # F16
                    data = np.frombuffer(f.read(size * 2), dtype=np.float16).astype(np.float32)
                elif tensor['dtype'] == 2:  # Q4_0 (quantized)
                    # For Q8_0: read quantized and dequantize
                    # Simplified: just read as bytes for now
                    block_size = 32
                    blocks = (size + block_size - 1) // block_size
                    raw_data = f.read(blocks * 34)  # Q8_0: 32 weights + 2 bytes scale
                    # Skip dequantization for now - would need proper implementation
                    print(f"  Warning: Tensor {tensor['name']} is quantized (Q8_0), skipping")
                    continue
                else:
                    print(f"  Unknown dtype {tensor['dtype']} for {tensor['name']}")
                    continue
                
                # Write as float32
                out.write(data.tobytes())
                
                if (i + 1) % 10 == 0:
                    print(f"  Processed {i+1}/{len(tensors)} tensors...")
    
    print(f"\n✓ Conversion complete: {output_path}")
    print(f"  Size: {Path(output_path).stat().st_size / (1024*1024):.1f} MB")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python convert_gguf_to_raw.py <input.gguf> <output.bin>")
        sys.exit(1)
    
    gguf_path = sys.argv[1]
    output_path = sys.argv[2]
    
    convert_gguf_to_raw(gguf_path, output_path)
