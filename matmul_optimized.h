/*
 * OPTIMIZED MATRIX MULTIPLICATION
 * Based on advanced tinyBLAS optimizations
 * 
 * Techniques:
 * - Blocked (tiled) algorithm for cache efficiency
 * - CPU feature detection (AVX2/AVX-512)
 * - Loop unrolling
 * 
 * Made in Senegal by Djiby Diop - December 2025
 */

#ifndef MATMUL_OPTIMIZED_H
#define MATMUL_OPTIMIZED_H

#include <efi.h>
#include <efilib.h>

// ============================================================================
// CPU FEATURE DETECTION
// ============================================================================

typedef struct {
    BOOLEAN has_sse2;
    BOOLEAN has_avx;
    BOOLEAN has_avx2;
    BOOLEAN has_avx512;
    BOOLEAN has_fma;
} MatmulCPUFeatures;

static MatmulCPUFeatures g_cpu = {
    .has_sse2 = FALSE,
    .has_avx = FALSE,
    .has_avx2 = FALSE,
    .has_avx512 = FALSE,
    .has_fma = FALSE
};

// CPUID instruction wrapper
static inline void cpuid(UINT32 leaf, UINT32 subleaf, UINT32* eax, UINT32* ebx, UINT32* ecx, UINT32* edx) {
    __asm__ volatile(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

// Detect CPU features
void detect_cpu_features(void) {
    UINT32 eax, ebx, ecx, edx;
    
    // Get max CPUID leaf
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    UINT32 max_leaf = eax;
    
    if (max_leaf >= 1) {
        // Leaf 1: SSE2, AVX, FMA
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        g_cpu.has_sse2 = (edx & (1 << 26)) != 0;  // SSE2
        g_cpu.has_avx = (ecx & (1 << 28)) != 0;   // AVX
        g_cpu.has_fma = (ecx & (1 << 12)) != 0;   // FMA
    }
    
    if (max_leaf >= 7) {
        // Leaf 7: AVX2, AVX-512
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        g_cpu.has_avx2 = (ebx & (1 << 5)) != 0;    // AVX2
        g_cpu.has_avx512 = (ebx & (1 << 16)) != 0; // AVX-512F
    }
}

// ============================================================================
// BLOCKED MATRIX MULTIPLICATION
// ============================================================================

#define BLOCK_SIZE 32  // Optimize for L1 cache (32 KB)

// Generic blocked matmul (no SIMD)
void matmul_blocked_generic(float* C, const float* A, const float* B, int M, int N, int K) {
    // C = A * B
    // A: M x K
    // B: K x N
    // C: M x N
    
    // Zero output
    for (int i = 0; i < M * N; i++) {
        C[i] = 0.0f;
    }
    
    // Blocked algorithm (cache-friendly)
    for (int ii = 0; ii < M; ii += BLOCK_SIZE) {
        for (int jj = 0; jj < N; jj += BLOCK_SIZE) {
            for (int kk = 0; kk < K; kk += BLOCK_SIZE) {
                // Inner block
                int i_end = (ii + BLOCK_SIZE < M) ? ii + BLOCK_SIZE : M;
                int j_end = (jj + BLOCK_SIZE < N) ? jj + BLOCK_SIZE : N;
                int k_end = (kk + BLOCK_SIZE < K) ? kk + BLOCK_SIZE : K;
                
                for (int i = ii; i < i_end; i++) {
                    for (int j = jj; j < j_end; j++) {
                        float sum = C[i * N + j];
                        
                        // Unroll by 4 for better ILP
                        int k = kk;
                        for (; k + 3 < k_end; k += 4) {
                            sum += A[i * K + k] * B[k * N + j];
                            sum += A[i * K + k + 1] * B[(k + 1) * N + j];
                            sum += A[i * K + k + 2] * B[(k + 2) * N + j];
                            sum += A[i * K + k + 3] * B[(k + 3) * N + j];
                        }
                        
                        // Remainder
                        for (; k < k_end; k++) {
                            sum += A[i * K + k] * B[k * N + j];
                        }
                        
                        C[i * N + j] = sum;
                    }
                }
            }
        }
    }
}

// Matrix-vector multiplication (optimized for transformers)
void matvec_blocked(float* out, const float* mat, const float* vec, int rows, int cols) {
    // out = mat * vec
    // mat: rows x cols
    // vec: cols x 1
    // out: rows x 1
    
    for (int i = 0; i < rows; i++) {
        float sum = 0.0f;
        const float* row = &mat[i * cols];
        
        // Unroll by 8
        int j = 0;
        for (; j + 7 < cols; j += 8) {
            sum += row[j] * vec[j];
            sum += row[j + 1] * vec[j + 1];
            sum += row[j + 2] * vec[j + 2];
            sum += row[j + 3] * vec[j + 3];
            sum += row[j + 4] * vec[j + 4];
            sum += row[j + 5] * vec[j + 5];
            sum += row[j + 6] * vec[j + 6];
            sum += row[j + 7] * vec[j + 7];
        }
        
        // Remainder
        for (; j < cols; j++) {
            sum += row[j] * vec[j];
        }
        
        out[i] = sum;
    }
}

// ============================================================================
// DISPATCH FUNCTIONS
// ============================================================================

// Select best implementation based on CPU
void matmul_optimized(float* C, const float* A, const float* B, int M, int N, int K) {
    // For now, use blocked generic
    // Future: add AVX2/AVX-512 versions
    matmul_blocked_generic(C, A, B, M, N, K);
}

void matvec_optimized(float* out, const float* mat, const float* vec, int rows, int cols) {
    matvec_blocked(out, mat, vec, rows, cols);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void matmul_init(void) {
    detect_cpu_features();
    
    Print(L"[MATMUL] 🚀 Optimized matrix multiplication initialized\r\n");
    Print(L"[MATMUL] CPU Features:\r\n");
    Print(L"  SSE2:    %s\r\n", g_cpu.has_sse2 ? L"YES" : L"NO");
    Print(L"  AVX:     %s\r\n", g_cpu.has_avx ? L"YES" : L"NO");
    Print(L"  AVX2:    %s\r\n", g_cpu.has_avx2 ? L"YES" : L"NO");
    Print(L"  AVX-512: %s\r\n", g_cpu.has_avx512 ? L"YES" : L"NO");
    Print(L"  FMA:     %s\r\n", g_cpu.has_fma ? L"YES" : L"NO");
    Print(L"[MATMUL] Algorithm: Blocked (32x32 tiles)\r\n");
}

#endif // MATMUL_OPTIMIZED_H
