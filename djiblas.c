/*
 * DjibLAS - Implementation
 * Optimized matmul kernels for bare-metal UEFI
 * 
 * Created by Djiby Diop - Made in Senegal 🇸🇳
 * December 2025
 */

#include "djiblas.h"

// CPUID for x86 feature detection
static inline void cpuid(UINT32 leaf, UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx) {
#if defined(__x86_64__) || defined(_M_X64)
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(0));
#else
    *eax = *ebx = *ecx = *edx = 0;
#endif
}

void djiblas_detect_cpu(CPUFeatures *features) {
    UINT32 eax, ebx, ecx, edx;
    
    // Clear all features
    features->has_sse2 = FALSE;
    features->has_avx = FALSE;
    features->has_avx2 = FALSE;
    features->has_fma = FALSE;
    features->has_avx512f = FALSE;
    features->has_avx512_vnni = FALSE;

#if DJIBLAS_DISABLE_CPUID
    // Safe baseline: we compile the project with at least SSE2 enabled.
    // Avoid executing CPUID, which may #UD in some UEFI/QEMU configurations.
    features->has_sse2 = TRUE;
    return;
#endif
    
#if defined(__x86_64__) || defined(_M_X64)
    // Check for CPUID support
    cpuid(0, &eax, &ebx, &ecx, &edx);
    if (eax == 0) return;
    
    // CPUID leaf 1: SSE2, AVX, FMA
    cpuid(1, &eax, &ebx, &ecx, &edx);
    features->has_sse2 = (edx & (1 << 26)) != 0;  // SSE2
    features->has_avx = (ecx & (1 << 28)) != 0;   // AVX
    features->has_fma = (ecx & (1 << 12)) != 0;   // FMA3
    
    // CPUID leaf 7: AVX2, AVX512
    cpuid(7, &eax, &ebx, &ecx, &edx);
    features->has_avx2 = (ebx & (1 << 5)) != 0;        // AVX2
    features->has_avx512f = (ebx & (1 << 16)) != 0;    // AVX512F
    features->has_avx512_vnni = (ecx & (1 << 11)) != 0; // AVX512_VNNI
#endif
}

// ===================================================================
// SCALAR FALLBACK (no SIMD)
// ===================================================================
void djiblas_sgemm_scalar(int m, int n, int k, 
                           const float *A, int lda,
                           const float *B, int ldb,
                           float *C, int ldc) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int l = 0; l < k; l++) {
                sum += A[lda * i + l] * B[ldb * j + l];
            }
            C[ldc * j + i] = sum;
        }
    }
}

// ===================================================================
// SSE2 KERNEL (baseline x86-64)
// ===================================================================
#if defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h>  // SSE2

void djiblas_sgemm_sse2(int m, int n, int k,
                         const float *A, int lda,
                         const float *B, int ldb,
                         float *C, int ldc) {
    // Process 4 elements at a time with SSE2
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            __m128 sum = _mm_setzero_ps();
            
            int l = 0;
            for (; l + 4 <= k; l += 4) {
                __m128 a = _mm_loadu_ps(&A[lda * i + l]);
                __m128 b = _mm_loadu_ps(&B[ldb * j + l]);
                sum = _mm_add_ps(sum, _mm_mul_ps(a, b));
            }
            
            // Horizontal sum
            float result[4];
            _mm_storeu_ps(result, sum);
            float total = result[0] + result[1] + result[2] + result[3];
            
            // Handle remainder
            for (; l < k; l++) {
                total += A[lda * i + l] * B[ldb * j + l];
            }
            
            C[ldc * j + i] = total;
        }
    }
}
#else
void djiblas_sgemm_sse2(int m, int n, int k,
                         const float *A, int lda,
                         const float *B, int ldb,
                         float *C, int ldc) {
    djiblas_sgemm_scalar(m, n, k, A, lda, B, ldb, C, ldc);
}
#endif

// ===================================================================
// AVX2 KERNEL (8 floats at once, with FMA)
// ===================================================================
#if defined(__AVX2__) && defined(__FMA__)
#include <immintrin.h>

static inline float hsum_avx(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    lo = _mm_add_ps(lo, hi);
    hi = _mm_movehl_ps(hi, lo);
    lo = _mm_add_ps(lo, hi);
    hi = _mm_shuffle_ps(lo, lo, 1);
    lo = _mm_add_ss(lo, hi);
    return _mm_cvtss_f32(lo);
}

