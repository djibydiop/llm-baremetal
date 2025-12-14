#!/usr/bin/env python3
"""
Test INT4 Quantization Quality
Validates quantization accuracy and measures performance degradation.

Usage:
    python test_quantization.py --weights llama3_8b_q4.bin
"""

import argparse
import numpy as np
import struct
from pathlib import Path


class QuantizationTester:
    """Test INT4 quantization quality."""
    
    def __init__(self, group_size: int = 32):
        self.group_size = group_size
    
    def quantize_dequantize(self, weights: np.ndarray):
        """Quantize and dequantize to measure error."""
        flat = weights.flatten()
        n = len(flat)
        n_groups = (n + self.group_size - 1) // self.group_size
        
        # Pad
        padded_n = n_groups * self.group_size
        if padded_n > n:
            flat = np.pad(flat, (0, padded_n - n), mode='constant')
        
        grouped = flat.reshape(n_groups, self.group_size)
        
        # Quantize
        group_mins = grouped.min(axis=1)
        group_maxs = grouped.max(axis=1)
        ranges = np.where(group_maxs - group_mins == 0, 1.0, group_maxs - group_mins)
        
        scales = ranges / 15.0
        zeros = group_mins
        
        quantized = np.zeros_like(grouped, dtype=np.uint8)
        for g in range(n_groups):
            q = np.round((grouped[g] - zeros[g]) / scales[g])
            quantized[g] = np.clip(q, 0, 15).astype(np.uint8)
        
        # Dequantize
        dequantized = np.zeros_like(grouped, dtype=np.float32)
        for g in range(n_groups):
            dequantized[g] = zeros[g] + quantized[g] * scales[g]
        
        dequantized = dequantized.flatten()[:n]
        
        return dequantized
    
    def test_accuracy(self, original: np.ndarray, dequantized: np.ndarray):
        """Compute error metrics."""
        abs_error = np.abs(original - dequantized)
        rel_error = abs_error / (np.abs(original) + 1e-8)
        
        metrics = {
            "mean_abs_error": np.mean(abs_error),
            "max_abs_error": np.max(abs_error),
            "mean_rel_error": np.mean(rel_error),
            "max_rel_error": np.max(rel_error),
            "mse": np.mean((original - dequantized)**2),
            "snr_db": 10 * np.log10(np.var(original) / (np.var(original - dequantized) + 1e-10))
        }
        
        return metrics
    
    def run_tests(self):
        """Run comprehensive tests."""
        print("🧪 Testing INT4 Quantization\n")
        
        test_cases = [
            ("Small weights (normal)", np.random.randn(1000) * 0.01),
            ("Large weights (outliers)", np.random.randn(1000) * 10.0),
            ("Uniform distribution", np.random.uniform(-1, 1, 1000)),
            ("Attention weights", np.random.randn(1000) * 0.1),
            ("Zero-centered", np.random.randn(1000) - 0.5),
        ]
        
        print(f"{'Test Case':<30} {'Mean Err':<12} {'Max Err':<12} {'SNR (dB)':<12}")
        print("="*70)
        
        for name, weights in test_cases:
            dequantized = self.quantize_dequantize(weights)
            metrics = self.test_accuracy(weights, dequantized)
            
            print(f"{name:<30} "
                  f"{metrics['mean_abs_error']:<12.6f} "
                  f"{metrics['max_abs_error']:<12.6f} "
                  f"{metrics['snr_db']:<12.2f}")
        
        print("\n✅ Quantization tests complete!")


def main():
    parser = argparse.ArgumentParser(description="Test INT4 quantization quality")
    parser.add_argument("--group-size", type=int, default=32, help="Group size")
    args = parser.parse_args()
    
    tester = QuantizationTester(group_size=args.group_size)
    tester.run_tests()


if __name__ == "__main__":
    main()
