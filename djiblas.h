/*
 * DjibLAS - Optimized Matrix Multiplication for Bare Metal
 * High-performance SGEMM kernels for x86 AVX2/AVX512 and ARM NEON
 * 
 * Created by Djiby Diop - Made in Senegal 🇸🇳
 * December 2025
 */

#ifndef DJIBLAS_H
#define DJIBLAS_H

#include <efi.h>
#include <efilib.h>

// Some UEFI/QEMU setups may fault on CPUID (#UD).
// Define DJIBLAS_DISABLE_CPUID=1 to skip runtime detection and use a safe baseline.
#ifndef DJIBLAS_DISABLE_CPUID
#define DJIBLAS_DISABLE_CPUID 0
#endif

// CPU feature detection
typedef struct {
    BOOLEAN has_sse2;
    BOOLEAN has_avx;
    BOOLEAN has_avx2;
    BOOLEAN has_fma;
    BOOLEAN has_avx512f;
    BOOLEAN has_avx512_vnni;
} CPUFeatures;

// Detect CPU capabilities via CPUID
void djiblas_detect_cpu(CPUFeatures *features);

// Optimized matrix multiplication: C = A^T * B
// A: m x k (transposed)
// B: k x n
// C: m x n (output)
void djiblas_sgemm_f32(
    int m, int n, int k,
    const float *A, int lda,  // A is transposed
    const float *B, int ldb,
    float *C, int ldc
);

// Quantized 8-bit matrix multiplication
void djiblas_sgemm_q8(
    int m, int n, int k,
    const char *A, int lda,
    const char *B, int ldb,
    float *C, int ldc
);

// Kernel selection based on CPU features
typedef void (*sgemm_kernel_t)(int m, int n, int k,
                                const float *A, int lda,
                                const float *B, int ldb,
                                float *C, int ldc);

// Get best kernel for current CPU
sgemm_kernel_t djiblas_get_best_kernel(CPUFeatures *features);

// Individual kernel implementations
void djiblas_sgemm_avx2(int m, int n, int k, const float *A, int lda, const float *B, int ldb, float *C, int ldc);
void djiblas_sgemm_avx512(int m, int n, int k, const float *A, int lda, const float *B, int ldb, float *C, int ldc);
void djiblas_sgemm_sse2(int m, int n, int k, const float *A, int lda, const float *B, int ldb, float *C, int ldc);
void djiblas_sgemm_scalar(int m, int n, int k, const float *A, int lda, const float *B, int ldb, float *C, int ldc);

#endif // DJIBLAS_H