void djiblas_sgemm_avx2(int m, int n, int k,
                         const float *A, int lda,
                         const float *B, int ldb,
                         float *C, int ldc) {
    // Optimized 3x4 tile algorithm
    for (int i = 0; i < m; i += 3) {
        for (int j = 0; j < n; j += 4) {
            __m256 c00 = _mm256_setzero_ps();
            __m256 c01 = _mm256_setzero_ps();
            __m256 c02 = _mm256_setzero_ps();
            __m256 c03 = _mm256_setzero_ps();
            __m256 c10 = _mm256_setzero_ps();
            __m256 c11 = _mm256_setzero_ps();
            __m256 c12 = _mm256_setzero_ps();
            __m256 c13 = _mm256_setzero_ps();
            __m256 c20 = _mm256_setzero_ps();
            __m256 c21 = _mm256_setzero_ps();
            __m256 c22 = _mm256_setzero_ps();
            __m256 c23 = _mm256_setzero_ps();
            
            int l = 0;
            for (; l + 8 <= k; l += 8) {
                __m256 b0 = _mm256_loadu_ps(&B[ldb * (j + 0) + l]);
                __m256 b1 = _mm256_loadu_ps(&B[ldb * (j + 1) + l]);
                __m256 b2 = _mm256_loadu_ps(&B[ldb * (j + 2) + l]);
                __m256 b3 = _mm256_loadu_ps(&B[ldb * (j + 3) + l]);
                
                if (i + 0 < m) {
                    __m256 a0 = _mm256_loadu_ps(&A[lda * (i + 0) + l]);
                    c00 = _mm256_fmadd_ps(a0, b0, c00);
                    c01 = _mm256_fmadd_ps(a0, b1, c01);
                    c02 = _mm256_fmadd_ps(a0, b2, c02);
                    c03 = _mm256_fmadd_ps(a0, b3, c03);
                }
                
                if (i + 1 < m) {
                    __m256 a1 = _mm256_loadu_ps(&A[lda * (i + 1) + l]);
                    c10 = _mm256_fmadd_ps(a1, b0, c10);
                    c11 = _mm256_fmadd_ps(a1, b1, c11);
                    c12 = _mm256_fmadd_ps(a1, b2, c12);
                    c13 = _mm256_fmadd_ps(a1, b3, c13);
                }
                
                if (i + 2 < m) {
                    __m256 a2 = _mm256_loadu_ps(&A[lda * (i + 2) + l]);
                    c20 = _mm256_fmadd_ps(a2, b0, c20);
                    c21 = _mm256_fmadd_ps(a2, b1, c21);
                    c22 = _mm256_fmadd_ps(a2, b2, c22);
                    c23 = _mm256_fmadd_ps(a2, b3, c23);
                }
            }
            
            // Store results
            if (i + 0 < m) {
                if (j + 0 < n) C[ldc * (j + 0) + (i + 0)] = hsum_avx(c00);
                if (j + 1 < n) C[ldc * (j + 1) + (i + 0)] = hsum_avx(c01);
                if (j + 2 < n) C[ldc * (j + 2) + (i + 0)] = hsum_avx(c02);
                if (j + 3 < n) C[ldc * (j + 3) + (i + 0)] = hsum_avx(c03);
            }
            if (i + 1 < m) {
                if (j + 0 < n) C[ldc * (j + 0) + (i + 1)] = hsum_avx(c10);
                if (j + 1 < n) C[ldc * (j + 1) + (i + 1)] = hsum_avx(c11);
                if (j + 2 < n) C[ldc * (j + 2) + (i + 1)] = hsum_avx(c12);
                if (j + 3 < n) C[ldc * (j + 3) + (i + 1)] = hsum_avx(c13);
            }
            if (i + 2 < m) {
                if (j + 0 < n) C[ldc * (j + 0) + (i + 2)] = hsum_avx(c20);
                if (j + 1 < n) C[ldc * (j + 1) + (i + 2)] = hsum_avx(c21);
                if (j + 2 < n) C[ldc * (j + 2) + (i + 2)] = hsum_avx(c22);
                if (j + 3 < n) C[ldc * (j + 3) + (i + 2)] = hsum_avx(c23);
            }
            
            // Handle remainder with scalar
            for (; l < k; l++) {
                for (int ii = i; ii < i + 3 && ii < m; ii++) {
                    for (int jj = j; jj < j + 4 && jj < n; jj++) {
                        C[ldc * jj + ii] += A[lda * ii + l] * B[ldb * jj + l];
                    }
                }
            }
        }
    }
}
#else
void djiblas_sgemm_avx2(int m, int n, int k,
                         const float *A, int lda,
                         const float *B, int ldb,
                         float *C, int ldc) {
    djiblas_sgemm_sse2(m, n, k, A, lda, B, ldb, C, ldc);
}
#endif

// ===================================================================
// AVX512 KERNEL (16 floats at once)
// ===================================================================
#if defined(__AVX512F__)
#include <immintrin.h>

void djiblas_sgemm_avx512(int m, int n, int k,
                           const float *A, int lda,
                           const float *B, int ldb,
                           float *C, int ldc) {
    // AVX512 implementation would go here
    // For now, fallback to AVX2
    djiblas_sgemm_avx2(m, n, k, A, lda, B, ldb, C, ldc);
}
#else
void djiblas_sgemm_avx512(int m, int n, int k,
                           const float *A, int lda,
                           const float *B, int ldb,
                           float *C, int ldc) {
    djiblas_sgemm_avx2(m, n, k, A, lda, B, ldb, C, ldc);
}
#endif

// ===================================================================
// KERNEL SELECTION
// ===================================================================
sgemm_kernel_t djiblas_get_best_kernel(CPUFeatures *features) {
    if (features->has_avx512f) {
        return djiblas_sgemm_avx512;
    }
    if (features->has_avx2 && features->has_fma) {
        return djiblas_sgemm_avx2;
    }
    if (features->has_sse2) {
        return djiblas_sgemm_sse2;
    }
    return djiblas_sgemm_scalar;
}

// ===================================================================
// PUBLIC API
// ===================================================================
void djiblas_sgemm_f32(int m, int n, int k,
                        const float *A, int lda,
                        const float *B, int ldb,
                        float *C, int ldc) {
    CPUFeatures features;
    djiblas_detect_cpu(&features);
    sgemm_kernel_t kernel = djiblas_get_best_kernel(&features);
    kernel(m, n, k, A, lda, B, ldb, C, ldc);
}
