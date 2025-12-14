#!/usr/bin/env python3
"""
LLaMA 3 to Bare-Metal Converter
Converts Hugging Face LLaMA 3 models to bare-metal binary format with INT4 quantization.

Usage:
    python convert_llama3_to_baremetal.py \
        --model meta-llama/Meta-Llama-3-8B \
        --output llama3_8b_q4.bin \
        --quantize int4

Author: djibydiop
License: MIT
"""

import argparse
import struct
import numpy as np
import torch
from pathlib import Path
from typing import Dict, List, Tuple
from tqdm import tqdm

try:
    from transformers import LlamaForCausalLM, LlamaConfig
except ImportError:
    print("❌ transformers not installed. Run: pip install transformers")
    exit(1)


# ============================================================================
# INT4 Quantization
# ============================================================================

class INT4Quantizer:
    """Group-wise INT4 quantization for model weights."""
    
    def __init__(self, group_size: int = 32):
        self.group_size = group_size
    
    def quantize(self, weights: np.ndarray) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """
        Quantize weights to INT4 with per-group scale and zero-point.
        
        Args:
            weights: FP32 weights (any shape)
        
        Returns:
            data: Packed INT4 values (uint8 array, 2 values per byte)
            scales: Per-group scale factors (float32)
            zeros: Per-group zero points (float32)
        """
        flat_weights = weights.flatten()
        n = len(flat_weights)
        n_groups = (n + self.group_size - 1) // self.group_size
        
        # Pad to multiple of group_size
        padded_n = n_groups * self.group_size
        if padded_n > n:
            flat_weights = np.pad(flat_weights, (0, padded_n - n), mode='constant')
        
        # Reshape into groups
        grouped = flat_weights.reshape(n_groups, self.group_size)
        
        # Compute per-group min/max
        group_mins = grouped.min(axis=1)
        group_maxs = grouped.max(axis=1)
        
        # Avoid division by zero
        ranges = group_maxs - group_mins
        ranges = np.where(ranges == 0, 1.0, ranges)
        
        # Compute scale and zero-point
        scales = ranges / 15.0  # INT4 range: 0-15
        zeros = group_mins
        
        # Quantize each group
        quantized = np.zeros((n_groups, self.group_size), dtype=np.uint8)
        for g in range(n_groups):
            group_vals = grouped[g]
            q = np.round((group_vals - zeros[g]) / scales[g])
            q = np.clip(q, 0, 15).astype(np.uint8)
            quantized[g] = q
        
        # Pack 2 INT4 values per byte (high nibble, low nibble)
        quantized_flat = quantized.flatten()
        packed_size = (len(quantized_flat) + 1) // 2
        packed = np.zeros(packed_size, dtype=np.uint8)
        
        for i in range(0, len(quantized_flat), 2):
            high = quantized_flat[i]
            low = quantized_flat[i+1] if i+1 < len(quantized_flat) else 0
            packed[i//2] = (high << 4) | low
        
        return packed, scales.astype(np.float32), zeros.astype(np.float32)
    
    def estimate_size(self, n_params: int) -> int:
        """Estimate quantized size in bytes."""
        n_groups = (n_params + self.group_size - 1) // self.group_size
        data_size = (n_params + 1) // 2  # 4 bits per value
        metadata_size = n_groups * 8  # 2 floats per group
        return data_size + metadata_size


# ============================================================================
# Model Converter
# ============================================================================

class LLaMA3Converter:
    """Converts LLaMA 3 from Hugging Face to bare-metal format."""
    
    def __init__(self, model_name: str, quantize: str = "int4", group_size: int = 32):
        self.model_name = model_name
        self.quantize = quantize
        self.quantizer = INT4Quantizer(group_size) if quantize == "int4" else None
        
        print(f"📦 Loading model: {model_name}")
        self.model = LlamaForCausalLM.from_pretrained(
            model_name,
            torch_dtype=torch.float32,
            device_map="cpu",
            low_cpu_mem_usage=True
        )
        self.config = self.model.config
        
        print(f"✓ Model loaded: {self.config.num_hidden_layers} layers, "
              f"{self.config.vocab_size} vocab")
    
    def extract_weights(self) -> Dict[str, np.ndarray]:
        """Extract all weights from the model."""
        print("📊 Extracting weights...")
        weights = {}
        
        for name, param in tqdm(self.model.named_parameters(), desc="Extracting"):
            weights[name] = param.detach().cpu().numpy()
        
        total_params = sum(w.size for w in weights.values())
        print(f"✓ Extracted {len(weights)} tensors ({total_params/1e9:.2f}B params)")
        
        return weights
    
    def write_config(self, f):
        """Write model configuration header."""
        print("📝 Writing config...")
        
        # LLaMA 3 config structure (must match llama2_efi.c Config struct)
        f.write(struct.pack('i', self.config.hidden_size))           # dim
        f.write(struct.pack('i', self.config.intermediate_size))      # hidden_dim
        f.write(struct.pack('i', self.config.num_hidden_layers))     # n_layers
        f.write(struct.pack('i', self.config.num_attention_heads))   # n_heads
        f.write(struct.pack('i', self.config.num_key_value_heads))   # n_kv_heads (GQA!)
        f.write(struct.pack('i', self.config.vocab_size))            # vocab_size
        f.write(struct.pack('i', self.config.max_position_embeddings)) # seq_len
        f.write(struct.pack('i', 0))                                 # model_type (0 = unknown)
        
        # LLaMA 3 specific: RoPE theta
        rope_theta = getattr(self.config, 'rope_theta', 500000.0)
        f.write(struct.pack('f', rope_theta))
        
        print(f"✓ Config written:")
        print(f"  - dim: {self.config.hidden_size}")
        print(f"  - n_layers: {self.config.num_hidden_layers}")
        print(f"  - n_heads: {self.config.num_attention_heads}")
        print(f"  - n_kv_heads: {self.config.num_key_value_heads} (GQA)")
        print(f"  - vocab: {self.config.vocab_size}")
        print(f"  - seq_len: {self.config.max_position_embeddings}")
        print(f"  - rope_theta: {rope_theta}")
    
    def write_weights(self, f, weights: Dict[str, np.ndarray]):
        """Write quantized weights to file."""
        print(f"\n🔢 Quantizing weights ({self.quantize})...")
        
        # Order weights as expected by bare-metal loader
        weight_order = self._get_weight_order()
        
        total_size = 0
        original_size = 0
        
        for name in tqdm(weight_order, desc="Quantizing & Writing"):
            if name not in weights:
                print(f"⚠️  Warning: {name} not found in model")
                continue
            
            w = weights[name]
            original_size += w.nbytes
            
            if self.quantize == "int4":
                # Quantize to INT4
                packed, scales, zeros = self.quantizer.quantize(w)
                
                # Write: n_elements, n_groups, packed_data, scales, zeros
                n_elements = w.size
                n_groups = len(scales)
                
                f.write(struct.pack('i', n_elements))
                f.write(struct.pack('i', n_groups))
                f.write(packed.tobytes())
                f.write(scales.tobytes())
                f.write(zeros.tobytes())
                
                size = 8 + len(packed) + len(scales)*4 + len(zeros)*4
                total_size += size
                
            elif self.quantize == "fp32":
                # No quantization
                f.write(w.tobytes())
                total_size += w.nbytes
            
            else:
                raise ValueError(f"Unknown quantization: {self.quantize}")
        
        compression = (1 - total_size / original_size) * 100
        print(f"\n✓ Weights written:")
        print(f"  - Original: {original_size/1e9:.2f} GB")
        print(f"  - Compressed: {total_size/1e9:.2f} GB")
        print(f"  - Compression: {compression:.1f}%")
        
        return total_size
    
    def _get_weight_order(self) -> List[str]:
        """Get expected weight order for bare-metal loader."""
        # LLaMA 3 weight naming convention
        weights = []
        
        # Token embeddings
        weights.append("model.embed_tokens.weight")
        
        # Transformer layers
        for i in range(self.config.num_hidden_layers):
            prefix = f"model.layers.{i}"
            
            # Attention
            weights.append(f"{prefix}.self_attn.q_proj.weight")
            weights.append(f"{prefix}.self_attn.k_proj.weight")
            weights.append(f"{prefix}.self_attn.v_proj.weight")
            weights.append(f"{prefix}.self_attn.o_proj.weight")
            
            # MLP
            weights.append(f"{prefix}.mlp.gate_proj.weight")
            weights.append(f"{prefix}.mlp.up_proj.weight")
            weights.append(f"{prefix}.mlp.down_proj.weight")
            
            # RMS Norm
            weights.append(f"{prefix}.input_layernorm.weight")
            weights.append(f"{prefix}.post_attention_layernorm.weight")
        
        # Final norm
        weights.append("model.norm.weight")
        
        # Output layer
        weights.append("lm_head.weight")
        
        return weights
    
    def convert(self, output_path: str):
        """Main conversion pipeline."""
        print(f"\n{'='*60}")
        print(f"LLaMA 3 → Bare-Metal Converter")
        print(f"{'='*60}\n")
        
        # Extract weights
        weights = self.extract_weights()
        
        # Write to file
        print(f"\n💾 Writing to: {output_path}")
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_path, 'wb') as f:
            # Write config header
            self.write_config(f)
            
            # Write quantized weights
            file_size = self.write_weights(f, weights)
        
        actual_size = output_path.stat().st_size
        print(f"\n✅ Conversion complete!")
        print(f"📁 Output: {output_path}")
        print(f"📏 Size: {actual_size/1e9:.2f} GB")
        print(f"\n🚀 Ready for bare-metal deployment!")


