// REPL V3 - Full Interactive Chat Loop
// Type "quit" or "exit" to stop

#include <efi.h>
#include <efilib.h>
#include <stdint.h>

// GOP types + GUID are provided by gnu-efi headers (efiprot.h / efilib.h).

#if defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h>
#endif

// djiblas optimized matmul
#define DJIBLAS_DISABLE_CPUID 0
#include "djiblas.h"

// LLM-Kernel primitives (zones + sentinel + post-mortem log)
#include "llmk_zones.h"
#include "llmk_log.h"
#include "llmk_sentinel.h"

// DjibMark - Omnipresent execution tracing (Made in Senegal 🇸🇳)
#include "djibmark.h"

// Model config
#define DIM 288
#define HIDDEN_DIM 768
#define N_LAYERS 6
#define N_HEADS 6
#define N_KV_HEADS 6
#define VOCAB_SIZE 32000
#define SEQ_LEN 256
#define MAX_TOKENS 256

// Token ids used by this tiny tokenizer export.
// NOTE: encode() currently inserts BOS=1.
#define TOKEN_BOS 1
#define TOKEN_EOS 2

static int has_suffix_repeat(const int* tokens, int n_tokens, int span) {
    if (span <= 0) return 0;
    if (n_tokens < 2 * span) return 0;
    for (int i = 0; i < span; i++) {
        if (tokens[n_tokens - span + i] != tokens[n_tokens - 2 * span + i]) return 0;
    }
    return 1;
}

// AVX2 attention helpers live in attention_avx2.c (compiled with -mavx2)
float llmk_dot_f32_avx2(const float *a, const float *b, int n);
void llmk_axpy_f32_avx2(float *dst, const float *src, float alpha, int n);

static int g_attn_use_avx2 = 0;
// -1=auto, 0=force SSE2, 1=force AVX2 (only allowed if auto-detected AVX2 is enabled)
static int g_attn_force = -1;

// One-shot fail-safe test harness.
static int g_test_failsafe_active = 0;
static BOOLEAN g_test_failsafe_prev_strict_budget = FALSE;
static UINT64 g_test_failsafe_prev_prefill = 0;
static UINT64 g_test_failsafe_prev_decode = 0;

static void uefi_print_utf8_decode(const unsigned char *p, int len) {
    if (!p || len <= 0) return;

    // Convert UTF-8 bytes to UTF-16 and stream to the UEFI console.
    // Uses U+FFFD replacement on invalid sequences.
    CHAR16 out[256];
    int out_len = 0;

    int i = 0;
    while (i < len) {
        UINT32 cp = 0xFFFD;
        unsigned char b0 = p[i];

        if (b0 < 0x80) {
            cp = (UINT32)b0;
            i += 1;
        } else if ((b0 & 0xE0) == 0xC0) {
            if (i + 1 < len) {
                unsigned char b1 = p[i + 1];
                if ((b1 & 0xC0) == 0x80) {
                    cp = ((UINT32)(b0 & 0x1F) << 6) | (UINT32)(b1 & 0x3F);
                    if (cp < 0x80) cp = 0xFFFD;
                    i += 2;
                } else {
                    i += 1;
                }
            } else {
                i += 1;
            }
        } else if ((b0 & 0xF0) == 0xE0) {
            if (i + 2 < len) {
                unsigned char b1 = p[i + 1];
                unsigned char b2 = p[i + 2];
                if (((b1 & 0xC0) == 0x80) && ((b2 & 0xC0) == 0x80)) {
                    cp = ((UINT32)(b0 & 0x0F) << 12) | ((UINT32)(b1 & 0x3F) << 6) | (UINT32)(b2 & 0x3F);
                    if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD;
                    i += 3;
                } else {
                    i += 1;
                }
            } else {
                i += 1;
            }
        } else if ((b0 & 0xF8) == 0xF0) {
            if (i + 3 < len) {
                unsigned char b1 = p[i + 1];
                unsigned char b2 = p[i + 2];
                unsigned char b3 = p[i + 3];
                if (((b1 & 0xC0) == 0x80) && ((b2 & 0xC0) == 0x80) && ((b3 & 0xC0) == 0x80)) {
                    cp = ((UINT32)(b0 & 0x07) << 18) | ((UINT32)(b1 & 0x3F) << 12) | ((UINT32)(b2 & 0x3F) << 6) | (UINT32)(b3 & 0x3F);
                    if (cp < 0x10000 || cp > 0x10FFFF) cp = 0xFFFD;
                    i += 4;
                } else {
                    i += 1;
                }
            } else {
                i += 1;
            }
        } else {
            i += 1;
        }

        if (out_len > (int)(sizeof(out) / sizeof(out[0])) - 3) {
            out[out_len] = 0;
            uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, out);
            out_len = 0;
        }

        if (cp <= 0xFFFF) {
            out[out_len++] = (CHAR16)cp;
        } else {
            cp -= 0x10000;
            out[out_len++] = (CHAR16)(0xD800 + (cp >> 10));
            out[out_len++] = (CHAR16)(0xDC00 + (cp & 0x3FF));
        }
    }

    if (out_len > 0) {
        out[out_len] = 0;
        uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, out);
    }
}

// Some generations still contain the classic mojibake sequence "ÔÇÖ" for U+2019.
// This can span token boundaries, so keep a small byte tail and repair across calls.
static unsigned char g_utf8_repair_tail[5];
static int g_utf8_repair_tail_len = 0;

static void uefi_print_utf8_bytes(const char *bytes, int len) {
    if (!bytes || len <= 0) return;

    typedef struct {
        unsigned char pat[6];
        unsigned char rep[3];
    } Mojimap;

    // Common mojibake seen in generations (CP437-ish smart punctuation).
    // Each pat is UTF-8 for the visible mojibake string; rep is UTF-8 for the intended punctuation.
    static const Mojimap maps[] = {
        // ÔÇÖ -> ’
        { { 0xC3, 0x94, 0xC3, 0x87, 0xC3, 0x96 }, { 0xE2, 0x80, 0x99 } },
        // ÔÇ£ -> “
        { { 0xC3, 0x94, 0xC3, 0x87, 0xC2, 0xA3 }, { 0xE2, 0x80, 0x9C } },
        // ÔÇØ -> ”
        { { 0xC3, 0x94, 0xC3, 0x87, 0xC3, 0x98 }, { 0xE2, 0x80, 0x9D } },
        // ÔÇö -> —
        { { 0xC3, 0x94, 0xC3, 0x87, 0xC3, 0xB6 }, { 0xE2, 0x80, 0x94 } },
        // ÔÇª -> …
        { { 0xC3, 0x94, 0xC3, 0x87, 0xC2, 0xAA }, { 0xE2, 0x80, 0xA6 } },
    };

    const int keep = 5; // pat_len - 1
    unsigned char inbuf[512];
    unsigned char outbuf[512];

    int offset = 0;
    while (offset < len) {
        int inlen = 0;
        for (int i = 0; i < g_utf8_repair_tail_len && inlen < (int)sizeof(inbuf); i++) {
            inbuf[inlen++] = g_utf8_repair_tail[i];
        }

        int cap = (int)sizeof(inbuf) - inlen;
        int take = len - offset;
        if (take > cap) take = cap;
        for (int i = 0; i < take; i++) {
            inbuf[inlen++] = (unsigned char)bytes[offset + i];
        }
        offset += take;

        if (inlen <= 0) return;

        if (inlen <= keep) {
            g_utf8_repair_tail_len = inlen;
            for (int i = 0; i < inlen; i++) g_utf8_repair_tail[i] = inbuf[i];
            continue;
        }

        int upto = inlen - keep;
        int outlen = 0;
        int j = 0;
        while (j < upto && outlen < (int)sizeof(outbuf)) {
            int matched = 0;
            if (j + 6 <= upto) {
                for (UINTN m = 0; m < (sizeof(maps) / sizeof(maps[0])); m++) {
                    const Mojimap *mm = &maps[m];
                    if (inbuf[j + 0] == mm->pat[0] && inbuf[j + 1] == mm->pat[1] && inbuf[j + 2] == mm->pat[2] &&
                        inbuf[j + 3] == mm->pat[3] && inbuf[j + 4] == mm->pat[4] && inbuf[j + 5] == mm->pat[5]) {
                        if (outlen + 3 <= (int)sizeof(outbuf)) {
                            outbuf[outlen++] = mm->rep[0];
                            outbuf[outlen++] = mm->rep[1];
                            outbuf[outlen++] = mm->rep[2];
                        }
                        j += 6;
                        matched = 1;
                        break;
                    }
                }
            }
            if (matched) continue;
            outbuf[outlen++] = inbuf[j++];
        }

        // Save tail for boundary-spanning repair.
        g_utf8_repair_tail_len = keep;
        for (int i = 0; i < keep; i++) g_utf8_repair_tail[i] = inbuf[upto + i];

        // Decode+print processed bytes.
        uefi_print_utf8_decode(outbuf, outlen);

        // If we ever filled the buffer before consuming all of upto, drop the remainder to avoid
        // stalling. This should be extremely rare with typical tokenizer pieces.
        // (We intentionally keep this minimal and avoid heap allocations.)
        if (j < upto) {
            // best-effort: continue printing remaining bytes directly (no repair inside this chunk)
            uefi_print_utf8_decode(inbuf + j, upto - j);
        }
    }
}

static void uefi_print_utf8_flush(void) {
    if (g_utf8_repair_tail_len <= 0) return;
    uefi_print_utf8_decode(g_utf8_repair_tail, g_utf8_repair_tail_len);
    g_utf8_repair_tail_len = 0;
}

// Best-effort: enable AVX state (OSXSAVE + XCR0) in UEFI so AVX/AVX2 code can run.
// Without an OS, some firmwares leave XCR0 unset; QEMU/OVMF often does.
static inline void cpuidex_u32(UINT32 leaf, UINT32 subleaf, UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx) {
    UINT32 a, b, c, d;
    __asm__ volatile(
        "cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "a"(leaf), "c"(subleaf)
        : "memory"
    );
    if (eax) *eax = a;
    if (ebx) *ebx = b;
    if (ecx) *ecx = c;
    if (edx) *edx = d;
}

static inline UINT64 read_cr4_u64(void) {
    UINT64 v;
    __asm__ volatile("mov %%cr4, %0" : "=r"(v));
    return v;
}

static inline void write_cr4_u64(UINT64 v) {
    __asm__ volatile("mov %0, %%cr4" :: "r"(v) : "memory");
}

static void enable_avx_best_effort(void) {
    UINT32 eax, ebx, ecx, edx;
    cpuidex_u32(1, 0, &eax, &ebx, &ecx, &edx);
    int has_xsave = (ecx & (1u << 26)) != 0;
    int has_avx_hw = (ecx & (1u << 28)) != 0;
    if (!has_xsave || !has_avx_hw) return;

    // Enable OSXSAVE in CR4 (bit 18).
    UINT64 cr4 = read_cr4_u64();
    if ((cr4 & (1ULL << 18)) == 0) {
        write_cr4_u64(cr4 | (1ULL << 18));
    }

    // Enable x87 (bit0), XMM (bit1), YMM (bit2) state in XCR0.
    UINT32 xcr0_lo, xcr0_hi;
    __asm__ volatile(
        "xgetbv"
        : "=a"(xcr0_lo), "=d"(xcr0_hi)
        : "c"(0)
        : "memory"
    );
    UINT32 new_lo = xcr0_lo | 0x7u;
    if (new_lo != xcr0_lo) {
        __asm__ volatile(
            "xsetbv"
            :: "a"(new_lo), "d"(xcr0_hi), "c"(0)
            : "memory"
        );
    }
}

static void apply_no_repeat_ngram(float* logits, int vocab_size, const int* tokens, int n_tokens, int ngram) {
    if (ngram < 2) return;
    if (n_tokens < ngram - 1) return;

    int prefix_len = ngram - 1;
    int prefix_start = n_tokens - prefix_len;
    int limit = n_tokens - ngram;
    for (int i = 0; i <= limit; i++) {
        int match = 1;
        for (int j = 0; j < prefix_len; j++) {
            if (tokens[i + j] != tokens[prefix_start + j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            int banned = tokens[i + prefix_len];
            if (banned >= 0 && banned < vocab_size) {
                // Large negative value to effectively zero it after softmax.
                logits[banned] = -1.0e9f;
            }
        }
    }
}

static inline float dot_f32_sse2(const float* a, const float* b, int n) {
#if defined(__x86_64__) || defined(_M_X64)
    __m128 sum = _mm_setzero_ps();
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        sum = _mm_add_ps(sum, _mm_mul_ps(va, vb));
    }
    float tmp[4];
    _mm_storeu_ps(tmp, sum);
    float total = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    for (; i < n; i++) total += a[i] * b[i];
    return total;
#else
    float total = 0.0f;
    for (int i = 0; i < n; i++) total += a[i] * b[i];
    return total;
#endif
}

static inline void axpy_f32_sse2(float* dst, const float* src, float a, int n) {
#if defined(__x86_64__) || defined(_M_X64)
    __m128 va = _mm_set1_ps(a);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 vd = _mm_loadu_ps(dst + i);
        __m128 vs = _mm_loadu_ps(src + i);
        vd = _mm_add_ps(vd, _mm_mul_ps(va, vs));
        _mm_storeu_ps(dst + i, vd);
    }
    for (; i < n; i++) dst[i] += a * src[i];
#else
    for (int i = 0; i < n; i++) dst[i] += a * src[i];
#endif
}

static inline float dot_f32_best(const float* a, const float* b, int n) {
    int use_avx2 = g_attn_use_avx2;
    if (g_attn_force == 0) use_avx2 = 0;
    else if (g_attn_force == 1) use_avx2 = 1;
    if (use_avx2) return llmk_dot_f32_avx2(a, b, n);
    return dot_f32_sse2(a, b, n);
}

static inline void axpy_f32_best(float* dst, const float* src, float a, int n) {
    int use_avx2 = g_attn_use_avx2;
    if (g_attn_force == 0) use_avx2 = 0;
    else if (g_attn_force == 1) use_avx2 = 1;
    if (use_avx2) { llmk_axpy_f32_avx2(dst, src, a, n); return; }
    axpy_f32_sse2(dst, src, a, n);
}

// ============================================================================
// HEAP ALLOCATOR
// ============================================================================

static char* heap_base = NULL;
static unsigned long heap_offset = 0;
static unsigned long heap_size = 0;

static LlmkZones g_zones;
static LlmkLog g_llmk_log;
static LlmkSentinel g_sentinel;
static int g_llmk_ready = 0;

// DjibMark global state
DjibMarkState g_djibmark_state = {0};

// Root volume handle (set after OpenVolume). Used for best-effort dumps to files.
static EFI_FILE_HANDLE g_root = NULL;

// GOP framebuffer (best-effort; may be unavailable on headless firmware paths)
static EFI_GRAPHICS_OUTPUT_PROTOCOL *g_gop = NULL;
static UINT32 g_gop_w = 0;
static UINT32 g_gop_h = 0;
static UINT32 g_gop_ppsl = 0;
static EFI_GRAPHICS_PIXEL_FORMAT g_gop_pf = PixelFormatMax;
static EFI_PIXEL_BITMASK g_gop_mask = {0};
static volatile UINT32 *g_gop_fb32 = NULL;

static int llmk_ascii_is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static UINT32 llmk_u32_ctz(UINT32 x) {
    if (x == 0) return 32;
    UINT32 n = 0;
    while ((x & 1U) == 0U) { n++; x >>= 1; }
    return n;
}

static UINT32 llmk_u32_popcount(UINT32 x) {
    UINT32 n = 0;
    while (x) { x &= (x - 1U); n++; }
    return n;
}

static EFI_STATUS llmk_gop_init_best_effort(void) {
    g_gop = NULL;
    g_gop_fb32 = NULL;
    g_gop_w = g_gop_h = g_gop_ppsl = 0;
    g_gop_pf = PixelFormatMax;
    g_gop_mask.RedMask = g_gop_mask.GreenMask = g_gop_mask.BlueMask = g_gop_mask.ReservedMask = 0;

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS st = uefi_call_wrapper(BS->LocateProtocol, 3, &gEfiGraphicsOutputProtocolGuid, NULL, (void **)&gop);
    if (EFI_ERROR(st) || !gop || !gop->Mode || !gop->Mode->Info) {
        return EFI_NOT_FOUND;
    }

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = gop->Mode->Info;
    if (info->PixelFormat == PixelBltOnly) {
        return EFI_UNSUPPORTED;
    }
    if (gop->Mode->FrameBufferBase == 0 || gop->Mode->FrameBufferSize < 4) {
        return EFI_UNSUPPORTED;
    }

    g_gop = gop;
    g_gop_w = info->HorizontalResolution;
    g_gop_h = info->VerticalResolution;
    g_gop_ppsl = info->PixelsPerScanLine;
    g_gop_pf = info->PixelFormat;
    g_gop_mask = info->PixelInformation;
    g_gop_fb32 = (volatile UINT32 *)(UINTN)gop->Mode->FrameBufferBase;

    // Sanity: require 32bpp-like stride.
    UINT64 needed = (UINT64)g_gop_ppsl * (UINT64)g_gop_h * 4ULL;
    if (needed > (UINT64)gop->Mode->FrameBufferSize) {
        // Still allow writes, but clamp later.
    }
    return EFI_SUCCESS;
}

