/*
 * Attention AVX2 helpers (built with -mavx2)
 *
 * Kept in a separate translation unit so the rest of the binary can remain
 * SSE2-safe on CPUs/firmware that can't execute AVX2.
 */

#include <efi.h>
#include <efilib.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>

static inline float hsum256_ps(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    lo = _mm_add_ps(lo, hi);
    // horizontal sum of __m128
    __m128 shuf = _mm_movehdup_ps(lo);
    __m128 sums = _mm_add_ps(lo, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}

float llmk_dot_f32_avx2(const float *a, const float *b, int n) {
    __m256 sum = _mm256_setzero_ps();
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
#if defined(__FMA__)
        sum = _mm256_fmadd_ps(va, vb, sum);
#else
        sum = _mm256_add_ps(sum, _mm256_mul_ps(va, vb));
#endif
    }
    float total = hsum256_ps(sum);
    for (; i < n; i++) total += a[i] * b[i];
    return total;
}

void llmk_axpy_f32_avx2(float *dst, const float *src, float alpha, int n) {
    __m256 a = _mm256_set1_ps(alpha);
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 vd = _mm256_loadu_ps(dst + i);
        __m256 vs = _mm256_loadu_ps(src + i);
#if defined(__FMA__)
        vd = _mm256_fmadd_ps(a, vs, vd);
#else
        vd = _mm256_add_ps(vd, _mm256_mul_ps(a, vs));
#endif
        _mm256_storeu_ps(dst + i, vd);
    }
    for (; i < n; i++) dst[i] += alpha * src[i];
}

#else
float llmk_dot_f32_avx2(const float *a, const float *b, int n) {
    float total = 0.0f;
    for (int i = 0; i < n; i++) total += a[i] * b[i];
    return total;
}

void llmk_axpy_f32_avx2(float *dst, const float *src, float alpha, int n) {
    for (int i = 0; i < n; i++) dst[i] += alpha * src[i];
}
#endif
