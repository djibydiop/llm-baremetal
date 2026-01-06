/*
 * DjibLAS - AVX2/FMA kernels (built with -mavx2 -mfma)
 *
 * This file is compiled separately so the rest of the binary can remain
 * SSE2-safe on CPUs without AVX2.
 */

#include "djiblas.h"

#if defined(__x86_64__) || defined(_M_X64)
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
    // Non-x86 build: fall back to scalar path via SSE2/Scalar in other translation units.
    djiblas_sgemm_sse2(m, n, k, A, lda, B, ldb, C, ldc);
}
#endif