static void llmk_gop_put_pixel(UINT32 x, UINT32 y, UINT8 r, UINT8 g, UINT8 b) {
    if (!g_gop_fb32) return;
    if (x >= g_gop_w || y >= g_gop_h) return;
    UINTN idx = (UINTN)y * (UINTN)g_gop_ppsl + (UINTN)x;

    UINT32 px = 0;
    if (g_gop_pf == PixelBlueGreenRedReserved8BitPerColor) {
        px = ((UINT32)b) | ((UINT32)g << 8) | ((UINT32)r << 16) | (0xFFU << 24);
    } else if (g_gop_pf == PixelRedGreenBlueReserved8BitPerColor) {
        px = ((UINT32)r) | ((UINT32)g << 8) | ((UINT32)b << 16) | (0xFFU << 24);
    } else if (g_gop_pf == PixelBitMask) {
        UINT32 rm = g_gop_mask.RedMask;
        UINT32 gm = g_gop_mask.GreenMask;
        UINT32 bm = g_gop_mask.BlueMask;
        UINT32 rs = llmk_u32_ctz(rm);
        UINT32 gs = llmk_u32_ctz(gm);
        UINT32 bs = llmk_u32_ctz(bm);
        UINT32 rbits = llmk_u32_popcount(rm);
        UINT32 gbits = llmk_u32_popcount(gm);
        UINT32 bbits = llmk_u32_popcount(bm);
        UINT32 rmax = (rbits >= 32) ? 0xFFFFFFFFU : ((1U << rbits) - 1U);
        UINT32 gmax = (gbits >= 32) ? 0xFFFFFFFFU : ((1U << gbits) - 1U);
        UINT32 bmax = (bbits >= 32) ? 0xFFFFFFFFU : ((1U << bbits) - 1U);
        UINT32 rv = (rmax == 0) ? 0 : ((UINT32)r * rmax + 127U) / 255U;
        UINT32 gv = (gmax == 0) ? 0 : ((UINT32)g * gmax + 127U) / 255U;
        UINT32 bv = (bmax == 0) ? 0 : ((UINT32)b * bmax + 127U) / 255U;
        px = ((rv << rs) & rm) | ((gv << gs) & gm) | ((bv << bs) & bm);
    } else {
        return;
    }
    g_gop_fb32[idx] = px;
}

static void llmk_gop_get_pixel(UINT32 x, UINT32 y, UINT8 *out_r, UINT8 *out_g, UINT8 *out_b) {
    if (!out_r || !out_g || !out_b) return;
    *out_r = *out_g = *out_b = 0;
    if (!g_gop_fb32) return;
    if (x >= g_gop_w || y >= g_gop_h) return;
    UINTN idx = (UINTN)y * (UINTN)g_gop_ppsl + (UINTN)x;
    UINT32 px = g_gop_fb32[idx];

    if (g_gop_pf == PixelBlueGreenRedReserved8BitPerColor) {
        *out_b = (UINT8)(px & 0xFFU);
        *out_g = (UINT8)((px >> 8) & 0xFFU);
        *out_r = (UINT8)((px >> 16) & 0xFFU);
    } else if (g_gop_pf == PixelRedGreenBlueReserved8BitPerColor) {
        *out_r = (UINT8)(px & 0xFFU);
        *out_g = (UINT8)((px >> 8) & 0xFFU);
        *out_b = (UINT8)((px >> 16) & 0xFFU);
    } else if (g_gop_pf == PixelBitMask) {
        UINT32 rm = g_gop_mask.RedMask;
        UINT32 gm = g_gop_mask.GreenMask;
        UINT32 bm = g_gop_mask.BlueMask;
        UINT32 rs = llmk_u32_ctz(rm);
        UINT32 gs = llmk_u32_ctz(gm);
        UINT32 bs = llmk_u32_ctz(bm);
        UINT32 rbits = llmk_u32_popcount(rm);
        UINT32 gbits = llmk_u32_popcount(gm);
        UINT32 bbits = llmk_u32_popcount(bm);
        UINT32 rmax = (rbits >= 32) ? 0xFFFFFFFFU : ((1U << rbits) - 1U);
        UINT32 gmax = (gbits >= 32) ? 0xFFFFFFFFU : ((1U << gbits) - 1U);
        UINT32 bmax = (bbits >= 32) ? 0xFFFFFFFFU : ((1U << bbits) - 1U);
        UINT32 rv = (rm == 0) ? 0 : ((px & rm) >> rs);
        UINT32 gv = (gm == 0) ? 0 : ((px & gm) >> gs);
        UINT32 bv = (bm == 0) ? 0 : ((px & bm) >> bs);
        *out_r = (rmax == 0) ? 0 : (UINT8)((rv * 255U) / rmax);
        *out_g = (gmax == 0) ? 0 : (UINT8)((gv * 255U) / gmax);
        *out_b = (bmax == 0) ? 0 : (UINT8)((bv * 255U) / bmax);
    }
}

static void llmk_gop_clear(UINT8 r, UINT8 g, UINT8 b) {
    if (!g_gop_fb32) return;
    for (UINT32 y = 0; y < g_gop_h; y++) {
        for (UINT32 x = 0; x < g_gop_w; x++) {
            llmk_gop_put_pixel(x, y, r, g, b);
        }
    }
}

static void llmk_gop_fill_rect(UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT8 r, UINT8 g, UINT8 b) {
    if (!g_gop_fb32) return;
    if (w == 0 || h == 0) return;
    UINT32 x2 = x + w;
    UINT32 y2 = y + h;
    if (x >= g_gop_w || y >= g_gop_h) return;
    if (x2 > g_gop_w) x2 = g_gop_w;
    if (y2 > g_gop_h) y2 = g_gop_h;
    for (UINT32 yy = y; yy < y2; yy++) {
        for (UINT32 xx = x; xx < x2; xx++) {
            llmk_gop_put_pixel(xx, yy, r, g, b);
        }
    }
}

static const char* llmk_parse_word(const char *s, char *out, int out_cap) {
    if (!s || !out || out_cap <= 0) return s;
    while (*s && llmk_ascii_is_space(*s)) s++;
    int n = 0;
    while (*s && !llmk_ascii_is_space(*s) && *s != ';') {
        if (n + 1 < out_cap) out[n++] = *s;
        s++;
    }
    out[n] = 0;
    return s;
}

static const char* llmk_parse_i32(const char *s, int *out) {
    if (!s || !out) return s;
    while (*s && llmk_ascii_is_space(*s)) s++;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    int v = 0;
    int any = 0;
    while (*s >= '0' && *s <= '9') {
        any = 1;
        v = v * 10 + (int)(*s - '0');
        s++;
    }
    if (!any) {
        *out = 0;
        return NULL;
    }
    *out = v * sign;
    return s;
}

static const char* llmk_skip_to_stmt_end(const char *s) {
    if (!s) return s;
    while (*s && *s != ';') s++;
    if (*s == ';') s++;
    return s;
}

static int llmk_streq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == 0 && *b == 0);
}

static int llmk_render_scene_dsl(const char *dsl) {
    if (!dsl) return 0;
    if (!g_gop_fb32) return 0;

    const char *s = dsl;
    int any = 0;
    while (*s) {
        while (*s && (llmk_ascii_is_space(*s) || *s == ';')) s++;
        if (!*s) break;

        char op[16];
        const char *ns = llmk_parse_word(s, op, (int)sizeof(op));
        if (!ns) return 0;
        s = ns;

        if (llmk_streq(op, "clear")) {
            int r, g, b;
            s = llmk_parse_i32(s, &r); if (!s) return 0;
            s = llmk_parse_i32(s, &g); if (!s) return 0;
            s = llmk_parse_i32(s, &b); if (!s) return 0;
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            llmk_gop_clear((UINT8)r, (UINT8)g, (UINT8)b);
            any = 1;
            s = llmk_skip_to_stmt_end(s);
        } else if (llmk_streq(op, "rect")) {
            int x, y, w, h, r, g, b;
            s = llmk_parse_i32(s, &x); if (!s) return 0;
            s = llmk_parse_i32(s, &y); if (!s) return 0;
            s = llmk_parse_i32(s, &w); if (!s) return 0;
            s = llmk_parse_i32(s, &h); if (!s) return 0;
            s = llmk_parse_i32(s, &r); if (!s) return 0;
            s = llmk_parse_i32(s, &g); if (!s) return 0;
            s = llmk_parse_i32(s, &b); if (!s) return 0;
            if (x < 0) x = 0; if (y < 0) y = 0;
            if (w < 0) w = 0; if (h < 0) h = 0;
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            llmk_gop_fill_rect((UINT32)x, (UINT32)y, (UINT32)w, (UINT32)h, (UINT8)r, (UINT8)g, (UINT8)b);
            any = 1;
            s = llmk_skip_to_stmt_end(s);
        } else if (llmk_streq(op, "pixel")) {
            int x, y, r, g, b;
            s = llmk_parse_i32(s, &x); if (!s) return 0;
            s = llmk_parse_i32(s, &y); if (!s) return 0;
            s = llmk_parse_i32(s, &r); if (!s) return 0;
            s = llmk_parse_i32(s, &g); if (!s) return 0;
            s = llmk_parse_i32(s, &b); if (!s) return 0;
            if (x < 0) x = 0; if (y < 0) y = 0;
            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;
            llmk_gop_put_pixel((UINT32)x, (UINT32)y, (UINT8)r, (UINT8)g, (UINT8)b);
            any = 1;
            s = llmk_skip_to_stmt_end(s);
        } else {
            // Unknown op: skip to ';' to avoid getting stuck.
            s = llmk_skip_to_stmt_end(s);
        }
    }
    return any;
}

static EFI_STATUS llmk_open_binary_file(EFI_FILE_HANDLE *out, const CHAR16 *name) {
    if (!out) return EFI_INVALID_PARAMETER;
    *out = NULL;
    if (!g_root || !name) return EFI_NOT_READY;

    // Best-effort truncate by deleting existing file first.
    EFI_FILE_HANDLE existing = NULL;
    EFI_STATUS st = uefi_call_wrapper(g_root->Open, 5, g_root, &existing, (CHAR16 *)name,
                                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (!EFI_ERROR(st) && existing) {
        uefi_call_wrapper(existing->Delete, 1, existing);
        existing = NULL;
    }

    EFI_FILE_HANDLE f = NULL;
    st = uefi_call_wrapper(g_root->Open, 5, g_root, &f, (CHAR16 *)name,
                           EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(st)) return st;
    uefi_call_wrapper(f->SetPosition, 2, f, 0);
    *out = f;
    return EFI_SUCCESS;
}

static EFI_STATUS llmk_file_write_bytes(EFI_FILE_HANDLE f, const void *buf, UINTN nb) {
    if (!f || (!buf && nb)) return EFI_INVALID_PARAMETER;
    if (nb == 0) return EFI_SUCCESS;
    return uefi_call_wrapper(f->Write, 3, f, &nb, (void *)buf);
}

static int llmk_ascii_append_u32(char *dst, int cap, int pos, UINT32 v) {
    if (!dst || cap <= 0) return pos;
    if (pos < 0) pos = 0;
    if (pos >= cap) return pos;

    char tmp[16];
    int n = 0;
    if (v == 0) {
        tmp[n++] = '0';
    } else {
        while (v && n < (int)sizeof(tmp)) {
            tmp[n++] = (char)('0' + (v % 10U));
            v /= 10U;
        }
    }
    for (int i = n - 1; i >= 0; i--) {
        if (pos + 1 >= cap) break;
        dst[pos++] = tmp[i];
    }
    return pos;
}

// Forward declaration (llmk_save_ppm uses it before definition)
void* simple_alloc(unsigned long bytes);

static EFI_STATUS llmk_save_ppm(const CHAR16 *name) {
    if (!g_gop_fb32) return EFI_NOT_READY;
    if (!name) name = L"llmk-img.ppm";

    EFI_FILE_HANDLE f = NULL;
    EFI_STATUS st = llmk_open_binary_file(&f, name);
    if (EFI_ERROR(st)) return st;

    char header[64];
    int pos = 0;
    header[pos++] = 'P'; header[pos++] = '6'; header[pos++] = '\n';
    pos = llmk_ascii_append_u32(header, (int)sizeof(header), pos, g_gop_w);
    header[pos++] = ' ';
    pos = llmk_ascii_append_u32(header, (int)sizeof(header), pos, g_gop_h);
    header[pos++] = '\n';
    header[pos++] = '2'; header[pos++] = '5'; header[pos++] = '5'; header[pos++] = '\n';

    st = llmk_file_write_bytes(f, header, (UINTN)pos);
    if (EFI_ERROR(st)) {
        uefi_call_wrapper(f->Close, 1, f);
        return st;
    }

    // Row buffer: RGB bytes
    UINTN row_bytes = (UINTN)g_gop_w * 3U;
    UINT8 *row = (UINT8 *)simple_alloc((unsigned long)row_bytes);
    if (!row) {
        uefi_call_wrapper(f->Close, 1, f);
        return EFI_OUT_OF_RESOURCES;
    }

    for (UINT32 y = 0; y < g_gop_h; y++) {
        UINTN off = 0;
        for (UINT32 x = 0; x < g_gop_w; x++) {
            UINT8 r, g, b;
            llmk_gop_get_pixel(x, y, &r, &g, &b);
            row[off++] = r;
            row[off++] = g;
            row[off++] = b;
        }
        st = llmk_file_write_bytes(f, row, row_bytes);
        if (EFI_ERROR(st)) {
            uefi_call_wrapper(f->Close, 1, f);
            return st;
        }
    }

    // Flush-before-close for persistence on real hardware
    uefi_call_wrapper(f->Flush, 1, f);
    uefi_call_wrapper(f->Close, 1, f);
    return EFI_SUCCESS;
}

static UINT64 g_budget_prefill_cycles = 0;
static UINT64 g_budget_decode_cycles = 0;

// Rate-limit budget overrun prints (avoid flooding console).
static UINT32 g_budget_overruns_prefill = 0;
static UINT32 g_budget_overruns_decode = 0;

// Forward decl (used by repl.cfg loader before definition)
static void set_seed(unsigned int seed);

static void llmk_reset_runtime_state(void) {
    // Budgets
    g_budget_prefill_cycles = 0;
    g_budget_decode_cycles = 0;
    g_budget_overruns_prefill = 0;
    g_budget_overruns_decode = 0;

    // Log (best-effort)
    if (g_llmk_log.capacity) {
        g_llmk_log.entries = 0;
        g_llmk_log.write_idx = 0;
    }

    // Sentinel
    g_sentinel.tripped = FALSE;
    g_sentinel.last_error = LLMK_OK;
    g_sentinel.last_reason[0] = 0;

    // UTF-8 repair tail
    uefi_print_utf8_flush();
}

static UINT64 llmk_u64_max(UINT64 a, UINT64 b) { return (a > b) ? a : b; }

static void llmk_budget_update(UINT64 *budget, UINT64 last_dt) {
    // Adaptive budget: target = last_dt * margin, then EMA to smooth.
    // Margin must tolerate pos growth and occasional slowdowns.
    const UINT64 margin = 6ULL;
    UINT64 target = last_dt * margin;
    if (target < 500000ULL) target = 500000ULL;
    if (*budget == 0) {
        *budget = target;
        return;
    }
    UINT64 prev = *budget;
    // If we started from a huge initial budget, snap down quickly once we have a real measurement.
    if (prev > target * 4ULL) {
        *budget = target;
        return;
    }
    // EMA: new = (7/8)*old + (1/8)*target
    *budget = ((*budget * 7ULL) + target) / 8ULL;
    // Never decrease too aggressively; keep at least 80% of previous.
    *budget = llmk_u64_max(*budget, (prev * 4ULL) / 5ULL);
}

static void* llmk_alloc_acts(UINT64 bytes, const CHAR16* tag) {
    if (!g_llmk_ready) return NULL;
    return llmk_sentinel_alloc(&g_sentinel, LLMK_ARENA_ACTIVATIONS, bytes, 16, tag);
}

static void* llmk_alloc_weights(UINT64 bytes, const CHAR16* tag) {
    if (!g_llmk_ready) return NULL;
    return llmk_sentinel_alloc(&g_sentinel, LLMK_ARENA_WEIGHTS, bytes, 64, tag);
}

static void* llmk_alloc_kv(UINT64 bytes, const CHAR16* tag) {
    if (!g_llmk_ready) return NULL;
    return llmk_sentinel_alloc(&g_sentinel, LLMK_ARENA_KV_CACHE, bytes, 64, tag);
}

void* simple_alloc(unsigned long bytes) {
    // Backward-compatible interface: route default allocations into ACTS arena
    // once the kernel allocator is initialized.
    if (g_llmk_ready) {
        return llmk_alloc_acts((UINT64)bytes, L"repl alloc");
    }
    if (heap_offset + bytes > heap_size) return NULL;
    void* ptr = heap_base + heap_offset;
    heap_offset += bytes;
    return ptr;
}

static EFI_STATUS read_exact(EFI_FILE_HANDLE file, void *dst, UINTN total_bytes) {
    UINT8 *p = (UINT8 *)dst;
    UINTN remaining = total_bytes;
    UINTN done = 0;
    UINTN next_report = 0;
    while (remaining > 0) {
        UINTN chunk = remaining;
        // Large reads can fail on some UEFI implementations; keep chunks modest.
        if (chunk > (16U * 1024U * 1024U)) chunk = (16U * 1024U * 1024U);
        UINTN got = chunk;
        EFI_STATUS st = uefi_call_wrapper(file->Read, 3, file, &got, p);
        if (EFI_ERROR(st)) return st;
        if (got == 0) return EFI_LOAD_ERROR;
        p += got;
        done += got;
        if (got > remaining) return EFI_LOAD_ERROR;
        remaining -= got;

        // Progress (avoid spamming): report every 64MB for large reads.
        if (total_bytes >= (128U * 1024U * 1024U)) {
            if (done >= next_report) {
                UINTN mb_done = done / (1024U * 1024U);
                UINTN mb_total = total_bytes / (1024U * 1024U);
                Print(L"  Reading weights... %d / %d MB\r\n", (int)mb_done, (int)mb_total);
                next_report = done + (64U * 1024U * 1024U);
            }
        }
    }
    return EFI_SUCCESS;
}

// ============================================================================
// BEST-EFFORT DUMP TO FILE (UTF-16LE)
// ============================================================================

static EFI_STATUS llmk_open_text_file(EFI_FILE_HANDLE *out, const CHAR16 *name) {
    if (!out) return EFI_INVALID_PARAMETER;
    *out = NULL;
    if (!g_root || !name) return EFI_NOT_READY;

    EFI_FILE_HANDLE f = NULL;
    EFI_STATUS st = uefi_call_wrapper(g_root->Open, 5, g_root, &f, (CHAR16 *)name,
                                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(st)) return st;

    // Truncate to 0 and write BOM.
    uefi_call_wrapper(f->SetPosition, 2, f, 0);
    UINT16 bom = 0xFEFF;
    UINTN nb = sizeof(bom);
    uefi_call_wrapper(f->Write, 3, f, &nb, &bom);

    *out = f;
    return EFI_SUCCESS;
}

static EFI_STATUS llmk_open_read_file(EFI_FILE_HANDLE *out, const CHAR16 *name) {
    if (!out) return EFI_INVALID_PARAMETER;
    *out = NULL;
    if (!g_root || !name) return EFI_NOT_READY;

    EFI_FILE_HANDLE f = NULL;
    EFI_STATUS st = uefi_call_wrapper(g_root->Open, 5, g_root, &f, (CHAR16 *)name, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) return st;
    *out = f;
    return EFI_SUCCESS;
}

static int llmk_cfg_is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

static char llmk_cfg_tolower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static void llmk_cfg_trim(char **s) {
    if (!s || !*s) return;
    char *p = *s;
    while (llmk_cfg_is_space(*p)) p++;
    *s = p;
    char *end = p;
    while (*end) end++;
    while (end > p && llmk_cfg_is_space(end[-1])) end--;
    *end = 0;
}

static int llmk_cfg_streq_ci(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (llmk_cfg_tolower(*a) != llmk_cfg_tolower(*b)) return 0;
        a++;
        b++;
    }
    return (*a == 0 && *b == 0);
}

