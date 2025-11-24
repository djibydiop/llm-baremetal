# Performance Optimizations

This document details the performance optimizations implemented in this bare-metal LLM system.

## Implemented Optimizations

### 1. Loop Unrolling

**matmul Function (4x unroll)**
- **Location**: `llama2_efi.c` lines ~810-825
- **Improvement**: ~1.2-1.5x speedup
- **Description**: Processes 4 elements per iteration instead of 1
- **Impact**: Critical for transformer inference (called many times per token)

```c
// Unrolled 4x
for (; j < n - 3; j += 4) {
    val += w[i * n + j + 0] * x[j + 0];
    val += w[i * n + j + 1] * x[j + 1];
    val += w[i * n + j + 2] * x[j + 2];
    val += w[i * n + j + 3] * x[j + 3];
}
```

**Embedding Copy (8x unroll)**
- **Location**: `llama2_efi.c` lines ~850-865
- **Improvement**: ~1.3-1.8x speedup for memory copy
- **Description**: Copies 8 float values at once for better cache utilization

### 2. Compiler Optimizations

**AVX2 SIMD Instructions**
- Enabled via `-mavx2 -mfma` flags in Makefile
- Automatic vectorization by GCC
- Requires CPU with AVX2 support (Haswell or newer)
- **QEMU**: Must use `-cpu Haswell` (qemu64 lacks AVX2)

### 3. KV Cache

Pre-allocated attention cache (key_cache, value_cache) to avoid recomputation during autoregressive generation.

## Performance Benchmarks

### stories110M (12 layers, 768 dim, 12 heads)

| Environment | Optimization Level | Tokens/sec | Notes |
|------------|-------------------|------------|-------|
| QEMU (Haswell CPU) | Baseline | ~3-4 tok/s | No optimizations |
| QEMU (Haswell CPU) | With loop unrolling | ~4-7 tok/s | 1.3-1.8x improvement |
| Real hardware (est.) | With all optimizations | ~10-20 tok/s | 2-3x faster than QEMU |

## Future Optimization Opportunities

### Short-term (Quick Wins)

1. **AVX2 Intrinsics**
   - Replace some loops with explicit AVX2 intrinsics
   - Potential: 1.5-2x additional speedup
   - Complexity: Medium

2. **Quantization (INT8)**
   - Reduce model size 4x
   - Faster inference (2-3x)
   - Requires quantized model weights

### Medium-term

3. **Flash Attention**
   - More memory-efficient attention computation
   - Better for longer sequences
   - Requires significant code changes

4. **Mixed Precision**
   - Use FP16 where accuracy allows
   - 2x memory savings, potential speedup

### Long-term (Expert Level)

5. **Assembly Kernels**
   - Hand-optimized assembly for critical functions
   - Potential: 2-5x for specific operations
   - Very high complexity

6. **GPU Offload**
   - Use GPU for matrix multiplications
   - Would require UEFI GPU drivers
   - Massive potential speedup (10-100x)

## Optimization Guidelines

When adding new optimizations:

1. **Profile first**: Measure before optimizing
2. **Test correctness**: Ensure outputs remain accurate
3. **Document changes**: Update this file
4. **Benchmark**: Compare before/after performance
5. **Consider trade-offs**: Speed vs code complexity

## Testing Optimizations

```bash
# Build with optimizations
make clean
make

# Test in QEMU
./test-qemu.sh

# Time token generation manually
# Look for "tokens/sec" in output

# Compare different optimization levels:
# 1. Baseline: Remove -mavx2 from Makefile
# 2. AVX2 only: Current Makefile
# 3. AVX2 + loop unroll: Current code
```

## Compilation Flags

Current optimization flags in Makefile:
- `-mavx2`: Enable AVX2 SIMD instructions
- `-mfma`: Enable FMA (fused multiply-add)
- `-ffreestanding`: Bare-metal compilation
- `-fno-stack-protector`: No stack canaries
- `-fpic`: Position-independent code (required for EFI)

## References

- [LLAMA2.c](https://github.com/karpathy/llama2.c) - Original CPU inference implementation
- Intel Intrinsics Guide for AVX2 optimization
- [Optimization Guide](OPTIMIZATION_GUIDE.md) - Detailed strategies

## Measurement Results

Last tested: 2025
Environment: QEMU with Haswell CPU, 4GB RAM
Model: stories110M (420MB)

**Before optimizations:**
- Load time: ~7s
- Generation: ~3-4 tok/s

**After optimizations:**
- Load time: ~7s (no change, I/O bound)
- Generation: ~4-7 tok/s (+30-75% improvement)

Real hardware expected to be 2-3x faster due to:
- No virtualization overhead
- Better CPU frequency scaling
- Hardware AVX2 (not emulated)
