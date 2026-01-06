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

static inline UINT64 xgetbv0(void) {
#if defined(__x86_64__) || defined(_M_X64)
    UINT32 eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return ((UINT64)edx << 32) | (UINT64)eax;
#else
    return 0;
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

    // AVX requires OSXSAVE + XCR0 enabling XMM/YMM state.
    BOOLEAN osxsave = (ecx & (1 << 27)) != 0;
    BOOLEAN avx_hw = (ecx & (1 << 28)) != 0;
    BOOLEAN fma_hw = (ecx & (1 << 12)) != 0;
    if (osxsave && avx_hw) {
        UINT64 xcr0 = xgetbv0();
        // XMM (bit 1) and YMM (bit 2) must be enabled.
        if ((xcr0 & 0x6ULL) == 0x6ULL) {
            features->has_avx = TRUE;
            // Only report FMA if AVX state is usable.
            features->has_fma = fma_hw;
        }
    }
    
    // CPUID leaf 7: AVX2, AVX512
    cpuid(7, &eax, &ebx, &ecx, &edx);
    // AVX2 only makes sense if AVX state is enabled.
    if (features->has_avx) {
        features->has_avx2 = (ebx & (1 << 5)) != 0;        // AVX2
    }
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

// AVX2 implementation is in djiblas_avx2.c (compiled with -mavx2 -mfma).

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