static int llmk_cfg_parse_u64(const char *s, UINT64 *out) {
    if (!s || !out) return 0;
    UINT64 v = 0;
    int any = 0;
    while (*s && llmk_cfg_is_space(*s)) s++;
    while (*s >= '0' && *s <= '9') {
        any = 1;
        v = v * 10ULL + (UINT64)(*s - '0');
        s++;
    }
    if (!any) return 0;
    *out = v;
    return 1;
}

static int llmk_cfg_parse_i32(const char *s, int *out) {
    if (!s || !out) return 0;
    int sign = 1;
    while (*s && llmk_cfg_is_space(*s)) s++;
    if (*s == '-') { sign = -1; s++; }
    int v = 0;
    int any = 0;
    while (*s >= '0' && *s <= '9') {
        any = 1;
        v = v * 10 + (int)(*s - '0');
        s++;
    }
    if (!any) return 0;
    *out = v * sign;
    return 1;
}

static int llmk_cfg_parse_f32(const char *s, float *out) {
    if (!s || !out) return 0;
    int sign = 1;
    while (*s && llmk_cfg_is_space(*s)) s++;
    if (*s == '-') { sign = -1; s++; }
    float val = 0.0f;
    int any = 0;
    while (*s >= '0' && *s <= '9') {
        any = 1;
        val = val * 10.0f + (float)(*s - '0');
        s++;
    }
    if (*s == '.') {
        s++;
        float frac = 0.1f;
        while (*s >= '0' && *s <= '9') {
            any = 1;
            val += (float)(*s - '0') * frac;
            frac *= 0.1f;
            s++;
        }
    }
    if (!any) return 0;
    *out = val * (float)sign;
    return 1;
}

static int llmk_cfg_parse_bool(const char *s, int *out) {
    if (!s || !out) return 0;
    while (*s && llmk_cfg_is_space(*s)) s++;
    if (llmk_cfg_streq_ci(s, "1") || llmk_cfg_streq_ci(s, "true") || llmk_cfg_streq_ci(s, "on") || llmk_cfg_streq_ci(s, "yes")) {
        *out = 1;
        return 1;
    }
    if (llmk_cfg_streq_ci(s, "0") || llmk_cfg_streq_ci(s, "false") || llmk_cfg_streq_ci(s, "off") || llmk_cfg_streq_ci(s, "no")) {
        *out = 0;
        return 1;
    }
    int iv = 0;
    if (llmk_cfg_parse_i32(s, &iv)) {
        *out = (iv != 0);
        return 1;
    }
    return 0;
}

static void llmk_load_repl_cfg_best_effort(
    float *temperature,
    float *min_p,
    float *top_p,
    int *top_k,
    float *repeat_penalty,
    int *no_repeat_ngram,
    int *max_gen_tokens,
    int *stats_enabled,
    int *stop_on_you,
    int *stop_on_double_nl
) {
    EFI_FILE_HANDLE f = NULL;
    EFI_STATUS st = llmk_open_read_file(&f, L"repl.cfg");
    if (EFI_ERROR(st)) return;

    char buf[4096];
    UINTN sz = sizeof(buf) - 1;
    st = uefi_call_wrapper(f->Read, 3, f, &sz, buf);
    uefi_call_wrapper(f->Close, 1, f);
    if (EFI_ERROR(st) || sz == 0) return;
    buf[sz] = 0;

    int applied = 0;

    char *p = buf;
    while (*p) {
        char *line = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') { *p = 0; p++; }

        // Trim CR and whitespace.
        llmk_cfg_trim(&line);
        if (line[0] == 0) continue;
        if (line[0] == '#' || line[0] == ';') continue;

        // Strip inline comment.
        for (char *c = line; *c; c++) {
            if (*c == '#') { *c = 0; break; }
        }
        llmk_cfg_trim(&line);
        if (line[0] == 0) continue;

        // key=value
        char *eq = line;
        while (*eq && *eq != '=') eq++;
        if (*eq != '=') continue;
        *eq = 0;
        char *key = line;
        char *val = eq + 1;
        llmk_cfg_trim(&key);
        llmk_cfg_trim(&val);
        if (key[0] == 0) continue;

        // Lowercase key in-place (ASCII).
        for (char *k = key; *k; k++) *k = llmk_cfg_tolower(*k);

        if (llmk_cfg_streq_ci(key, "temp") || llmk_cfg_streq_ci(key, "temperature")) {
            float v;
            if (llmk_cfg_parse_f32(val, &v)) {
                if (v < 0.0f) v = 0.0f;
                *temperature = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "min_p")) {
            float v;
            if (llmk_cfg_parse_f32(val, &v)) {
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                *min_p = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "top_p")) {
            float v;
            if (llmk_cfg_parse_f32(val, &v)) {
                if (v < 0.0f) v = 0.0f;
                if (v > 1.0f) v = 1.0f;
                *top_p = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "top_k")) {
            int v;
            if (llmk_cfg_parse_i32(val, &v)) {
                if (v < 0) v = 0;
                if (v > 256) v = 256;
                *top_k = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "repeat") || llmk_cfg_streq_ci(key, "repeat_penalty")) {
            float v;
            if (llmk_cfg_parse_f32(val, &v)) {
                if (v <= 0.0f) v = 1.0f;
                *repeat_penalty = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "norepeat") || llmk_cfg_streq_ci(key, "no_repeat_ngram")) {
            int v;
            if (llmk_cfg_parse_i32(val, &v)) {
                if (v < 0) v = 0;
                if (v > 16) v = 16;
                *no_repeat_ngram = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "max_tokens")) {
            int v;
            if (llmk_cfg_parse_i32(val, &v)) {
                if (v < 1) v = 1;
                if (v > MAX_TOKENS) v = MAX_TOKENS;
                *max_gen_tokens = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "stats")) {
            int b;
            if (llmk_cfg_parse_bool(val, &b)) {
                *stats_enabled = b;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "stop_you")) {
            int b;
            if (llmk_cfg_parse_bool(val, &b)) {
                *stop_on_you = b;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "stop_nl")) {
            int b;
            if (llmk_cfg_parse_bool(val, &b)) {
                *stop_on_double_nl = b;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "seed")) {
            int v;
            if (llmk_cfg_parse_i32(val, &v)) {
                if (v < 0) v = -v;
                set_seed((unsigned int)v);
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "budget")) {
            UINT64 v;
            if (llmk_cfg_parse_u64(val, &v)) {
                g_budget_prefill_cycles = v;
                g_budget_decode_cycles = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "budget_prefill")) {
            UINT64 v;
            if (llmk_cfg_parse_u64(val, &v)) {
                g_budget_prefill_cycles = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "budget_decode")) {
            UINT64 v;
            if (llmk_cfg_parse_u64(val, &v)) {
                g_budget_decode_cycles = v;
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "strict_budget")) {
            int b;
            if (llmk_cfg_parse_bool(val, &b)) {
                g_sentinel.cfg.strict_budget = (b != 0);
                applied = 1;
            }
        } else if (llmk_cfg_streq_ci(key, "attn")) {
            if (llmk_cfg_streq_ci(val, "auto")) {
                g_attn_force = -1;
                applied = 1;
            } else if (llmk_cfg_streq_ci(val, "sse2")) {
                g_attn_force = 0;
                applied = 1;
            } else if (llmk_cfg_streq_ci(val, "avx2")) {
                if (g_attn_use_avx2) {
                    g_attn_force = 1;
                    applied = 1;
                }
            }
        }
    }

    if (applied) {
        Print(L"[cfg] repl.cfg loaded\r\n");
    }
}

static EFI_STATUS llmk_file_write_u16(EFI_FILE_HANDLE f, const CHAR16 *s) {
    if (!f || !s) return EFI_INVALID_PARAMETER;
    UINTN chars = (UINTN)StrLen((CHAR16 *)s);
    UINTN nb = chars * sizeof(CHAR16);
    if (nb == 0) return EFI_SUCCESS;
    return uefi_call_wrapper(f->Write, 3, f, &nb, (void *)s);
}

static EFI_STATUS llmk_dump_zones_to_file(EFI_FILE_HANDLE f, const LlmkZones *zones) {
    if (!f || !zones) return EFI_INVALID_PARAMETER;
    CHAR16 line[256];
    SPrint(line, sizeof(line), L"[llmk] Zone B: base=0x%lx size=%lu MiB\r\n",
           (UINT64)zones->zone_b_base, zones->zone_b_size / (1024ULL * 1024ULL));
    llmk_file_write_u16(f, line);
    for (int i = 0; i < LLMK_ARENA_COUNT; i++) {
        const LlmkArena *a = &zones->arenas[i];
        SPrint(line, sizeof(line), L"  [%s] base=0x%lx size=%lu MiB used=%lu MiB flags=0x%x\r\n",
               a->name,
               a->base,
               a->size / (1024ULL * 1024ULL),
               a->cursor / (1024ULL * 1024ULL),
               (unsigned)a->flags);
        llmk_file_write_u16(f, line);
    }
    llmk_file_write_u16(f, L"\r\n");
    return EFI_SUCCESS;
}

static EFI_STATUS llmk_dump_sentinel_to_file(EFI_FILE_HANDLE f, const LlmkSentinel *s) {
    if (!f || !s) return EFI_INVALID_PARAMETER;
    CHAR16 line[256];
    SPrint(line, sizeof(line), L"[llmk][sentinel] enabled=%d strict=%d max_cycles=%lu last_err=%d reason=%s\r\n\r\n",
           s->cfg.enabled ? 1 : 0,
           s->cfg.strict_mode ? 1 : 0,
           s->cfg.max_cycles,
           (int)s->last_error,
           s->last_reason);
    llmk_file_write_u16(f, line);
    return EFI_SUCCESS;
}

static EFI_STATUS llmk_dump_log_to_file(EFI_FILE_HANDLE f, const LlmkLog *log, UINT32 max_entries) {
    if (!f || !log || !log->entries || log->capacity == 0) return EFI_INVALID_PARAMETER;

    UINT32 n = log->capacity;
    if (max_entries != 0 && max_entries < n) n = max_entries;

    CHAR16 line[256];
    SPrint(line, sizeof(line), L"[llmk][log] last %u events (ring cap=%u)\r\n", n, log->capacity);
    llmk_file_write_u16(f, line);

    UINT32 w = log->write_idx;
    for (UINT32 i = 0; i < n; i++) {
        UINT32 off = (w + log->capacity - 1 - i) % log->capacity;
        const LlmkLogEntry *e = &log->entries[off];
        if (e->tsc == 0 && e->code == 0 && e->msg[0] == 0) continue;
        SPrint(line, sizeof(line), L"  #%u tsc=%lu code=%u arena=%d ptr=0x%lx size=%lu msg=%s\r\n",
               i, e->tsc, e->code, e->arena, e->ptr, e->size, e->msg);
        llmk_file_write_u16(f, line);
    }
    llmk_file_write_u16(f, L"\r\n");
    return EFI_SUCCESS;
}

// ============================================================================
// MATH FUNCTIONS
// ============================================================================

float fast_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float xhalf = 0.5f * x;
    int i = *(int*)&x;
    i = 0x5f3759df - (i >> 1);
    x = *(float*)&i;
    x = x * (1.5f - xhalf * x * x);
    x = x * (1.5f - xhalf * x * x);
    return 1.0f / x;
}

float fast_exp(float x) {
    if (x < -10.0f) return 0.0f;
    if (x > 10.0f) return 22026.0f;
    x = 1.0f + x / 256.0f;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x; x *= x; x *= x;
    return x;
}

int my_strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == 0) return 0;
    }
    return 0;
}

// ============================================================================
// TRANSFORMER OPERATIONS
// ============================================================================