# ============================================================================
# CLI
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convert LLaMA 3 to bare-metal format",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Convert LLaMA 3 8B with INT4 quantization
  python convert_llama3_to_baremetal.py \\
      --model meta-llama/Meta-Llama-3-8B \\
      --output llama3_8b_q4.bin \\
      --quantize int4
  
  # Convert with larger group size (better quality, larger file)
  python convert_llama3_to_baremetal.py \\
      --model meta-llama/Meta-Llama-3-8B \\
      --output llama3_8b_q4_g64.bin \\
      --quantize int4 \\
      --group-size 64
        """
    )
    
    parser.add_argument(
        "--model",
        type=str,
        required=True,
        help="Hugging Face model name (e.g., meta-llama/Meta-Llama-3-8B)"
    )
    
    parser.add_argument(
        "--output",
        type=str,
        required=True,
        help="Output binary file path"
    )
    
    parser.add_argument(
        "--quantize",
        type=str,
        choices=["int4", "fp32"],
        default="int4",
        help="Quantization method (default: int4)"
    )
    
    parser.add_argument(
        "--group-size",
        type=int,
        default=32,
        help="Group size for INT4 quantization (default: 32)"
    )
    
    args = parser.parse_args()
    
    # Convert
    converter = LLaMA3Converter(
        model_name=args.model,
        quantize=args.quantize,
        group_size=args.group_size
    )
    
    converter.convert(args.output)


if __name__ == "__main__":
    main()
