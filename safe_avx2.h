// safe_avx2.h - AVX2 with automatic SSE2 fallback
// Made in Senegal by Djiby Diop - December 20, 2025

#ifndef SAFE_AVX2_H
#define SAFE_AVX2_H

#include <efi.h>
#include <efilib.h>

// Global flag for AVX2 availability
static BOOLEAN g_avx2_available = FALSE;
static BOOLEAN g_avx2_tested = FALSE;

// Test if AVX2 is truly available and safe
static inline BOOLEAN test_avx2_safe(void) {
    if (g_avx2_tested) {
        return g_avx2_available;
    }
    
    // CPU feature detection
    UINT32 eax, ebx, ecx, edx;
    
    // Check CPUID support
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    
    // Check OSXSAVE (bit 27 of ECX)
    if (!(ecx & (1 << 27))) {
        g_avx2_available = FALSE;
        g_avx2_tested = TRUE;
        return FALSE;
    }
    
    // Check AVX (bit 28 of ECX)
    if (!(ecx & (1 << 28))) {
        g_avx2_available = FALSE;
        g_avx2_tested = TRUE;
        return FALSE;
    }
    
    // Check extended features (leaf 7)
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(7), "c"(0)
    );
    
    // Check AVX2 (bit 5 of EBX)
    if (!(ebx & (1 << 5))) {
        g_avx2_available = FALSE;
        g_avx2_tested = TRUE;
        return FALSE;
    }
    
    // Check XCR0 register
    UINT32 xcr0_lo, xcr0_hi;
    __asm__ volatile (
        "xgetbv"
        : "=a"(xcr0_lo), "=d"(xcr0_hi)
        : "c"(0)
    );
    
    // Check if AVX state is enabled (bits 1-2)
    if ((xcr0_lo & 0x6) != 0x6) {
        g_avx2_available = FALSE;
        g_avx2_tested = TRUE;
        return FALSE;
    }
    
    // All CPUID checks passed - AVX2 is supported
    // Do NOT execute AVX2 instructions for testing - CPUID is sufficient
    // Testing with vmovaps would crash on systems without AVX2
    g_avx2_available = TRUE;
    g_avx2_tested = TRUE;
    return g_avx2_available;
}

// Safe matmul with automatic fallback
static inline void safe_matmul(float* out, const float* a, const float* b, 
                                int m, int n, int k) {
    if (test_avx2_safe()) {
        // Use AVX2 optimized version
        // TODO: Implement AVX2 matmul
        // For now, fallback to scalar
    }
    
    // SSE2/Scalar fallback (always works)
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int l = 0; l < k; l++) {
                sum += a[i * k + l] * b[l * n + j];
            }
            out[i * n + j] = sum;
        }
    }
}

// Safe vector dot product
static inline float safe_dot(const float* a, const float* b, int n) {
    float sum = 0.0f;
    
    if (test_avx2_safe() && n >= 8) {
        // AVX2 path (8 floats at once)
        int i = 0;
        __asm__ volatile ("vzeroupper" ::: "ymm0", "ymm1", "ymm2");
        
        for (; i <= n - 8; i += 8) {
            __asm__ volatile (
                "vmovups (%0), %%ymm0\n\t"
                "vmovups (%1), %%ymm1\n\t"
                "vmulps %%ymm1, %%ymm0, %%ymm0\n\t"
                "vextractf128 $1, %%ymm0, %%xmm1\n\t"
                "vaddps %%xmm1, %%xmm0, %%xmm0\n\t"
                "vhaddps %%xmm0, %%xmm0, %%xmm0\n\t"
                "vhaddps %%xmm0, %%xmm0, %%xmm0\n\t"
                "vaddss %%xmm0, %2, %2"
                : "+r"(a), "+r"(b), "+x"(sum)
                :
                : "ymm0", "ymm1", "memory"
            );
            a += 8;
            b += 8;
        }
        
        __asm__ volatile ("vzeroupper" ::: "ymm0", "ymm1");
        
        // Remaining elements
        for (; i < n; i++) {
            sum += a[i] * b[i];
        }
    } else {
        // SSE2/Scalar fallback
        for (int i = 0; i < n; i++) {
            sum += a[i] * b[i];
        }
    }
    
    return sum;
}

// Print AVX2 status
static inline void print_avx2_status(void) {
    if (test_avx2_safe()) {
        Print(L"  🚀 AVX2: ENABLED (High Performance)\r\n");
    } else {
        Print(L"  ⚠️  AVX2: DISABLED (Fallback to SSE2/Scalar)\r\n");
    }
}

#endif // SAFE_AVX2_H