void rmsnorm(float* o, float* x, float* weight, int size) {
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / fast_sqrt(ss);
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void matmul(float* xout, float* x, float* w, int n, int d) {
    // DjibLAS computes (column-major): C(m×n) = A(k×m)^T · B(k×n)
    // We want (row-major weights): xout(d) = W(d×n) · x(n)
    // Trick: W(d×n) row-major has the same memory layout as B(k×n_out)
    // column-major when k=n and n_out=d (because W[i*n + l] == B[l + k*i]).
    // Use A = x as a (k×1) column-major matrix.
    // Result C is (1×d) column-major, so it lands contiguous into xout.
    djiblas_sgemm_f32(
        /*m=*/1, /*n=*/d, /*k=*/n,
        /*A=*/x, /*lda=*/n,
        /*B=*/w, /*ldb=*/n,
        /*C=*/xout, /*ldc=*/1
    );
}

void softmax(float* x, int size) {
    float max_val = x[0];
#if defined(__x86_64__) || defined(_M_X64)
    // SSE2 max reduction
    {
        __m128 vmax = _mm_set1_ps(max_val);
        int i = 0;
        for (; i + 4 <= size; i += 4) {
            __m128 v = _mm_loadu_ps(&x[i]);
            vmax = _mm_max_ps(vmax, v);
        }
        __m128 shuf = _mm_shuffle_ps(vmax, vmax, _MM_SHUFFLE(2, 3, 0, 1));
        vmax = _mm_max_ps(vmax, shuf);
        shuf = _mm_shuffle_ps(vmax, vmax, _MM_SHUFFLE(1, 0, 3, 2));
        vmax = _mm_max_ps(vmax, shuf);
        _mm_store_ss(&max_val, vmax);
        for (; i < size; i++) {
            if (x[i] > max_val) max_val = x[i];
        }
    }
#else
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
#endif

    float sum = 0.0f;
#if defined(__x86_64__) || defined(_M_X64)
    // Scalar exp, but vectorized accumulation + normalization.
    {
        __m128 vsum = _mm_setzero_ps();
        int i = 0;
        for (; i + 4 <= size; i += 4) {
            float e0 = fast_exp(x[i + 0] - max_val);
            float e1 = fast_exp(x[i + 1] - max_val);
            float e2 = fast_exp(x[i + 2] - max_val);
            float e3 = fast_exp(x[i + 3] - max_val);
            x[i + 0] = e0;
            x[i + 1] = e1;
            x[i + 2] = e2;
            x[i + 3] = e3;
            __m128 v = _mm_loadu_ps(&x[i]);
            vsum = _mm_add_ps(vsum, v);
        }
        __m128 shuf = _mm_shuffle_ps(vsum, vsum, _MM_SHUFFLE(2, 3, 0, 1));
        vsum = _mm_add_ps(vsum, shuf);
        shuf = _mm_shuffle_ps(vsum, vsum, _MM_SHUFFLE(1, 0, 3, 2));
        vsum = _mm_add_ps(vsum, shuf);
        _mm_store_ss(&sum, vsum);
        for (; i < size; i++) {
            x[i] = fast_exp(x[i] - max_val);
            sum += x[i];
        }

        float invsum = 1.0f / sum;
        __m128 vinv = _mm_set1_ps(invsum);
        i = 0;
        for (; i + 4 <= size; i += 4) {
            __m128 v = _mm_loadu_ps(&x[i]);
            v = _mm_mul_ps(v, vinv);
            _mm_storeu_ps(&x[i], v);
        }
        for (; i < size; i++) {
            x[i] *= invsum;
        }
    }
#else
    for (int i = 0; i < size; i++) {
        x[i] = fast_exp(x[i] - max_val);
        sum += x[i];
    }
    float invsum = 1.0f / sum;
    for (int i = 0; i < size; i++) {
        x[i] *= invsum;
    }
#endif
}

// ============================================================================
// STRUCTURES
// ============================================================================

typedef struct {
    int dim;
    int hidden_dim;
    int n_layers;
    int n_heads;
    int n_kv_heads;
    int vocab_size;
    int seq_len;
} Config;

static void llmk_print_ctx(const Config *config,
                   const CHAR16 *model_name,
                   int kv_pos,
                   float temperature,
                   float min_p,
                   float top_p,
                   int top_k,
                   int no_repeat_ngram,
                   float repeat_penalty,
                   int max_gen_tokens) {
    Print(L"\r\nCTX\r\n");
    Print(L"  model=%s\r\n", model_name ? model_name : L"(unknown)");
    Print(L"  dim=%d layers=%d heads=%d kv=%d vocab=%d\r\n",
        config->dim, config->n_layers, config->n_heads, config->n_kv_heads, config->vocab_size);
    Print(L"  seq_len=%d kv_pos=%d\r\n", config->seq_len, kv_pos);
    Print(L"  sample: temp=%d.%02d min_p=%d.%02d top_p=%d.%02d top_k=%d\r\n",
        (int)temperature, (int)((temperature - (int)temperature) * 100.0f),
        (int)min_p, (int)((min_p - (int)min_p) * 100.0f),
        (int)top_p, (int)((top_p - (int)top_p) * 100.0f),
        top_k);
    Print(L"          norepeat=%d repeat=%d.%02d max_tokens=%d\r\n",
        no_repeat_ngram,
        (int)repeat_penalty, (int)((repeat_penalty - (int)repeat_penalty) * 100.0f),
        max_gen_tokens);
    if (g_llmk_ready) {
      Print(L"  budget: prefill=%lu decode=%lu strict=%d overruns(p=%d d=%d)\r\n",
          g_budget_prefill_cycles, g_budget_decode_cycles,
          (int)g_sentinel.cfg.strict_budget,
          (int)g_budget_overruns_prefill, (int)g_budget_overruns_decode);
    }
    Print(L"\r\n");
}

static void llmk_print_log(UINT32 n) {
    if (n == 0) n = 16;
    if (n > 128) n = 128;
    Print(L"\r\nLog (last %d):\r\n", (int)n);
    if (g_llmk_ready && g_llmk_log.capacity) {
        llmk_log_dump(&g_llmk_log, n);
    } else {
        Print(L"  (log not available)\r\n");
    }
    Print(L"\r\n");
}

typedef struct {
    float* token_embedding_table;
    float* rms_att_weight;
    float* wq;
    float* wk;
    float* wv;
    float* wo;
    float* rms_ffn_weight;
    float* w1;
    float* w2;
    float* w3;
    float* rms_final_weight;
    float* wcls;
} TransformerWeights;

typedef struct {
    float* x;
    float* xb;
    float* xb2;
    float* hb;
    float* hb2;
    float* q;
    float* k;
    float* v;
    float* att;
    float* logits;
    float* key_cache;
    float* value_cache;
} RunState;

typedef struct {
    char** vocab;
    float* vocab_scores;
    int vocab_size;
    int max_token_length;
} Tokenizer;

// ============================================================================
// FORWARD PASS
// ============================================================================

void transformer_forward(RunState* s, TransformerWeights* w, Config* p, int token, int pos) {
    // DjibMark: record entry into transformer (prefill vs decode determined by caller)
    if (pos == 0) {
        DJIBMARK_PREFILL();
    } else {
        DJIBMARK_DECODE();
    }
    
    int dim = p->dim;
    int hidden_dim = p->hidden_dim;
    int n_layers = p->n_layers;
    int n_heads = p->n_heads;
    int head_size = dim / n_heads;
    int kv_dim = (dim * p->n_kv_heads) / n_heads;
    int kv_mul = n_heads / p->n_kv_heads;
    
    // Copy embedding
    float* content_row = w->token_embedding_table + token * dim;
    for (int i = 0; i < dim; i++) {
        s->x[i] = content_row[i];
    }
    
    // Forward all layers
    for (int l = 0; l < n_layers; l++) {
        // Attention RMSNorm
        rmsnorm(s->xb, s->x, w->rms_att_weight + l*dim, dim);
        
        // Q, K, V matrices
        matmul(s->q, s->xb, w->wq + l*dim*dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l*dim*kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l*dim*kv_dim, dim, kv_dim);
        
        // Store in KV cache
        int loff = l * p->seq_len * kv_dim;
        float* key_cache_row = s->key_cache + loff + pos * kv_dim;
        float* value_cache_row = s->value_cache + loff + pos * kv_dim;
        for (int i = 0; i < kv_dim; i++) {
            key_cache_row[i] = s->k[i];
            value_cache_row[i] = s->v[i];
        }
        
        // Multihead attention
        for (int h = 0; h < n_heads; h++) {
            float* q_h = s->q + h * head_size;
            int att_offset = h * p->seq_len;
            float inv_scale = 1.0f / fast_sqrt((float)head_size);
            
            // Attention scores
            for (int t = 0; t <= pos; t++) {
                float* k_t = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float score = dot_f32_best(q_h, k_t, head_size) * inv_scale;
                s->att[att_offset + t] = score;
            }
            
            // Softmax
            softmax(s->att + att_offset, pos + 1);
            
            // Weighted sum
            float* xb_h = s->xb + h * head_size;
            for (int i = 0; i < head_size; i++) xb_h[i] = 0.0f;
            
            for (int t = 0; t <= pos; t++) {
                float* v_t = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float a = s->att[att_offset + t];
                axpy_f32_best(xb_h, v_t, a, head_size);
            }
        }
        
        // Output projection
        matmul(s->xb2, s->xb, w->wo + l*dim*dim, dim, dim);
        
        // Residual
        for (int i = 0; i < dim; i++) {
            s->x[i] += s->xb2[i];
        }
        
        // FFN RMSNorm
        rmsnorm(s->xb, s->x, w->rms_ffn_weight + l*dim, dim);
        
        // FFN
        matmul(s->hb, s->xb, w->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l*dim*hidden_dim, dim, hidden_dim);
        
        // SwiGLU
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            val *= (1.0f / (1.0f + fast_exp(-val)));
            s->hb[i] = val * s->hb2[i];
        }
        
        matmul(s->xb, s->hb, w->w2 + l*dim*hidden_dim, hidden_dim, dim);
        
        // Residual
        for (int i = 0; i < dim; i++) {
            s->x[i] += s->xb[i];
        }
    }
    
    // Final RMSNorm
    rmsnorm(s->x, s->x, w->rms_final_weight, dim);
    
    // Classifier
    matmul(s->logits, s->x, w->wcls, dim, p->vocab_size);
}

// Simple PRNG for sampling
static unsigned int g_seed = 1234567;

static void set_seed(unsigned int seed) {
    // Avoid a zero seed getting stuck in some LCGs.
    if (seed == 0) seed = 1;
    g_seed = seed;
}

static unsigned long long rdtsc(void) {
    unsigned int lo, hi;
    // Serialize via LFENCE to reduce reordering noise.
    __asm__ __volatile__("lfence\nrdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return ((unsigned long long)hi << 32) | lo;
}

// 0 means "unavailable / calibration failed".
static unsigned long long tsc_per_sec = 0;

// Best-effort wall-clock microsecond timestamp using UEFI GetTime.
// Returns 1 on success, 0 on failure.
static int uefi_wall_us(unsigned long long *out_us) {
    if (!out_us) return 0;
    if (!ST || !ST->RuntimeServices || !ST->RuntimeServices->GetTime) return 0;
    EFI_TIME t;
    EFI_STATUS st = uefi_call_wrapper(ST->RuntimeServices->GetTime, 2, &t, NULL);
    if (EFI_ERROR(st)) return 0;
    // Seconds-of-day is sufficient for short deltas (we handle midnight wrap).
    unsigned long long sod = (unsigned long long)t.Hour * 3600ULL + (unsigned long long)t.Minute * 60ULL + (unsigned long long)t.Second;
    unsigned long long us = sod * 1000000ULL;
    // Nanosecond is defined by EFI_TIME; firmware may provide 0.
    us += ((unsigned long long)t.Nanosecond) / 1000ULL;
    *out_us = us;
    return 1;
}

static void calibrate_tsc_once(void) {
    if (tsc_per_sec != 0) return;
    // Use UEFI Stall (microseconds) to estimate TSC frequency.
    // 500ms gives decent accuracy even on coarse/slow TSC emulation.
    unsigned long long t0 = rdtsc();
    uefi_call_wrapper(BS->Stall, 1, 500000);
    unsigned long long t1 = rdtsc();
    unsigned long long dt = (t1 > t0) ? (t1 - t0) : 0;
    // If dt is implausibly small, treat as unavailable.
    if (dt < 1000ULL) {
        tsc_per_sec = 0;
        return;
    }
    // 500ms -> multiply by 2 to get cycles/sec.
    tsc_per_sec = dt * 2ULL;
}

static float randf(void) {
    g_seed = g_seed * 1664525 + 1013904223;
    return (float)(g_seed >> 8) / 16777216.0f;
}

// Sample with temperature + min_p + top-p + top-k + repetition penalty
int sample_advanced(float* logits, int n, float temperature, float min_p, float top_p, int top_k,
                    int* recent_tokens, int n_recent, float repeat_penalty) {
    // Apply repetition penalty
    if (repeat_penalty != 1.0f && n_recent > 0) {
        for (int i = 0; i < n_recent; i++) {
            int tok = recent_tokens[i];
            if (tok >= 0 && tok < n) {
                if (logits[tok] > 0) {
                    logits[tok] /= repeat_penalty;
                } else {
                    logits[tok] *= repeat_penalty;
                }
            }
        }
    }
    
    // Greedy if temp=0
    if (temperature <= 0.0f) {
        int max_i = 0;
        float max_val = logits[0];
        for (int i = 1; i < n; i++) {
            if (logits[i] > max_val) {
                max_val = logits[i];
                max_i = i;
            }
        }
        return max_i;
    }
    
    // Apply temperature
    for (int i = 0; i < n; i++) {
        logits[i] /= temperature;
    }
    
    // Softmax
    float max_val = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > max_val) max_val = logits[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        logits[i] = fast_exp(logits[i] - max_val);
        sum += logits[i];
    }
    for (int i = 0; i < n; i++) {
        logits[i] /= sum;
    }

    // Min-p filtering (relative to max probability)
    if (min_p > 0.0f) {
        float max_p = 0.0f;
        for (int i = 0; i < n; i++) {
            if (logits[i] > max_p) max_p = logits[i];
        }
        float thresh = min_p * max_p;
        float new_sum = 0.0f;
        for (int i = 0; i < n; i++) {
            if (logits[i] < thresh) {
                logits[i] = 0.0f;
            }
            new_sum += logits[i];
        }
        if (new_sum > 0.0f) {
            for (int i = 0; i < n; i++) {
                logits[i] /= new_sum;
            }
        }
    }
    
    // Top-k / Top-p sampling
    {
        // IMPORTANT: vocab is 32k; do NOT full-sort.
        // We maintain a small descending top-list.
        #define MAX_TOP_K 256
        static int top_idx[MAX_TOP_K];
        static float top_prob[MAX_TOP_K];
        int k = top_k;
        if (k < 0) k = 0;
        if (k > MAX_TOP_K) k = MAX_TOP_K;
        if (k == 0 || k > n) k = (n < MAX_TOP_K) ? n : MAX_TOP_K;

        int top_count = 0;
        for (int i = 0; i < n; i++) {
            float p = logits[i];
            if (top_count < k) {
                int j = top_count;
                while (j > 0 && top_prob[j - 1] < p) {
                    top_prob[j] = top_prob[j - 1];
                    top_idx[j] = top_idx[j - 1];
                    j--;
                }
                top_prob[j] = p;
                top_idx[j] = i;
                top_count++;
            } else if (p > top_prob[top_count - 1]) {
                int j = top_count - 1;
                while (j > 0 && top_prob[j - 1] < p) {
                    top_prob[j] = top_prob[j - 1];
                    top_idx[j] = top_idx[j - 1];
                    j--;
                }
                top_prob[j] = p;
                top_idx[j] = i;
            }
        }

        // If both are effectively "disabled" (top_p>=1 and top_k<=0), fall through to full sampling.
        if (top_k > 0 || top_p < 1.0f) {
            float mass = 0.0f;
            int cutoff = 0;
            for (int i = 0; i < top_count; i++) {
                mass += top_prob[i];
                cutoff++;
                if (top_p < 1.0f && mass >= top_p) break;
            }
            if (cutoff < 1) cutoff = 1;

            float r = randf() * mass;
            float cdf = 0.0f;
            for (int i = 0; i < cutoff; i++) {
                cdf += top_prob[i];
                if (r < cdf) {
                    return top_idx[i];
                }
            }
            return top_idx[cutoff - 1];
        }
        #undef MAX_TOP_K
    }
    
    // Sample from distribution
    float r = randf();
    float cumsum = 0.0f;
    for (int i = 0; i < n; i++) {
        cumsum += logits[i];
        if (r < cumsum) {
            return i;
        }
    }
    
    return n - 1;
}

int sample(float* logits, int n) {
    // Simple greedy for now (kept for compatibility)
    int max_i = 0;
    float max_val = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > max_val) {
            max_val = logits[i];
            max_i = i;
        }
    }
    return max_i;
}

// ============================================================================
// TOKENIZER
// ============================================================================

static int my_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int my_strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

int str_lookup(char* str, char** vocab, int vocab_size) {
    for (int i = 0; i < vocab_size; i++) {
        if (vocab[i] && my_strcmp(str, vocab[i]) == 0) {
            return i;
        }
    }
    return -1;
}

void encode(char* text, int* tokens, int* n_tokens, int max_tokens, Tokenizer* t) {
    *n_tokens = 0;
    if (max_tokens <= 0) return;

    // Add BOS
    tokens[(*n_tokens)++] = TOKEN_BOS;
    if (*n_tokens >= max_tokens) return;

    // Greedy longest-match encoding
    char* str = text;
    while (*str && *n_tokens < max_tokens) {
        int best_id = -1;
        int best_len = 0;

        for (int len = 64; len > 0; len--) {
            char piece[65];
            int i = 0;
            for (i = 0; i < len && str[i]; i++) {
                piece[i] = str[i];
            }
            if (i != len) continue; // not enough chars remaining
            piece[i] = '\0';

            int id = str_lookup(piece, t->vocab, t->vocab_size);
            if (id >= 0) {
                best_id = id;
                best_len = len;
                break;
            }
        }

        if (best_id >= 0) {
            if (*n_tokens >= max_tokens) break;
            tokens[(*n_tokens)++] = best_id;
            str += best_len;
        } else {
            char single[2];
            single[0] = *str;
            single[1] = '\0';
            int id = str_lookup(single, t->vocab, t->vocab_size);
            if (id >= 0) {
                if (*n_tokens >= max_tokens) break;
                tokens[(*n_tokens)++] = id;
            }
            str++;
        }
    }
}

// ============================================================================
// KEYBOARD INPUT
// ============================================================================

void read_user_input(CHAR16* buffer, int max_len) {
    int pos = 0;
    EFI_INPUT_KEY Key;
    int line_start = 0;
    
    while (pos < max_len - 1) {
        // Wait for key
        UINTN index;
        uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &index);
        uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
        
        if (Key.UnicodeChar == 0x000D) {  // Enter
            // If the user ends the line with "\\\\", treat it as a literal "\\" and do NOT continue.
            if (pos >= 2 && buffer[pos - 2] == L'\\' && buffer[pos - 1] == L'\\') {
                pos -= 1;
                buffer[pos - 1] = L'\\';
            } else {
                // Multi-line continuation: if the current line ends with '\\', continue.
                if (pos > 0 && buffer[pos - 1] == L'\\') {
                    buffer[pos - 1] = L'\n';  // Replace \ with newline
                    Print(L"\r\n... ");
                    line_start = pos;
                    continue;
                }
            }

            // Multi-line terminator: line is exactly ";;" on its own line.
            if ((pos - line_start) == 2 && buffer[line_start] == L';' && buffer[line_start + 1] == L';') {
                // Remove terminator line and the preceding newline if present.
                if (line_start > 0 && buffer[line_start - 1] == L'\n') {
                    pos = line_start - 1;
                } else {
                    pos = line_start;
                }
                buffer[pos] = 0;
                Print(L"\r\n");
                break;
            }
            buffer[pos] = 0;
            Print(L"\r\n");
            break;
        } else if (Key.UnicodeChar == 0x0008) {  // Backspace
            if (pos > line_start) {
                pos--;
                Print(L"\b \b");
            }
        } else if (Key.UnicodeChar >= 32 && Key.UnicodeChar < 127) {
            buffer[pos++] = Key.UnicodeChar;
            Print(L"%c", Key.UnicodeChar);
        }
    }
    
    buffer[pos] = 0;
}

void char16_to_char(char* dest, CHAR16* src, int max_len) {
    int i;
    for (i = 0; i < max_len - 1 && src[i]; i++) {
        dest[i] = (char)src[i];
    }
    dest[i] = 0;
}

static void ascii_to_char16(CHAR16 *dst, const char *src, int max_len) {
    if (!dst || max_len <= 0) return;
    int i = 0;
    if (!src) {
        dst[0] = 0;
        return;
    }
    for (; i < max_len - 1 && src[i]; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x20 || c > 0x7E) {
            dst[i] = L'_';
        } else {
            dst[i] = (CHAR16)c;
        }
    }
    dst[i] = 0;
}

int check_quit_command(char* text) {
    // Check for "quit" or "exit"
    if (my_strcmp(text, "quit") == 0 || my_strcmp(text, "exit") == 0) {
        return 1;
    }
    return 0;
}

void reset_kv_cache(RunState* s, Config* p) {
    // Clear KV cache for new conversation
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int cache_size = p->n_layers * p->seq_len * kv_dim;
    
    for (int i = 0; i < cache_size; i++) {
        s->key_cache[i] = 0.0f;
        s->value_cache[i] = 0.0f;
    }
}

// ============================================================================
// MAIN
// ============================================================================

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    // Initialize DjibMark tracing system (Made in Senegal 🇸🇳)
    djibmark_init();
    DJIBMARK_BOOT();

    // Disable the UEFI watchdog timer (large model loads can take minutes).
    // If not disabled, firmware may reset/reboot mid-load and it looks like a hang.
    uefi_call_wrapper(BS->SetWatchdogTimer, 4, 0, 0, 0, NULL);
    
    Print(L"\r\n");
    Print(L"----------------------------------------\r\n");
    Print(L"  LLAMA2 CHAT REPL V3 - Full Loop\r\n");
    Print(L"----------------------------------------\r\n\r\n");
    
    // ========================================================================
    // [1/7] File System
    // ========================================================================
    
    Print(L"[1/7] Opening file system...\r\n");
    
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_STATUS status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &LoadedImageProtocol, &LoadedImage);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: LoadedImage protocol failed\r\n");
        return status;
    }
    
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &FileSystemProtocol, &FileSystem);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: FileSystem protocol failed\r\n");
        return status;
    }
    
    EFI_FILE_HANDLE Root;
    status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: OpenVolume failed\r\n");
        return status;
    }

    // Persist root handle for best-effort dumps.
    g_root = Root;
    
    Print(L"OK: File system ready\r\n\r\n");

    // Best-effort enable AVX/AVX2 state before feature detection.
    enable_avx_best_effort();

    // CPU feature detection (djiblas)
    {
        CPUFeatures cpu_features;
        djiblas_detect_cpu(&cpu_features);
        sgemm_kernel_t k = djiblas_get_best_kernel(&cpu_features);
        const CHAR16 *name = L"SCALAR";
        if (k == djiblas_sgemm_avx512) name = L"AVX512";
        else if (k == djiblas_sgemm_avx2) name = (cpu_features.has_fma ? L"AVX2+FMA" : L"AVX2");
        else if (k == djiblas_sgemm_sse2) name = L"SSE2";
        Print(L"[DJIBLAS] SGEMM kernel: %s (sse2=%d avx=%d avx2=%d fma=%d)\r\n\r\n",
              name,
              (int)cpu_features.has_sse2,
              (int)cpu_features.has_avx,
              (int)cpu_features.has_avx2,
              (int)cpu_features.has_fma);

          // Attention SIMD dispatch: only use AVX2 if firmware/OS state supports it.
          g_attn_use_avx2 = (cpu_features.has_avx2 && cpu_features.has_avx);
          Print(L"[ATTN] SIMD path: %s\r\n\r\n", g_attn_use_avx2 ? L"AVX2" : L"SSE2");
    }

    // Best-effort graphics init (GOP). Optional: REPL still works without it.
    {
        EFI_STATUS gst = llmk_gop_init_best_effort();
        if (!EFI_ERROR(gst)) {
            Print(L"[GOP] Framebuffer ready: %dx%d (ppsl=%d)\r\n\r\n", (int)g_gop_w, (int)g_gop_h, (int)g_gop_ppsl);
        } else {
            Print(L"[GOP] Not available (%r)\r\n\r\n", gst);
        }
    }
    
    // ========================================================================
    // [2/7] Load Model Header
    // ========================================================================
    
    Print(L"[2/7] Loading model...\r\n");
    
    EFI_FILE_HANDLE ModelFile;
    CHAR16 *model_filename = NULL;
    {
        // Try larger models first when present. Keep the list small and explicit
        // (UEFI shell users can rename the file to match one of these).
        CHAR16 *candidates[] = {
            L"stories300M.bin",
            L"stories260M.bin",
            L"stories200M.bin",
            L"stories110M.bin",
            L"stories15M.bin",
            L"model.bin",
        };
        const int n_candidates = (int)(sizeof(candidates) / sizeof(candidates[0]));
        EFI_STATUS last = EFI_NOT_FOUND;
        for (int i = 0; i < n_candidates; i++) {
            EFI_FILE_HANDLE f = 0;
            EFI_STATUS st = uefi_call_wrapper(Root->Open, 5, Root, &f, candidates[i], EFI_FILE_MODE_READ, 0);
            if (!EFI_ERROR(st)) {
                ModelFile = f;
                model_filename = candidates[i];
                status = st;
                break;
            }
            last = st;
        }
        if (model_filename == NULL) {
            Print(L"ERROR: Model file not found. Expected one of: stories300M.bin stories260M.bin stories200M.bin stories110M.bin stories15M.bin model.bin\r\n");
            return last;
        }
    }
    
    Config config;
    UINTN bytes_to_read = 7 * sizeof(int);
    uefi_call_wrapper(ModelFile->Read, 3, ModelFile, &bytes_to_read, &config);
    
    // In llama2.c format, a negative vocab_size indicates shared classifier weights.
    int shared_classifier = (config.vocab_size < 0);
    if (config.vocab_size < 0) config.vocab_size = -config.vocab_size;

    // Some exported model files may *still* share classifier weights even if vocab_size is positive.
    // Detect this by comparing expected weights size vs actual file size.
    UINT64 model_file_size = 0;
    {
        EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
        UINTN info_size = 0;
        EFI_STATUS st = uefi_call_wrapper(ModelFile->GetInfo, 4, ModelFile, &FileInfoGuid, &info_size, NULL);
        if (st == EFI_BUFFER_TOO_SMALL && info_size > 0) {
            EFI_FILE_INFO *info = NULL;
            st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, info_size, (void **)&info);
            if (!EFI_ERROR(st) && info) {
                st = uefi_call_wrapper(ModelFile->GetInfo, 4, ModelFile, &FileInfoGuid, &info_size, info);
                if (!EFI_ERROR(st)) {
                    model_file_size = info->FileSize;
                }
                uefi_call_wrapper(BS->FreePool, 1, info);
            }
        }
    }
    
        Print(L"OK: Model loaded: %s (dim=%d, layers=%d, heads=%d, kv=%d, vocab=%d, seq=%d)\r\n\r\n",
                    model_filename, config.dim, config.n_layers, config.n_heads, config.n_kv_heads, config.vocab_size, config.seq_len);

    // ========================================================================
    // [3/7] Kernel zones + heap (auto-sized)
    // ========================================================================

    int kv_dim = (config.dim * config.n_kv_heads) / config.n_heads;
    int head_size = config.dim / config.n_heads;

    // Compute total weights size (floats)
    UINTN n_floats_base = 0;
    n_floats_base += (UINTN)config.vocab_size * (UINTN)config.dim;                   // token_embedding_table
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim;                     // rms_att_weight
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim; // wq
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;     // wk
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;     // wv
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim; // wo
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim;                     // rms_ffn_weight
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim; // w1
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.hidden_dim * (UINTN)config.dim; // w2
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim; // w3
    n_floats_base += (UINTN)config.dim;                                              // rms_final_weight
    n_floats_base += (UINTN)config.seq_len * (UINTN)head_size / 2;                   // freq_cis_real
    n_floats_base += (UINTN)config.seq_len * (UINTN)head_size / 2;                   // freq_cis_imag

    UINTN n_floats_with_cls = n_floats_base + (UINTN)config.vocab_size * (UINTN)config.dim;

    // If file size is known, use it to infer whether wcls is present.
    if (model_file_size > 0) {
        UINT64 available = model_file_size;
        UINT64 header_bytes = (UINT64)(7 * sizeof(int));
        if (available > header_bytes) available -= header_bytes;
        UINT64 bytes_base = (UINT64)n_floats_base * sizeof(float);
        UINT64 bytes_with = (UINT64)n_floats_with_cls * sizeof(float);

        if (available < bytes_with && available >= bytes_base) {
            shared_classifier = 1;
        } else if (available >= bytes_with) {
            shared_classifier = 0;
        }
    }

    UINTN n_floats = shared_classifier ? n_floats_base : n_floats_with_cls;
    UINTN weights_bytes = n_floats * sizeof(float);
    UINTN state_bytes = 0;
    state_bytes += (UINTN)config.dim * sizeof(float) * 3; // x, xb, xb2
    state_bytes += (UINTN)config.hidden_dim * sizeof(float) * 2; // hb, hb2
    state_bytes += (UINTN)config.dim * sizeof(float); // q
    state_bytes += (UINTN)kv_dim * sizeof(float) * 2; // k, v
    state_bytes += (UINTN)config.n_heads * (UINTN)config.seq_len * sizeof(float); // att
    state_bytes += (UINTN)config.vocab_size * sizeof(float); // logits
    state_bytes += (UINTN)config.n_layers * (UINTN)config.seq_len * (UINTN)kv_dim * sizeof(float) * 2; // key/value cache

    // Tokenizer: pointers + scores + strings (strings size varies; reserve a safe budget)
    UINTN tokenizer_bytes = (UINTN)config.vocab_size * (sizeof(char*) + sizeof(float));
    tokenizer_bytes += 4 * 1024 * 1024; // string storage budget

    UINTN slack_bytes = 16 * 1024 * 1024;
    heap_size = weights_bytes + state_bytes + tokenizer_bytes + slack_bytes;
    if (heap_size < 100ULL * 1024ULL * 1024ULL) heap_size = 100ULL * 1024ULL * 1024ULL;

    // Initialize LLM-Kernel Zone B arenas sized from the same accounting.
    // This makes the REPL and the kernel work together: all big allocations go through zones/sentinel.
    {
        UINT64 zonec_bytes = 8ULL * 1024ULL * 1024ULL;
        UINT64 scratch_bytes = 32ULL * 1024ULL * 1024ULL;

        // KV cache lives in its own arena.
        UINT64 kv_bytes = (UINT64)config.n_layers * (UINT64)config.seq_len * (UINT64)kv_dim * sizeof(float) * 2ULL;

        UINT64 weights_u64 = (UINT64)weights_bytes;
        UINT64 acts_u64 = (UINT64)(state_bytes - (UINTN)kv_bytes) + (UINT64)tokenizer_bytes + (UINT64)slack_bytes;

        // Total Zone B includes all arenas.
        UINT64 total = weights_u64 + kv_bytes + scratch_bytes + acts_u64 + zonec_bytes;
        // Min 1GB for larger models; fallback to 768MB if allocation fails.
        UINT64 min_total = (total > 768ULL * 1024ULL * 1024ULL) ? (1024ULL * 1024ULL * 1024ULL) : (768ULL * 1024ULL * 1024ULL);
        if (total < min_total) total = min_total;

        LlmkZonesConfig zcfg;
        zcfg.total_bytes = total;
        zcfg.weights_bytes = weights_u64;
        zcfg.kv_bytes = kv_bytes;
        zcfg.scratch_bytes = scratch_bytes;
        zcfg.activations_bytes = acts_u64;
        zcfg.zone_c_bytes = zonec_bytes;

        Print(L"[3/7] Init kernel zones (%d MB)...\r\n", (int)(total / (1024 * 1024)));
        status = llmk_zones_init(BS, &zcfg, &g_zones);
        if (EFI_ERROR(status) && total > min_total) {
            // If the computed size can't be allocated (e.g. low guest RAM / fragmentation),
            // fall back to a smaller default so the REPL can still boot with smaller models.
            Print(L"[llmk] zones alloc failed, retrying with %d MB...\r\n", (int)(min_total / (1024 * 1024)));
            zcfg.total_bytes = min_total;
            zcfg.weights_bytes = 0;
            zcfg.kv_bytes = 0;
            zcfg.scratch_bytes = 0;
            zcfg.activations_bytes = 0;
            zcfg.zone_c_bytes = 0;
            status = llmk_zones_init(BS, &zcfg, &g_zones);
        }
        if (EFI_ERROR(status)) {
            Print(L"ERROR: llmk_zones_init failed: %r\r\n", status);
            return status;
        }

        // Init Zone C log (best-effort)
        EFI_STATUS logst = llmk_log_init(&g_zones, &g_llmk_log);
        if (EFI_ERROR(logst)) {
            g_llmk_log.entries = 0;
            g_llmk_log.capacity = 0;
            g_llmk_log.write_idx = 0;
        }

        // Init sentinel
        LlmkSentinelConfig scfg;
        scfg.enabled = TRUE;
        // REPL: keep allocation failures fatal, but keep budget overruns non-fatal.
        // This lets us "activate budgets" without killing the whole session.
        scfg.strict_mode = FALSE;
        scfg.strict_alloc = TRUE;
        scfg.strict_budget = FALSE;
        scfg.max_cycles = 0;
        scfg.max_cycles_prefill = 0;
        scfg.max_cycles_decode = 0;
        scfg.log_violations = TRUE;

        status = llmk_sentinel_init(&g_sentinel, &g_zones, (g_llmk_log.capacity ? &g_llmk_log : 0), &scfg);
        if (EFI_ERROR(status)) {
            Print(L"ERROR: llmk_sentinel_init failed: %r\r\n", status);
            return status;
        }

        g_llmk_ready = 1;
        llmk_zones_print(&g_zones);
        llmk_sentinel_print_status(&g_sentinel);
        Print(L"OK: Kernel allocator ready\r\n\r\n");
    }
    
    // ========================================================================
    // [4/7] Weight Pointers
    // ========================================================================
    
    Print(L"[4/7] Mapping weights...\r\n");
    bytes_to_read = weights_bytes;
    float* weights_mem = (float*)llmk_alloc_weights((UINT64)bytes_to_read, L"weights");
    if (weights_mem == NULL) {
        Print(L"ERROR: Out of heap while allocating weights (%d MB needed)\r\n", (int)(bytes_to_read / (1024 * 1024)));
        return EFI_OUT_OF_RESOURCES;
    }
    status = read_exact(ModelFile, weights_mem, bytes_to_read);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to read weights (need model file + enough RAM).\r\n");
        return EFI_LOAD_ERROR;
    }

    float* weights_ptr = weights_mem;

    TransformerWeights weights;
    weights.token_embedding_table = weights_ptr;
    weights_ptr += config.vocab_size * config.dim;
    
    weights.rms_att_weight = weights_ptr;
    weights_ptr += config.n_layers * config.dim;
    
    weights.wq = weights_ptr;
    weights_ptr += config.n_layers * config.dim * config.dim;
    
    weights.wk = weights_ptr;
    weights_ptr += config.n_layers * config.dim * kv_dim;
    
    weights.wv = weights_ptr;
    weights_ptr += config.n_layers * config.dim * kv_dim;
    
    weights.wo = weights_ptr;
    weights_ptr += config.n_layers * config.dim * config.dim;
    
    weights.rms_ffn_weight = weights_ptr;
    weights_ptr += config.n_layers * config.dim;
    
    weights.w1 = weights_ptr;
    weights_ptr += config.n_layers * config.dim * config.hidden_dim;
    
    weights.w2 = weights_ptr;
    weights_ptr += config.n_layers * config.hidden_dim * config.dim;
    
    weights.w3 = weights_ptr;
    weights_ptr += config.n_layers * config.dim * config.hidden_dim;
    
    weights.rms_final_weight = weights_ptr;
    weights_ptr += config.dim;
    
    // Skip freq_cis_real and freq_cis_imag (RoPE precomputed freqs)
    weights_ptr += config.seq_len * head_size / 2;  // freq_cis_real
    weights_ptr += config.seq_len * head_size / 2;  // freq_cis_imag
    
    weights.wcls = shared_classifier ? weights.token_embedding_table : weights_ptr;
    
    uefi_call_wrapper(ModelFile->Close, 1, ModelFile);
    
    Print(L"OK: Weights mapped\r\n\r\n");
    
    // ========================================================================
    // [5/7] State Buffers
    // ========================================================================
    
    Print(L"[5/7] Allocating state buffers...\r\n");
    
    RunState state;
    
    state.x = (float*)simple_alloc(config.dim * sizeof(float));
    state.xb = (float*)simple_alloc(config.dim * sizeof(float));
    state.xb2 = (float*)simple_alloc(config.dim * sizeof(float));
    state.hb = (float*)simple_alloc(config.hidden_dim * sizeof(float));
    state.hb2 = (float*)simple_alloc(config.hidden_dim * sizeof(float));
    state.q = (float*)simple_alloc(config.dim * sizeof(float));
    state.k = (float*)simple_alloc(kv_dim * sizeof(float));
    state.v = (float*)simple_alloc(kv_dim * sizeof(float));
    state.att = (float*)simple_alloc(config.n_heads * config.seq_len * sizeof(float));
    state.logits = (float*)simple_alloc(config.vocab_size * sizeof(float));
    state.key_cache = (float*)llmk_alloc_kv((UINT64)config.n_layers * (UINT64)config.seq_len * (UINT64)kv_dim * sizeof(float), L"key cache");
    state.value_cache = (float*)llmk_alloc_kv((UINT64)config.n_layers * (UINT64)config.seq_len * (UINT64)kv_dim * sizeof(float), L"value cache");
    
    Print(L"OK: State buffers allocated\r\n\r\n");
    
    // ========================================================================
    // [6/7] Tokenizer
    // ========================================================================
    
    Print(L"[6/7] Loading tokenizer...\r\n");
    
    EFI_FILE_HANDLE TokFile;
    status = uefi_call_wrapper(Root->Open, 5, Root, &TokFile, L"tokenizer.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Tokenizer file not found\r\n");
        return status;
    }
    
    Tokenizer tokenizer;
    bytes_to_read = sizeof(int);
    uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, &tokenizer.max_token_length);
    
    tokenizer.vocab_size = config.vocab_size;
    tokenizer.vocab = (char**)simple_alloc(config.vocab_size * sizeof(char*));
    tokenizer.vocab_scores = (float*)simple_alloc(config.vocab_size * sizeof(float));
    
    for (int i = 0; i < config.vocab_size; i++) {
        bytes_to_read = sizeof(float);
        uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, &tokenizer.vocab_scores[i]);
        
        int len;
        bytes_to_read = sizeof(int);
        uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, &len);
        
        tokenizer.vocab[i] = (char*)simple_alloc(len + 1);
        bytes_to_read = len;
        uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, tokenizer.vocab[i]);
        tokenizer.vocab[i][len] = '\0';
    }
    
    uefi_call_wrapper(TokFile->Close, 1, TokFile);
    
    Print(L"OK: Tokenizer loaded (%d tokens)\r\n\r\n", tokenizer.vocab_size);
    
    // ========================================================================
    // [7/7] Interactive REPL Loop
    // ========================================================================
    
    Print(L"[7/7] Entering chat loop...\r\n\r\n");
    
    Print(L"----------------------------------------\r\n");
    Print(L"  CHAT MODE ACTIVE\r\n");
    Print(L"  Type 'quit' or 'exit' to stop\r\n");
    Print(L"  Multi-line: end line with '\\' to continue; ';;' alone submits\r\n");
    Print(L"  Commands: /temp /min_p /top_p /top_k /norepeat /repeat /max_tokens /seed /stats /stop_you /stop_nl /model /cpu /zones /budget /attn /test_failsafe /ctx /log /save_log /save_dump /gop /render /save_img /reset /version /help\r\n");
    Print(L"----------------------------------------\r\n\r\n");
    
    // Sampling parameters
    // Default sampling tuned for TinyStories (less looping, still creative).
    float temperature = 0.85f;
    float min_p = 0.05f;
    float top_p = 0.95f;
    int top_k = 80;
    float repeat_penalty = 1.15f;
    int no_repeat_ngram = 4;
    int max_gen_tokens = 160;
    int stats_enabled = 1;
    int stop_on_you = 1;
    int stop_on_double_nl = 0;

    // Optional config: repl.cfg (key=value). Best-effort; ignored if missing.
    llmk_load_repl_cfg_best_effort(
        &temperature,
        &min_p,
        &top_p,
        &top_k,
        &repeat_penalty,
        &no_repeat_ngram,
        &max_gen_tokens,
        &stats_enabled,
        &stop_on_you,
        &stop_on_double_nl
    );
    
    int conversation_count = 0;
    
    // KV cache position tracking (persistent across prompts for context retention)
    int kv_pos = 0;
    
    // MAIN LOOP
    while (1) {
        conversation_count++;
        
        // Read user input
        CHAR16 user_input[512];
        Print(L"You: ");
        read_user_input(user_input, 512);
        
        // Convert to char
        char prompt[512];
        char16_to_char(prompt, user_input, 512);
        
        // Check for quit
        if (check_quit_command(prompt)) {
            Print(L"\r\n");
            Print(L"----------------------------------------\r\n");
            Print(L"  Goodbye! Had %d conversations.\r\n", conversation_count - 1);
            Print(L"----------------------------------------\r\n\r\n");
            break;
        }
        
        // Check for commands
        if (prompt[0] == '/') {
            if (my_strncmp(prompt, "/temp ", 6) == 0) {
                float val = 0.0f;
                int i = 6;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10.0f + (prompt[i] - '0');
                    i++;
                }
                if (prompt[i] == '.') {
                    i++;
                    float frac = 0.1f;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        val += (prompt[i] - '0') * frac;
                        frac /= 10.0f;
                        i++;
                    }
                }
                temperature = val;
                Print(L"  Temperature set to: ");
                Print(L"%d.", (int)temperature);
                Print(L"%d\r\n", (int)((temperature - (int)temperature) * 100.0f));
                continue;
            } else if (my_strncmp(prompt, "/min_p ", 7) == 0) {
                float val = 0.0f;
                int i = 7;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10.0f + (prompt[i] - '0');
                    i++;
                }
                if (prompt[i] == '.') {
                    i++;
                    float frac = 0.1f;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        val += (prompt[i] - '0') * frac;
                        frac /= 10.0f;
                        i++;
                    }
                }
                if (val < 0.0f) val = 0.0f;
                if (val > 1.0f) val = 1.0f;
                min_p = val;
                Print(L"  Min-p set to: ");
                Print(L"%d.", (int)min_p);
                Print(L"%d\r\n", (int)((min_p - (int)min_p) * 100.0f));
                continue;
            } else if (my_strncmp(prompt, "/top_p ", 7) == 0) {
                float val = 0.0f;
                int i = 7;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10.0f + (prompt[i] - '0');
                    i++;
                }
                if (prompt[i] == '.') {
                    i++;
                    float frac = 0.1f;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        val += (prompt[i] - '0') * frac;
                        frac /= 10.0f;
                        i++;
                    }
                }
                top_p = val;
                Print(L"  Top-p set to: ");
                Print(L"%d.", (int)top_p);
                Print(L"%d\r\n", (int)((top_p - (int)top_p) * 100.0f));
                continue;
            } else if (my_strncmp(prompt, "/top_k ", 7) == 0) {
                int val = 0;
                int i = 7;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10 + (prompt[i] - '0');
                    i++;
                }
                if (val < 0) val = 0;
                if (val > 256) val = 256;
                top_k = val;
                Print(L"  Top-k set to: %d\r\n", top_k);
                continue;
            } else if (my_strncmp(prompt, "/max_tokens ", 12) == 0) {
                int val = 0;
                int i = 12;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10 + (prompt[i] - '0');
                    i++;
                }
                if (val < 1) val = 1;
                if (val > MAX_TOKENS) val = MAX_TOKENS;
                max_gen_tokens = val;
                Print(L"  Max tokens set to: %d\r\n", max_gen_tokens);
                continue;
            } else if (my_strncmp(prompt, "/seed ", 6) == 0) {
                unsigned int val = 0;
                int i = 6;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10u + (unsigned int)(prompt[i] - '0');
                    i++;
                }
                set_seed(val);
                Print(L"  Seed set to: %d\r\n", (int)g_seed);
                continue;
            } else if (my_strncmp(prompt, "/stats ", 7) == 0) {
                int val = 0;
                int i = 7;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10 + (prompt[i] - '0');
                    i++;
                }
                stats_enabled = (val != 0);
                Print(L"  Stats: %s\r\n", stats_enabled ? L"on" : L"off");
                continue;
            } else if (my_strncmp(prompt, "/stop_you ", 10) == 0) {
                int val = 0;
                int i = 10;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10 + (prompt[i] - '0');
                    i++;
                }
                stop_on_you = (val != 0);
                Print(L"  Stop on \\nYou:: %s\r\n", stop_on_you ? L"on" : L"off");
                continue;
            } else if (my_strncmp(prompt, "/stop_nl ", 9) == 0) {
                int val = 0;
                int i = 9;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10 + (prompt[i] - '0');
                    i++;
                }
                stop_on_double_nl = (val != 0);
                Print(L"  Stop on double newline: %s\r\n", stop_on_double_nl ? L"on" : L"off");
                continue;
            } else if (my_strncmp(prompt, "/norepeat ", 10) == 0) {
                int val = 0;
                int i = 10;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10 + (prompt[i] - '0');
                    i++;
                }
                if (val < 0) val = 0;
                if (val > 16) val = 16;
                no_repeat_ngram = val;
                Print(L"  No-repeat ngram set to: %d\r\n", no_repeat_ngram);
                continue;
            } else if (my_strncmp(prompt, "/repeat ", 8) == 0) {
                float val = 0.0f;
                int i = 8;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10.0f + (prompt[i] - '0');
                    i++;
                }
                if (prompt[i] == '.') {
                    i++;
                    float frac = 0.1f;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        val += (prompt[i] - '0') * frac;
                        frac /= 10.0f;
                        i++;
                    }
                }
                repeat_penalty = val;
                Print(L"  Repetition penalty set to: ");
                Print(L"%d.", (int)repeat_penalty);
                Print(L"%d\r\n", (int)((repeat_penalty - (int)repeat_penalty) * 100.0f));
                continue;
            } else if (my_strncmp(prompt, "/model", 6) == 0) {
                Print(L"\r\nModel:\r\n");
                Print(L"  stories110M.bin\r\n");
                Print(L"Config:\r\n");
                Print(L"  dim=%d layers=%d heads=%d kv=%d vocab=%d seq=%d\r\n\r\n",
                      config.dim, config.n_layers, config.n_heads, config.n_kv_heads, config.vocab_size, config.seq_len);
                continue;
            } else if (my_strncmp(prompt, "/cpu", 4) == 0) {
                CPUFeatures f;
                djiblas_detect_cpu(&f);
                sgemm_kernel_t k = djiblas_get_best_kernel(&f);
                const CHAR16 *name = L"SCALAR";
                if (k == djiblas_sgemm_avx512) name = L"AVX512";
                else if (k == djiblas_sgemm_avx2) name = (f.has_fma ? L"AVX2+FMA" : L"AVX2");
                else if (k == djiblas_sgemm_sse2) name = L"SSE2";
                Print(L"\r\nCPU features:\r\n");
                Print(L"  sse2=%d avx=%d avx2=%d fma=%d\r\n", (int)f.has_sse2, (int)f.has_avx, (int)f.has_avx2, (int)f.has_fma);
                Print(L"  djiblas_sgemm=%s\r\n", name);
                const CHAR16 *attn = g_attn_use_avx2 ? L"AVX2" : L"SSE2";
                if (g_attn_force == 0) attn = L"SSE2 (forced)";
                else if (g_attn_force == 1) attn = L"AVX2 (forced)";
                Print(L"  attn_simd=%s\r\n\r\n", attn);
                continue;
            } else if (my_strncmp(prompt, "/zones", 6) == 0) {
                Print(L"\r\nZones:\r\n");
                if (g_llmk_ready) {
                    llmk_zones_print(&g_zones);
                    llmk_sentinel_print_status(&g_sentinel);
                    Print(L"\r\n");
                } else {
                    Print(L"  (llmk not ready)\r\n\r\n");
                }
                continue;
            } else if (my_strncmp(prompt, "/budget", 7) == 0) {
                if (!g_llmk_ready) {
                    Print(L"\r\n  (llmk not ready)\r\n\r\n");
                    continue;
                }

                // Usage:
                //   /budget                 -> show
                //   /budget <p>             -> set both prefill+decode
                //   /budget <p> <d>         -> set separately
                int i = 7;
                while (prompt[i] == ' ') i++;
                if (prompt[i] == 0) {
                    Print(L"\r\nBudgets (cycles):\r\n");
                    Print(L"  prefill_max=%lu\r\n", g_budget_prefill_cycles);
                    Print(L"  decode_max=%lu\r\n\r\n", g_budget_decode_cycles);
                    continue;
                }

                UINT64 pre = 0;
                UINT64 dec = 0;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    pre = pre * 10ULL + (UINT64)(prompt[i] - '0');
                    i++;
                }
                while (prompt[i] == ' ') i++;
                if (prompt[i] == 0) {
                    dec = pre;
                } else {
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        dec = dec * 10ULL + (UINT64)(prompt[i] - '0');
                        i++;
                    }
                }

                g_budget_prefill_cycles = pre;
                g_budget_decode_cycles = dec;
                Print(L"\r\nBudgets set (cycles):\r\n");
                Print(L"  prefill_max=%lu\r\n", g_budget_prefill_cycles);
                Print(L"  decode_max=%lu\r\n\r\n", g_budget_decode_cycles);
                continue;
            } else if (my_strncmp(prompt, "/attn", 5) == 0) {
                // Usage:
                //   /attn          -> show
                //   /attn auto     -> runtime default
                //   /attn sse2     -> force SSE2 path
                //   /attn avx2     -> force AVX2 path (only if auto AVX2 is enabled)
                int i = 5;
                while (prompt[i] == ' ') i++;

                if (prompt[i] == 0) {
                    Print(L"\r\nAttention SIMD:\r\n");
                    Print(L"  auto=%s\r\n", g_attn_use_avx2 ? L"AVX2" : L"SSE2");
                    Print(L"  mode=%s\r\n\r\n",
                          (g_attn_force == -1) ? L"auto" : (g_attn_force == 0 ? L"sse2 (forced)" : L"avx2 (forced)"));
                    continue;
                }

                if (prompt[i] == 'a') {
                    g_attn_force = -1;
                    Print(L"\r\nOK: attn mode=auto\r\n\r\n");
                    continue;
                }
                if (prompt[i] == 's') {
                    g_attn_force = 0;
                    Print(L"\r\nOK: attn mode=sse2 (forced)\r\n\r\n");
                    continue;
                }
                if (prompt[i] == 'v') {
                    if (!g_attn_use_avx2) {
                        Print(L"\r\nERROR: AVX2 attention not available (auto is SSE2)\r\n\r\n");
                        continue;
                    }
                    g_attn_force = 1;
                    Print(L"\r\nOK: attn mode=avx2 (forced)\r\n\r\n");
                    continue;
                }

                Print(L"\r\nUsage: /attn [auto|sse2|avx2]\r\n\r\n");
                continue;
            } else if (my_strncmp(prompt, "/test_failsafe", 14) == 0) {
                // One-shot: temporarily enable strict budget and set tiny budgets so the next prompt trips.
                // Usage:
                //   /test_failsafe                -> decode trip with default cycles
                //   /test_failsafe prefill [c]    -> trip during prefill
                //   /test_failsafe decode [c]     -> trip during decode
                //   /test_failsafe both [c]       -> either phase can trip
                if (!g_llmk_ready) {
                    Print(L"\r\n  (llmk not ready)\r\n\r\n");
                    continue;
                }

                UINT64 cycles = 10000ULL;
                int mode = 2; // 1=prefill, 2=decode, 3=both

                int i = 14;
                while (prompt[i] == ' ') i++;
                if (prompt[i] == 'p') mode = 1;
                else if (prompt[i] == 'd') mode = 2;
                else if (prompt[i] == 'b') mode = 3;

                // Skip word
                while (prompt[i] && prompt[i] != ' ') i++;
                while (prompt[i] == ' ') i++;

                if (prompt[i] >= '0' && prompt[i] <= '9') {
                    cycles = 0;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        cycles = cycles * 10ULL + (UINT64)(prompt[i] - '0');
                        i++;
                    }
                }
                if (cycles < 100ULL) cycles = 100ULL;

                if (!g_test_failsafe_active) {
                    g_test_failsafe_prev_strict_budget = g_sentinel.cfg.strict_budget;
                    g_test_failsafe_prev_prefill = g_budget_prefill_cycles;
                    g_test_failsafe_prev_decode = g_budget_decode_cycles;
                }
                g_test_failsafe_active = 1;
                g_sentinel.cfg.strict_budget = TRUE;

                const UINT64 huge = 100000000000ULL;
                if (mode == 1) {
                    g_budget_prefill_cycles = cycles;
                    g_budget_decode_cycles = huge;
                } else if (mode == 2) {
                    g_budget_prefill_cycles = huge;
                    g_budget_decode_cycles = cycles;
                } else {
                    g_budget_prefill_cycles = cycles;
                    g_budget_decode_cycles = cycles;
                }

                Print(L"\r\n[test] fail-safe armed (strict_budget=1)\r\n");
                Print(L"  prefill_max=%lu decode_max=%lu\r\n", g_budget_prefill_cycles, g_budget_decode_cycles);
                Print(L"  Next prompt should trip and auto-dump ctx/zones/sentinel/log.\r\n\r\n");
                continue;
            } else if (my_strncmp(prompt, "/ctx", 4) == 0) {
                llmk_print_ctx(&config, model_filename, kv_pos, temperature, min_p, top_p, top_k, no_repeat_ngram, repeat_penalty, max_gen_tokens);
                continue;
            } else if (my_strncmp(prompt, "/log", 4) == 0) {
                UINT32 n = 16;
                if (prompt[4] == ' ') {
                    int i = 5;
                    UINT32 val = 0;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        val = val * 10u + (UINT32)(prompt[i] - '0');
                        i++;
                    }
                    if (val > 0) n = val;
                    if (n > 128) n = 128;
                }
                llmk_print_log(n);
                continue;
            } else if (my_strncmp(prompt, "/save_log", 9) == 0) {
                if (!g_llmk_ready || !g_llmk_log.capacity) {
                    Print(L"\r\n  (log not available)\r\n\r\n");
                    continue;
                }

                UINT32 n = 64;
                if (prompt[9] == ' ') {
                    int i = 10;
                    UINT32 val = 0;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        val = val * 10u + (UINT32)(prompt[i] - '0');
                        i++;
                    }
                    if (val > 0) n = val;
                    if (n > 128) n = 128;
                }

                EFI_FILE_HANDLE f = NULL;
                EFI_STATUS st = llmk_open_text_file(&f, L"llmk-log.txt");
                if (EFI_ERROR(st)) {
                    Print(L"\r\nERROR: failed to open llmk-log.txt: %r\r\n\r\n", st);
                    continue;
                }
                llmk_dump_log_to_file(f, &g_llmk_log, n);
                EFI_STATUS flush_st = uefi_call_wrapper(f->Flush, 1, f);
                uefi_call_wrapper(f->Close, 1, f);
                if (EFI_ERROR(flush_st)) {
                    Print(L"\r\nWARNING: flush failed %r (file may not persist)\r\n\r\n", flush_st);
                } else {
                    Print(L"\r\nOK: wrote llmk-log.txt (flushed)\r\n\r\n");
                }
                continue;
            } else if (my_strncmp(prompt, "/save_dump", 10) == 0) {
                if (!g_llmk_ready) {
                    Print(L"\r\n  (llmk not ready)\r\n\r\n");
                    continue;
                }

                EFI_FILE_HANDLE f = NULL;
                EFI_STATUS st = llmk_open_text_file(&f, L"llmk-dump.txt");
                if (EFI_ERROR(st)) {
                    Print(L"\r\nERROR: failed to open llmk-dump.txt: %r\r\n\r\n", st);
                    continue;
                }

                // Minimal ctx dump (match /ctx, in UTF-16)
                {
                    CHAR16 line[256];
                    llmk_file_write_u16(f, L"Context:\r\n");
                          SPrint(line, sizeof(line), L"  model=%s\r\n", model_filename ? model_filename : L"(unknown)");
                    llmk_file_write_u16(f, line);
                    SPrint(line, sizeof(line), L"  dim=%d layers=%d heads=%d kv=%d vocab=%d seq=%d\r\n",
                           config.dim, config.n_layers, config.n_heads, config.n_kv_heads, config.vocab_size, config.seq_len);
                    llmk_file_write_u16(f, line);
                          SPrint(line, sizeof(line), L"  kv_pos=%d\r\n", kv_pos);
                          llmk_file_write_u16(f, line);
                    llmk_file_write_u16(f, L"Sampling:\r\n");
                    SPrint(line, sizeof(line), L"  temp=%d.%02d min_p=%d.%02d top_p=%d.%02d top_k=%d\r\n",
                           (int)temperature, (int)((temperature - (int)temperature) * 100.0f),
                           (int)min_p, (int)((min_p - (int)min_p) * 100.0f),
                           (int)top_p, (int)((top_p - (int)top_p) * 100.0f),
                           top_k);
                    llmk_file_write_u16(f, line);
                    SPrint(line, sizeof(line), L"  norepeat=%d repeat_penalty=%d.%02d max_tokens=%d\r\n",
                           no_repeat_ngram,
                           (int)repeat_penalty, (int)((repeat_penalty - (int)repeat_penalty) * 100.0f),
                           max_gen_tokens);
                    llmk_file_write_u16(f, line);
                    llmk_file_write_u16(f, L"Budgets:\r\n");
                    SPrint(line, sizeof(line), L"  prefill_max=%lu decode_max=%lu overruns(p=%d d=%d)\r\n\r\n",
                           g_budget_prefill_cycles, g_budget_decode_cycles,
                           (int)g_budget_overruns_prefill, (int)g_budget_overruns_decode);
                    llmk_file_write_u16(f, line);
                }

                llmk_dump_zones_to_file(f, &g_zones);
                llmk_dump_sentinel_to_file(f, &g_sentinel);
                if (g_llmk_log.capacity) {
                    llmk_dump_log_to_file(f, &g_llmk_log, 128);
                }

                EFI_STATUS flush_st = uefi_call_wrapper(f->Flush, 1, f);
                uefi_call_wrapper(f->Close, 1, f);
                if (EFI_ERROR(flush_st)) {
                    Print(L"\r\nWARNING: flush failed %r (file may not persist)\r\n\r\n", flush_st);
                } else {
                    Print(L"\r\nOK: wrote llmk-dump.txt (flushed)\r\n\r\n");
                }
                continue;
            } else if (my_strncmp(prompt, "/gop", 4) == 0) {
                if (!g_gop_fb32) {
                    Print(L"\r\n  GOP: not available\r\n\r\n");
                } else {
                    const CHAR16 *pf = L"unknown";
                    if (g_gop_pf == PixelBlueGreenRedReserved8BitPerColor) pf = L"BGRX8888";
                    else if (g_gop_pf == PixelRedGreenBlueReserved8BitPerColor) pf = L"RGBX8888";
                    else if (g_gop_pf == PixelBitMask) pf = L"BITMASK";
                    Print(L"\r\n  GOP: %dx%d ppsl=%d fmt=%s fb=0x%lx\r\n\r\n",
                          (int)g_gop_w, (int)g_gop_h, (int)g_gop_ppsl, pf, (UINT64)(UINTN)g_gop_fb32);
                }
                continue;
            } else if (my_strncmp(prompt, "/render", 7) == 0) {
                if (!g_gop_fb32) {
                    Print(L"\r\nERROR: GOP not available on this firmware path\r\n\r\n");
                    continue;
                }
                const char *dsl = prompt + 7;
                while (*dsl == ' ' || *dsl == '\t') dsl++;
                if (*dsl == 0) {
                    Print(L"\r\nUsage: /render <dsl>\r\n");
                    Print(L"  DSL ops (separate by ';'):\r\n");
                    Print(L"    clear R G B; rect X Y W H R G B; pixel X Y R G B\r\n\r\n");
                    continue;
                }
                int ok = llmk_render_scene_dsl(dsl);
                if (ok) Print(L"\r\nOK: rendered\r\n\r\n");
                else Print(L"\r\nERROR: render failed\r\n\r\n");
                continue;
            } else if (my_strncmp(prompt, "/save_img", 9) == 0) {
                if (!g_gop_fb32) {
                    Print(L"\r\nERROR: GOP not available (nothing to save)\r\n\r\n");
                    continue;
                }
                const char *name = prompt + 9;
                while (*name == ' ' || *name == '\t') name++;

                CHAR16 out_name[64];
                if (*name == 0) {
                    StrCpy(out_name, L"llmk-img.ppm");
                } else {
                    ascii_to_char16(out_name, name, (int)(sizeof(out_name) / sizeof(out_name[0])));
                }

                EFI_STATUS st = llmk_save_ppm(out_name);
                if (EFI_ERROR(st)) {
                    Print(L"\r\nERROR: save failed (%r)\r\n\r\n", st);
                } else {
                    Print(L"\r\nOK: wrote %s (PPM, flushed)\r\n\r\n", out_name);
                }
                continue;
            } else if (my_strncmp(prompt, "/reset", 6) == 0) {
                Print(L"\r\nResetting runtime state...\r\n");
                if (g_llmk_ready) {
                    llmk_reset_runtime_state();
                    Print(L"OK\r\n\r\n");
                } else {
                    Print(L"  (llmk not ready)\r\n\r\n");
                }
                continue;
            } else if (my_strncmp(prompt, "/clear", 6) == 0) {
                Print(L"\r\nClearing KV cache...\r\n");
                reset_kv_cache(&state, &config);
                kv_pos = 0;
                Print(L"OK: KV cache cleared, context reset\r\n\r\n");
                continue;
            } else if (my_strncmp(prompt, "/version", 8) == 0) {
                Print(L"\r\nllm-baremetal REPL v3\r\n");
                Print(L"  build=2026-01-08\r\n");
                Print(L"  model=%s seq_len=%d kv_pos=%d\r\n", model_filename ? model_filename : L"(unknown)", config.seq_len, kv_pos);
                Print(L"  features=zones+sentinel+log djibmark utf8 multiline persist\r\n");
                Print(L"  hint: /cpu for SIMD, /ctx for config\r\n\r\n");
                continue;
            } else if (my_strncmp(prompt, "/djibmarks", 10) == 0) {
                DJIBMARK_REPL();
                Print(L"\r\nDjibMark Trace (last %d marks):\r\n", (int)djibmark_count());
                Print(L"  Magic: 0x%08X (DJIB2026)\r\n", DJIBMARK_MAGIC);
                Print(L"  Total recorded: %u\r\n", g_djibmark_state.total_marks);
                Print(L"  Enabled: %s\r\n\r\n", g_djibmark_state.enabled ? L"yes" : L"no");
                
                UINT32 count = djibmark_count();
                if (count > 32) count = 32;  // Limit to 32 most recent
                
                Print(L"  Seq      TSC          Phase    Location\r\n");
                Print(L"  -------- ------------ -------- ------------------\r\n");
                for (UINT32 i = 0; i < count; i++) {
                    DjibMark* m = djibmark_get(i);
                    if (!m || m->magic != DJIBMARK_MAGIC) continue;
                    
                    // Convert CHAR8* to print char by char
                    Print(L"  %08u %012lu %-8s ", m->sequence, m->timestamp_tsc, djibmark_phase_name(m->phase));
                    if (m->location) {
                        for (const CHAR8* p = m->location; *p; p++) {
                            Print(L"%c", (CHAR16)*p);
                        }
                    }
                    Print(L":%u\r\n", m->line);
                }
                Print(L"\r\n");
                continue;
            } else if (my_strncmp(prompt, "/djibperf", 9) == 0) {
                DJIBMARK_REPL();
                Print(L"\r\nDjibMark Performance Analysis:\r\n\r\n");
                
                UINT32 count = djibmark_count();
                if (count < 2) {
                    Print(L"  Need at least 2 marks for analysis\r\n\r\n");
                    continue;
                }
                
                // Analyze phase transitions
                UINT64 prefill_cycles = 0, decode_cycles = 0;
                UINT32 prefill_count = 0, decode_count = 0;
                
                for (UINT32 i = 1; i < count && i < 128; i++) {
                    DjibMark* curr = djibmark_get(i-1);
                    DjibMark* prev = djibmark_get(i);
                    if (!curr || !prev) continue;
                    if (curr->magic != DJIBMARK_MAGIC || prev->magic != DJIBMARK_MAGIC) continue;
                    
                    UINT64 delta = (curr->timestamp_tsc > prev->timestamp_tsc) 
                                   ? (curr->timestamp_tsc - prev->timestamp_tsc) : 0;
                    
                    if (curr->phase == DJIBMARK_PHASE_PREFILL) {
                        prefill_cycles += delta;
                        prefill_count++;
                    } else if (curr->phase == DJIBMARK_PHASE_DECODE) {
                        decode_cycles += delta;
                        decode_count++;
                    }
                }
                
                Print(L"  Prefill phase:\r\n");
                Print(L"    Count: %u marks\r\n", prefill_count);
                Print(L"    Total cycles: %lu\r\n", prefill_cycles);
                if (prefill_count > 0) {
                    Print(L"    Avg cycles/mark: %lu\r\n", prefill_cycles / prefill_count);
                }
                
                Print(L"\r\n  Decode phase:\r\n");
                Print(L"    Count: %u marks\r\n", decode_count);
                Print(L"    Total cycles: %lu\r\n", decode_cycles);
                if (decode_count > 0) {
                    Print(L"    Avg cycles/mark: %lu\r\n", decode_cycles / decode_count);
                }
                Print(L"\r\n");
                continue;
            } else if (my_strncmp(prompt, "/help", 5) == 0) {
                Print(L"\r\nCommands:\r\n");
                Print(L"  /temp <val>   - Set temperature (0.0=greedy, 1.0=creative)\r\n");
                Print(L"  /min_p <val>  - Set min_p (0.0-1.0, 0=off)\r\n");
                Print(L"  /top_p <val>  - Set nucleus sampling (0.0-1.0)\r\n");
                Print(L"  /top_k <int>  - Set top-k (0=off, typical 40-200)\r\n");
                Print(L"  /norepeat <n> - No-repeat ngram (0=off, typical 3-6)\r\n");
                Print(L"  /max_tokens <n> - Max generation tokens (1-256)\r\n");
                Print(L"  /seed <n>       - RNG seed\r\n");
                Print(L"  /stats <0|1>    - Print generation stats\r\n");
                Print(L"  /stop_you <0|1> - Stop on \\nYou: pattern\r\n");
                Print(L"  /stop_nl <0|1>  - Stop on double newline\r\n");
                Print(L"  /repeat <val> - Set repetition penalty (1.0=none, 1.5=strong)\r\n");
                Print(L"  /model        - Show loaded model config\r\n");
                Print(L"  /cpu          - Show CPU SIMD status\r\n");
                Print(L"  /zones        - Dump allocator zones + sentinel\r\n");
                Print(L"  /budget [p] [d] - Set budgets in cycles (p=prefill, d=decode)\r\n");
                Print(L"  /attn [auto|sse2|avx2] - Force attention SIMD path\r\n");
                Print(L"  /test_failsafe [prefill|decode|both] [cycles] - One-shot strict budget trip\r\n");
                Print(L"  /ctx          - Show model + sampling + budgets\r\n");
                Print(L"  /log [n]      - Dump last n log entries\r\n");
                Print(L"  /save_log [n] - Write last n log entries to llmk-log.txt\r\n");
                Print(L"  /save_dump    - Write ctx+zones+sentinel+log to llmk-dump.txt\r\n");
                Print(L"  /gop          - Show GOP framebuffer info\r\n");
                Print(L"  /render <dsl> - Render simple shapes to GOP framebuffer\r\n");
                Print(L"  /save_img [f] - Save GOP framebuffer as PPM (default llmk-img.ppm)\r\n");
                Print(L"  /reset        - Clear budgets/log + untrip sentinel\r\n");
                Print(L"  /clear        - Clear KV cache (reset conversation context)\r\n");
                Print(L"  /djibmarks    - Show DjibMark execution trace (Made in 🇸🇳)\r\n");
                Print(L"  /djibperf     - DjibMark performance analysis by phase\r\n");
                Print(L"  /version      - Show build version + features\r\n");
                Print(L"  /help         - Show this help\r\n\r\n");
                Print(L"Multi-line input:\r\n");
                Print(L"  End a line with '\\' to continue; type ';;' on its own line to submit.\r\n");
                Print(L"  Use '\\\\' at end of line for a literal backslash.\r\n\r\n");
                Print(L"Render DSL:\r\n");
                Print(L"  clear R G B; rect X Y W H R G B; pixel X Y R G B\r\n\r\n");
                Print(L"Current settings:\r\n");
                Print(L"  Temperature: ");
                Print(L"%d.", (int)temperature);
                Print(L"%d\r\n", (int)((temperature - (int)temperature) * 100.0f));
                Print(L"  Min-p: ");
                Print(L"%d.", (int)min_p);
                Print(L"%d\r\n", (int)((min_p - (int)min_p) * 100.0f));
                Print(L"  Top-p: ");
                Print(L"%d.", (int)top_p);
                Print(L"%d\r\n", (int)((top_p - (int)top_p) * 100.0f));
                Print(L"  Top-k: %d\r\n", top_k);
                Print(L"  No-repeat ngram: %d\r\n", no_repeat_ngram);
                Print(L"  Max tokens: %d\r\n", max_gen_tokens);
                Print(L"  Stats: %s\r\n", stats_enabled ? L"on" : L"off");
                Print(L"  Stop on \\nYou:: %s\r\n", stop_on_you ? L"on" : L"off");
                Print(L"  Stop on double newline: %s\r\n", stop_on_double_nl ? L"on" : L"off");
                Print(L"  Repeat penalty: ");
                Print(L"%d.", (int)repeat_penalty);
                Print(L"%d\r\n\r\n", (int)((repeat_penalty - (int)repeat_penalty) * 100.0f));
                continue;
            }
        }
        
        // Encode prompt
        int prompt_tokens[256];
        int n_prompt_tokens = 0;
        encode(prompt, prompt_tokens, &n_prompt_tokens, 256, &tokenizer);
        
        // Check if KV cache will overflow
        if (kv_pos + n_prompt_tokens + max_gen_tokens > config.seq_len) {
            Print(L"\r\nWARNING: context too long (%d + %d tokens), clearing KV cache\r\n", 
                  kv_pos, n_prompt_tokens + max_gen_tokens);
            reset_kv_cache(&state, &config);
            kv_pos = 0;
        }
        
        Print(L"AI: ");

        if (g_llmk_ready) {
            // Reset per-generation overrun counters and print current budget state.
            g_budget_overruns_prefill = 0;
            g_budget_overruns_decode = 0;
            Print(L"\r\n[llmk][budget] prefill_max=%lu decode_max=%lu\r\n",
                  g_budget_prefill_cycles, g_budget_decode_cycles);
        }
        
        // Process prompt tokens through model first (prefill)
        for (int i = 0; i < n_prompt_tokens; i++) {
            int pos = kv_pos + i;  // Use persistent KV position
            if (g_llmk_ready) {
                // Per-token prefill budgeting (pos-dependent): set budget before each forward.
                if (g_budget_prefill_cycles == 0) {
                    // Start huge to ensure we get a first measurement without tripping.
                    // llmk_budget_update() will snap down quickly after the first dt sample.
                    g_budget_prefill_cycles = 100000000000ULL;
                }
                g_sentinel.cfg.max_cycles_prefill = g_budget_prefill_cycles;
                llmk_sentinel_phase_start(&g_sentinel, LLMK_PHASE_PREFILL);
                transformer_forward(&state, &weights, &config, prompt_tokens[i], pos);
                BOOLEAN ok = llmk_sentinel_phase_end(&g_sentinel);
                if (g_sentinel.tripped) {
                    Print(L"\r\n[llmk] prefill stopped (fail-safe) at i=%d\r\n", i);
                    llmk_print_ctx(&config, model_filename, kv_pos, temperature, min_p, top_p, top_k, no_repeat_ngram, repeat_penalty, max_gen_tokens);
                    llmk_zones_print(&g_zones);
                    llmk_sentinel_print_status(&g_sentinel);
                    llmk_print_log(32);

                    // Best-effort: persist dump to file for offline diagnosis.
                    {
                        EFI_FILE_HANDLE f = NULL;
                        EFI_STATUS st = llmk_open_text_file(&f, L"llmk-failsafe.txt");
                        if (!EFI_ERROR(st)) {
                            llmk_file_write_u16(f, L"FAIL-SAFE: prefill\r\n\r\n");
                            llmk_dump_zones_to_file(f, &g_zones);
                            llmk_dump_sentinel_to_file(f, &g_sentinel);
                            if (g_llmk_log.capacity) llmk_dump_log_to_file(f, &g_llmk_log, 128);
                            uefi_call_wrapper(f->Flush, 1, f);
                            uefi_call_wrapper(f->Close, 1, f);
                            Print(L"[llmk] wrote llmk-failsafe.txt\r\n");
                        }
                    }
                    if (g_test_failsafe_active) {
                        g_sentinel.cfg.strict_budget = g_test_failsafe_prev_strict_budget;
                        g_budget_prefill_cycles = g_test_failsafe_prev_prefill;
                        g_budget_decode_cycles = g_test_failsafe_prev_decode;
                        g_test_failsafe_active = 0;
                        Print(L"[test] fail-safe test complete (restored)\r\n");
                    }
                    break;
                }
                if (!ok) {
                    // Non-fatal budget overrun: adapt budget upward and continue.
                    g_budget_overruns_prefill++;
                    if (g_budget_overruns_prefill <= 3) {
                        Print(L"\r\n[llmk][budget] prefill overrun i=%d cycles=%lu max=%lu (auto-raise)\r\n",
                              i, g_sentinel.last_dt_cycles, g_sentinel.last_budget_cycles);
                    }
                }
                llmk_budget_update(&g_budget_prefill_cycles, g_sentinel.last_dt_cycles);
            } else {
                transformer_forward(&state, &weights, &config, prompt_tokens[i], i);
            }
        }
        
        // Start generation from the last prompt token.
        // After prefill, state.logits already corresponds to the last prompt token at position (n_prompt_tokens-1).
        int next;
        int token = prompt_tokens[n_prompt_tokens - 1];
        int pos = kv_pos + n_prompt_tokens - 1;  // Use persistent KV position
        
        int generated_count = 0;
        int repeat_count = 0;
        int last_token = -1;
        int loop_escape_used = 0;
        
        // Track context for repetition penalty and loop detection.
        int context_tokens[256 + MAX_TOKENS];
        int n_context_tokens = 0;
        for (int i = 0; i < n_prompt_tokens && n_context_tokens < (int)(sizeof(context_tokens) / sizeof(context_tokens[0])); i++) {
            context_tokens[n_context_tokens++] = prompt_tokens[i];
        }

        // Simple stop detection on the last bytes printed.
        char out_tail[64];
        int out_tail_len = 0;
        for (int i = 0; i < 64; i++) out_tail[i] = 0;

        unsigned long long gen_t0 = 0;
        unsigned long long gen_wall0_us = 0;
        int gen_have_wall = 0;
        if (stats_enabled) {
            calibrate_tsc_once();
            gen_t0 = rdtsc();
            gen_have_wall = uefi_wall_us(&gen_wall0_us);
        }

        for (int step = 0; step < max_gen_tokens; step++) {
            // We sample from the logits produced by the previous forward pass.
            // For step==0, logits come from the final prompt token (prefill).

            // Apply no-repeat ngram blocking (works on pre-softmax logits).
            if (no_repeat_ngram > 1) {
                apply_no_repeat_ngram(state.logits, config.vocab_size, context_tokens, n_context_tokens, no_repeat_ngram);
            }

            // Sample next token (temperature/top_p/top_k + repetition penalty)
            int n_recent = n_context_tokens;
            if (n_recent > 64) n_recent = 64;
            int* recent = (n_recent > 0) ? &context_tokens[n_context_tokens - n_recent] : (int*)0;

            // One-time loop escape: if we detect a short repeating suffix, ban the sampled token once and resample.
            for (int attempt = 0; attempt < 2; attempt++) {
                next = sample_advanced(state.logits, config.vocab_size, temperature, min_p, top_p, top_k, recent, n_recent, repeat_penalty);
                if (next == TOKEN_EOS || next == TOKEN_BOS) break;
                if (!loop_escape_used && n_context_tokens + 1 < (int)(sizeof(context_tokens) / sizeof(context_tokens[0]))) {
                    context_tokens[n_context_tokens] = next;
                    int would_repeat = has_suffix_repeat(context_tokens, n_context_tokens + 1, 8) ||
                                      has_suffix_repeat(context_tokens, n_context_tokens + 1, 12) ||
                                      has_suffix_repeat(context_tokens, n_context_tokens + 1, 16);
                    if (would_repeat) {
                        loop_escape_used = 1;
                        state.logits[next] = -1.0e9f;
                        continue;
                    }
                }
                break;
            }
            
            // Check for EOS (some exports may still emit BOS; treat both as stop)
            if (next == TOKEN_EOS || next == TOKEN_BOS) break;
            
            // Check if stuck on same token (per conversation)
            if (next == last_token) {
                repeat_count++;
                if (repeat_count > 5) break;
            } else {
                repeat_count = 0;
                last_token = next;
            }
            
            // Print token (ALL tokens for now, no filtering!)
            if (next >= 0 && next < config.vocab_size && tokenizer.vocab[next]) {
                char* piece = tokenizer.vocab[next];
                int len = my_strlen(piece);
                if (len > 0) {
                    uefi_print_utf8_bytes(piece, len);
                    generated_count++;

                    // Update ASCII tail buffer for stop detection.
                    for (int k = 0; k < len; k++) {
                        char ch = piece[k];
                        if (out_tail_len < (int)sizeof(out_tail) - 1) {
                            out_tail[out_tail_len++] = ch;
                            out_tail[out_tail_len] = 0;
                        } else {
                            // shift left by 1
                            for (int s = 0; s < (int)sizeof(out_tail) - 2; s++) out_tail[s] = out_tail[s + 1];
                            out_tail[(int)sizeof(out_tail) - 2] = ch;
                            out_tail[(int)sizeof(out_tail) - 1] = 0;
                        }
                    }

                    // Stop conditions
                    if (stop_on_double_nl) {
                        // Look for "\n\n" in tail.
                        for (int i = 0; i + 1 < out_tail_len; i++) {
                            if (out_tail[i] == '\n' && out_tail[i + 1] == '\n') {
                                step = max_gen_tokens; // force exit
                                break;
                            }
                        }
                    }
                    if (stop_on_you) {
                        // Look for "\nYou:" in tail.
                        for (int i = 0; i + 4 < out_tail_len; i++) {
                            if (out_tail[i] == '\n' && out_tail[i + 1] == 'Y' && out_tail[i + 2] == 'o' && out_tail[i + 3] == 'u' && out_tail[i + 4] == ':') {
                                step = max_gen_tokens; // force exit
                                break;
                            }
                        }
                    }
                }
            }

            // Append to context and apply a simple loop-stop heuristic.
            if (n_context_tokens < (int)(sizeof(context_tokens) / sizeof(context_tokens[0]))) {
                context_tokens[n_context_tokens++] = next;
            }
            // Stop if the tail repeats (common failure mode: short loops).
            // spans chosen to be cheap and effective in practice.
            if (has_suffix_repeat(context_tokens, n_context_tokens, 8) ||
                has_suffix_repeat(context_tokens, n_context_tokens, 12) ||
                has_suffix_repeat(context_tokens, n_context_tokens, 16)) {
                break;
            }
            
            // Advance position and compute next logits
            token = next;
            pos++;
            if (pos >= config.seq_len) break;

            if (g_llmk_ready) {
                if (g_budget_decode_cycles == 0) {
                    g_budget_decode_cycles = 100000000000ULL;
                }
                g_sentinel.cfg.max_cycles_decode = g_budget_decode_cycles;
                llmk_sentinel_phase_start(&g_sentinel, LLMK_PHASE_DECODE);
                transformer_forward(&state, &weights, &config, token, pos);
                BOOLEAN ok = llmk_sentinel_phase_end(&g_sentinel);
                if (g_sentinel.tripped) {
                    Print(L"\r\n[llmk] decode stopped (fail-safe) at step=%d pos=%d\r\n", step, pos);
                    llmk_print_ctx(&config, model_filename, kv_pos, temperature, min_p, top_p, top_k, no_repeat_ngram, repeat_penalty, max_gen_tokens);
                    llmk_zones_print(&g_zones);
                    llmk_sentinel_print_status(&g_sentinel);
                    llmk_print_log(32);

                    // Best-effort: persist dump to file for offline diagnosis.
                    {
                        EFI_FILE_HANDLE f = NULL;
                        EFI_STATUS st = llmk_open_text_file(&f, L"llmk-failsafe.txt");
                        if (!EFI_ERROR(st)) {
                            llmk_file_write_u16(f, L"FAIL-SAFE: decode\r\n\r\n");
                            llmk_dump_zones_to_file(f, &g_zones);
                            llmk_dump_sentinel_to_file(f, &g_sentinel);
                            if (g_llmk_log.capacity) llmk_dump_log_to_file(f, &g_llmk_log, 128);
                            uefi_call_wrapper(f->Flush, 1, f);
                            uefi_call_wrapper(f->Close, 1, f);
                            Print(L"[llmk] wrote llmk-failsafe.txt\r\n");
                        }
                    }
                    if (g_test_failsafe_active) {
                        g_sentinel.cfg.strict_budget = g_test_failsafe_prev_strict_budget;
                        g_budget_prefill_cycles = g_test_failsafe_prev_prefill;
                        g_budget_decode_cycles = g_test_failsafe_prev_decode;
                        g_test_failsafe_active = 0;
                        Print(L"[test] fail-safe test complete (restored)\r\n");
                    }
                    break;
                }
                if (!ok) {
                    g_budget_overruns_decode++;
                    if (g_budget_overruns_decode <= 3) {
                        Print(L"\r\n[llmk][budget] decode overrun step=%d pos=%d cycles=%lu max=%lu (auto-raise)\r\n",
                              step, pos, g_sentinel.last_dt_cycles, g_sentinel.last_budget_cycles);
                    }
                }
                llmk_budget_update(&g_budget_decode_cycles, g_sentinel.last_dt_cycles);
            } else {
                transformer_forward(&state, &weights, &config, token, pos);
            }
        }

        // Flush any pending bytes held for mojibake repair across token boundaries.
        uefi_print_utf8_flush();

        if (g_test_failsafe_active) {
            g_sentinel.cfg.strict_budget = g_test_failsafe_prev_strict_budget;
            g_budget_prefill_cycles = g_test_failsafe_prev_prefill;
            g_budget_decode_cycles = g_test_failsafe_prev_decode;
            g_test_failsafe_active = 0;
            Print(L"\r\n[test] fail-safe test cancelled (no trip; restored)\r\n");
        }

        if (g_llmk_ready) {
            Print(L"\r\n[llmk][budget] final prefill_max=%lu decode_max=%lu overruns(p=%d d=%d)\r\n",
                  g_budget_prefill_cycles,
                  g_budget_decode_cycles,
                  (int)g_budget_overruns_prefill,
                  (int)g_budget_overruns_decode);
        }

        if (stats_enabled) {
            unsigned long long gen_t1 = rdtsc();
            unsigned long long dt = (gen_t1 > gen_t0) ? (gen_t1 - gen_t0) : 0;

            // Prefer wall-clock timing when available (more stable under emulation).
            if (gen_have_wall) {
                unsigned long long gen_wall1_us = 0;
                if (uefi_wall_us(&gen_wall1_us)) {
                    unsigned long long wall_dt_us = (gen_wall1_us >= gen_wall0_us) ? (gen_wall1_us - gen_wall0_us)
                                                                                   : (gen_wall1_us + 86400ULL * 1000000ULL - gen_wall0_us);
                    unsigned long long ms = wall_dt_us / 1000ULL;
                    if (wall_dt_us == 0) {
                        Print(L"\r\n[stats] tokens=%d time_ms=%d tok_s=inf\r\n", generated_count, (int)ms);
                    } else {
                        unsigned long long tps_milli = ((unsigned long long)generated_count * 1000000ULL * 1000ULL) / wall_dt_us;
                        unsigned long long tps_int = tps_milli / 1000ULL;
                        unsigned long long tps_frac = tps_milli % 1000ULL;
                        Print(L"\r\n[stats] tokens=%d time_ms=%d tok_s=%d.%03d\r\n",
                              generated_count, (int)ms, (int)tps_int, (int)tps_frac);
                    }
                    goto stats_done;
                }
            }

            // Fallback to TSC-based estimate.
            if (tsc_per_sec == 0 || dt == 0) {
                Print(L"\r\n[stats] tokens=%d cycles=%d\r\n", generated_count, (int)dt);
            } else {
                unsigned long long ms = (dt * 1000ULL) / tsc_per_sec;
                // milli tok/s for visibility even when < 1 tok/s
                unsigned long long tps_milli = ((unsigned long long)generated_count * tsc_per_sec * 1000ULL) / dt;
                unsigned long long tps_int = tps_milli / 1000ULL;
                unsigned long long tps_frac = tps_milli % 1000ULL;
                Print(L"\r\n[stats] tokens=%d time_ms=%d tok_s=%d.%03d\r\n",
                      generated_count, (int)ms, (int)tps_int, (int)tps_frac);
            }
stats_done:
            ;
        }
        
        // Update persistent KV cache position for next generation
        kv_pos += n_prompt_tokens + generated_count;
        
        Print(L"\r\n\r\n");
    }
    
    Print(L"Press any key to exit...\r\n");
    EFI_INPUT_KEY Key;
    UINTN index;
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &index);
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
    
    return EFI_SUCCESS;
}
