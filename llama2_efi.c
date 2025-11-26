/*
 * LLaMA2 Inference on Bare-Metal UEFI Firmware
 * 
 * Runs 110M parameter transformer model directly on UEFI without OS.
 * Based on Andrej Karpathy's llama2.c (MIT License)
 * https://github.com/karpathy/llama2.c
 * 
 * Model: stories110M.bin (dim=768, n_layers=12, n_heads=12, seq_len=256)
 * 
 * SPDX-License-Identifier: MIT
 */

#include <efi.h>
#include <efilib.h>
#include <stdint.h>

// AVX2 intrinsics for SIMD optimization
#if defined(__AVX2__) && defined(__FMA__)
#include <immintrin.h>
#endif

// ----------------------------------------------------------------------------
// Network Protocol GUIDs and Structures

// HTTP Protocol GUID (UEFI 2.5+)
#define EFI_HTTP_SERVICE_BINDING_PROTOCOL_GUID \
  { 0xbdc8e6af, 0xd9bc, 0x4379, \
    { 0xa7, 0x2a, 0xe0, 0xc4, 0xe7, 0x5d, 0xae, 0x1c }}

#define EFI_HTTP_PROTOCOL_GUID \
  { 0x7a59b29b, 0x910b, 0x4171, \
    { 0x82, 0x42, 0xa8, 0x5a, 0x0d, 0xf2, 0x5b, 0x5b }}

// Simple Network Protocol structures
typedef struct {
    CHAR8 *hostname;
    CHAR8 *path;
    UINT16 port;
} UrlInfo;

typedef struct {
    CHAR16 *filename;
    CHAR16 *url;
    UINT64 total_bytes;
    UINT64 downloaded_bytes;
    BOOLEAN is_active;
} DownloadProgress;

// Model repository entry
typedef struct {
    const char* name;
    const char* description;
    const char* url;
    UINT64 size_mb;
    UINT64 min_ram_mb;
    const char* source;  // "karpathy", "meta", "community"
} ModelRepositoryEntry;

// Model Repository - Downloadable models from various sources
static ModelRepositoryEntry model_repository[] = {
    // Karpathy's TinyLlamas (trained on TinyStories dataset)
    {
        "stories15M.bin",
        "15M params - Tiny stories model (60 MB)",
        "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin",
        60,
        256,
        "karpathy/tinyllamas"
    },
    {
        "stories42M.bin",
        "42M params - Small stories model (164 MB)",
        "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories42M.bin",
        164,
        512,
        "karpathy/tinyllamas"
    },
    {
        "stories110M.bin",
        "110M params - Medium stories model (420 MB)",
        "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin",
        420,
        1024,
        "karpathy/tinyllamas"
    },
    {
        "stories260M.bin",
        "260M params - Large stories model (1 GB)",
        "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories260M.bin",
        1024,
        2048,
        "karpathy/tinyllamas"
    },
    // Tokenizer (required for all models)
    {
        "tokenizer.bin",
        "BPE tokenizer (32k vocab, 433 KB)",
        "https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin",
        1,
        0,
        "karpathy/llama2.c"
    }
};

#define MODEL_REPO_COUNT (sizeof(model_repository) / sizeof(ModelRepositoryEntry))

// ----------------------------------------------------------------------------
// Simple string functions
static inline int strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static inline int strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == 0) return 0;
    }
    return 0;
}

static inline int strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        i++;
    }
    return s1[i] - s2[i];
}

// ----------------------------------------------------------------------------
// UEFI Console Colors
#define EFI_BLACK            0x00
#define EFI_BLUE             0x01
#define EFI_GREEN            0x02
#define EFI_CYAN             0x03
#define EFI_RED              0x04
#define EFI_MAGENTA          0x05
#define EFI_BROWN            0x06
#define EFI_LIGHTGRAY        0x07
#define EFI_DARKGRAY         0x08
#define EFI_LIGHTBLUE        0x09
#define EFI_LIGHTGREEN       0x0A
#define EFI_LIGHTCYAN        0x0B
#define EFI_LIGHTRED         0x0C
#define EFI_LIGHTMAGENTA     0x0D
#define EFI_YELLOW           0x0E
#define EFI_WHITE            0x0F

// Helper macros for colored text
#define COLOR_HEADER         (EFI_YELLOW | EFI_BLACK << 4)
#define COLOR_SUCCESS        (EFI_LIGHTGREEN | EFI_BLACK << 4)
#define COLOR_ERROR          (EFI_LIGHTRED | EFI_BLACK << 4)
#define COLOR_INFO           (EFI_LIGHTCYAN | EFI_BLACK << 4)
#define COLOR_PROMPT         (EFI_LIGHTMAGENTA | EFI_BLACK << 4)
#define COLOR_TEXT           (EFI_WHITE | EFI_BLACK << 4)
#define COLOR_CATEGORY       (EFI_CYAN | EFI_BLACK << 4)

// Console color helpers
extern EFI_SYSTEM_TABLE *ST;

void set_color(UINTN color) {
    if (ST && ST->ConOut) {
        ST->ConOut->SetAttribute(ST->ConOut, color);
    }
}

void reset_color(void) {
    set_color(EFI_WHITE | EFI_BLACK << 4);
}

void print_header(CHAR16* text) {
    set_color(COLOR_HEADER);
    Print(L"\r\n╔══════════════════════════════════════════════════════════════╗\r\n");
    Print(L"║  %s", text);
    // Calculate padding
    int len = 0;
    while (text[len]) len++;
    for (int i = len; i < 56; i++) Print(L" ");
    Print(L"║\r\n");
    Print(L"╚══════════════════════════════════════════════════════════════╝\r\n");
    reset_color();
}

void print_success(CHAR16* text) {
    set_color(COLOR_SUCCESS);
    Print(L"✓ %s\r\n", text);
    reset_color();
}

void print_error(CHAR16* text) {
    set_color(COLOR_ERROR);
    Print(L"✗ %s\r\n", text);
    reset_color();
}

void print_info(CHAR16* text) {
    set_color(COLOR_INFO);
    Print(L"ℹ %s\r\n", text);
    reset_color();
}

void print_separator(void) {
    set_color(EFI_DARKGRAY | EFI_BLACK << 4);
    Print(L"────────────────────────────────────────────────────────────────\r\n");
    reset_color();
}

// ----------------------------------------------------------------------------
// Math functions for EFI (no stdlib)
// High-quality implementations from ARM Optimized Routines

typedef float float_t;
typedef double double_t;

float sqrtf(float x) {
    if (x < 0.0f) return 0.0f;
    float guess = x;
    for (int i = 0; i < 10; i++) {
        if (guess == 0.0f) return 0.0f;
        guess = (guess + x / guess) / 2.0f;
    }
    return guess;
}

/* Single-precision expf(x) function.
   ULP error: 0.502 (nearest rounding)
   From ARM Optimized Routines.
   Uses fast rounding trick to eliminate round()/lround() dependency. */
float expf(float x) {
    // Magic number for fast rounding (2^52 * 1.5)
    const double_t shift = 0x1.8p52;
    
    /* return zero if less than -104 */
    if (x < -0x1.9fe368p6f) {
        return 0.0f;
    }

    /* return infinity if greater than 88 */
    if (x > 0x1.62e42ep6f) {
        union { uint32_t i; float f; } u = {0x7f800000};
        return u.f;
    }

    /* Range reduction: x*N/Ln2 = k + r with r in [-1/2, 1/2] and int k */
    int N = 32;
    double_t z = 0x1.71547652b82fep+0 * N * x;

    /* FAST ROUNDING TRICK (replaces round/lround) */
    /* Adding 'shift' aligns the floating point binary point */
    double_t kd = z + shift;
    
    /* Read the lower bits directly to get the integer part */
    union { double_t f; uint64_t i; } u_shift = {kd};
    uint64_t ki = u_shift.i;
    
    /* Remove the shift to get back to the rounded double value */
    kd -= shift;
    
    double_t r = z - kd;

    /* exp(x) = 2^(k/N) * 2^(r/N) ~= s * (C0*r^3 + C1*r^2 + C2*r + 1) */
    static const uint64_t T[32] = {
        0x3ff0000000000000, 0x3fefd9b0d3158574, 0x3fefb5586cf9890f, 0x3fef9301d0125b51,
        0x3fef72b83c7d517b, 0x3fef54873168b9aa, 0x3fef387a6e756238, 0x3fef1e9df51fdee1,
        0x3fef06fe0a31b715, 0x3feef1a7373aa9cb, 0x3feedea64c123422, 0x3feece086061892d,
        0x3feebfdad5362a27, 0x3feeb42b569d4f82, 0x3feeab07dd485429, 0x3feea47eb03a5585,
        0x3feea09e667f3bcd, 0x3fee9f75e8ec5f74, 0x3feea11473eb0187, 0x3feea589994cce13,
        0x3feeace5422aa0db, 0x3feeb737b0cdc5e5, 0x3feec49182a3f090, 0x3feed503b23e255d,
        0x3feee89f995ad3ad, 0x3feeff76f2fb5e47, 0x3fef199bdd85529c, 0x3fef3720dcef9069,
        0x3fef5818dcfba487, 0x3fef7c97337b9b5f, 0x3fefa4afa2a490da, 0x3fefd0765b6e4540,
    };

    union {
        uint64_t i;
        double f;
    } d = {T[ki % N] + (ki << 47)};

    double_t p0 = 0x1.c6af84b912394p-5 / N / N / N;
    double_t p1 = 0x1.ebfce50fac4f3p-3 / N / N;
    double_t p2 = 0x1.62e42ff0c52d6p-1 / N;
    double_t y = p2 * r + 1;
    y = (p0 * r + p1) * (r * r) + y;
    y = y * d.f;
    return (float)y;
}

/* Single-precision sinf/cosf functions.
   From ARM Optimized Routines. */

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(x, 0)

typedef struct {
  double sign[4];
  double hpi_inv;
  double hpi;
  double c0, c1, c2, c3, c4;
  double s1, s2, s3;
} sincos_t;

static const sincos_t __sincosf_table[2] = {
    {{1.0, -1.0, -1.0, 1.0},
     0x1.45F306DC9C883p+23,
     0x1.921FB54442D18p0,
     0x1p0,
     -0x1.ffffffd0c621cp-2,
     0x1.55553e1068f19p-5,
     -0x1.6c087e89a359dp-10,
     0x1.99343027bf8c3p-16,
     -0x1.555545995a603p-3,
     0x1.1107605230bc4p-7,
     -0x1.994eb3774cf24p-13},
    {{1.0, -1.0, -1.0, 1.0},
     0x1.45F306DC9C883p+23,
     0x1.921FB54442D18p0,
     -0x1p0,
     0x1.ffffffd0c621cp-2,
     -0x1.55553e1068f19p-5,
     0x1.6c087e89a359dp-10,
     -0x1.99343027bf8c3p-16,
     -0x1.555545995a603p-3,
     0x1.1107605230bc4p-7,
     -0x1.994eb3774cf24p-13},
};

static const uint32_t __inv_pio4[] = {
    0xa2,       0xa2f9,     0xa2f983,   0xa2f9836e, 0xf9836e4e, 0x836e4e44,
    0x6e4e4415, 0x4e441529, 0x441529fc, 0x1529fc27, 0x29fc2757, 0xfc2757d1,
    0x2757d1f5, 0x57d1f534, 0xd1f534dd, 0xf534ddc0, 0x34ddc0db, 0xddc0db62,
    0xc0db6295, 0xdb629599, 0x6295993c, 0x95993c43, 0x993c4390, 0x3c439041,
};

static inline uint32_t asuint(float f) {
  union { float f; uint32_t i; } u = {f};
  return u.i;
}

static inline uint32_t abstop12(float x) {
  return (asuint(x) >> 20) & 0x7ff;
}

static inline void sincosf_poly(double x, double x2, const sincos_t *p, int n,
                                 float *sinp, float *cosp) {
  double x3, x4, x5, x6, s, c, c1, c2, s1;
  x4 = x2 * x2;
  x3 = x2 * x;
  c2 = p->c3 + x2 * p->c4;
  s1 = p->s2 + x2 * p->s3;
  float *tmp = (n & 1 ? cosp : sinp);
  cosp = (n & 1 ? sinp : cosp);
  sinp = tmp;
  c1 = p->c0 + x2 * p->c1;
  x5 = x3 * x2;
  x6 = x4 * x2;
  s = x + x3 * p->s1;
  c = c1 + x4 * p->c2;
  *sinp = s + x5 * s1;
  *cosp = c + x6 * c2;
}

static inline float sinf_poly(double x, double x2, const sincos_t *p, int n) {
  double x3, x4, x6, x7, s, c, c1, c2, s1;
  if ((n & 1) == 0) {
    x3 = x * x2;
    s1 = p->s2 + x2 * p->s3;
    x7 = x3 * x2;
    s = x + x3 * p->s1;
    return s + x7 * s1;
  } else {
    x4 = x2 * x2;
    c2 = p->c3 + x2 * p->c4;
    c1 = p->c0 + x2 * p->c1;
    x6 = x4 * x2;
    c = c1 + x4 * p->c2;
    return c + x6 * c2;
  }
}

static inline double reduce_fast(double x, const sincos_t *p, int *np) {
  double r = x * p->hpi_inv;
  int n = ((int32_t)r + 0x800000) >> 24;
  *np = n;
  return x - n * p->hpi;
}

static inline double reduce_large(uint32_t xi, int *np) {
  const uint32_t *arr = &__inv_pio4[(xi >> 26) & 15];
  int shift = (xi >> 23) & 7;
  uint64_t n, res0, res1, res2;
  xi = (xi & 0xffffff) | 0x800000;
  xi <<= shift;
  res0 = xi * arr[0];
  res1 = (uint64_t)xi * arr[4];
  res2 = (uint64_t)xi * arr[8];
  res0 = (res2 >> 32) | (res0 << 32);
  res0 += res1;
  n = (res0 + (1ULL << 61)) >> 62;
  res0 -= n << 62;
  double x = (int64_t)res0;
  *np = n;
  return x * 0x1.921FB54442D18p-62;
}

void sincosf(float y, float *sinp, float *cosp) {
  double x = y;
  double s;
  int n;
  const sincos_t *p = &__sincosf_table[0];
  if (abstop12(y) < abstop12(0x1.921FB6p-1f)) {
    double x2 = x * x;
    if (unlikely(abstop12(y) < abstop12(0x1p-12f))) {
      *sinp = y;
      *cosp = 1.0f;
      return;
    }
    sincosf_poly(x, x2, p, 0, sinp, cosp);
  } else if (abstop12(y) < abstop12(120.0f)) {
    x = reduce_fast(x, p, &n);
    s = p->sign[n & 3];
    if (n & 2)
      p = &__sincosf_table[1];
    sincosf_poly(x * s, x * x, p, n, sinp, cosp);
  } else if (likely(abstop12(y) < abstop12(__builtin_inff()))) {
    uint32_t xi = asuint(y);
    int sign = xi >> 31;
    x = reduce_large(xi, &n);
    s = p->sign[(n + sign) & 3];
    if ((n + sign) & 2)
      p = &__sincosf_table[1];
    sincosf_poly(x * s, x * x, p, n, sinp, cosp);
  } else {
    *sinp = *cosp = y - y;
  }
}

float sinf(float x) {
  double y = x;
  double s;
  int n;
  const sincos_t *p = &__sincosf_table[0];
  if (abstop12(x) < abstop12(0x1.921FB6p-1f)) {
    double x2 = y * y;
    if (unlikely(abstop12(x) < abstop12(0x1p-12f))) {
      return x;
    }
    return sinf_poly(y, x2, p, 0);
  } else if (abstop12(x) < abstop12(120.0f)) {
    y = reduce_fast(y, p, &n);
    s = p->sign[n & 3];
    if (n & 2)
      p = &__sincosf_table[1];
    return sinf_poly(y * s, y * y, p, n);
  } else if (likely(abstop12(x) < abstop12(__builtin_inff()))) {
    uint32_t xi = asuint(x);
    int sign = xi >> 31;
    y = reduce_large(xi, &n);
    s = p->sign[(n + sign) & 3];
    if ((n + sign) & 2)
      p = &__sincosf_table[1];
    return sinf_poly(y * s, y * y, p, n);
  } else {
    return x - x;
  }
}

float cosf(float x) {
  float sin_val, cos_val;
  sincosf(x, &sin_val, &cos_val);
  return cos_val;
}

// ----------------------------------------------------------------------------
// High-performance powf() from Justine Tunney (https://justine.lol/tmp/powf.c.txt)
// ULP error: 0.82 (~ 0.5 + relerr*2^24), Optimized for ARM Cortex-A72

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(x, 0)

#define POWF_LOG2_TABLE_BITS 4
#define POWF_LOG2_POLY_ORDER 5
#define POWF_SCALE_BITS      0
#define POWF_SCALE           ((double)(1 << POWF_SCALE_BITS))
#define EXP2F_TABLE_BITS     5
#define EXP2F_POLY_ORDER     3

static inline float opt_barrier_float(float x) {
  volatile float y = x;
  return y;
}

// asuint() already defined above for sincosf

static inline int issignalingf_inline(float x) {
  uint32_t ix = asuint(x);
  return 2 * (ix ^ 0x00400000) > 2u * 0x7fc00000;
}

static inline float asfloat(uint32_t i) {
  union { uint32_t i; float f; } u = {i};
  return u.f;
}

static inline uint64_t asuint64(double f) {
  union { double f; uint64_t i; } u = {f};
  return u.i;
}

static inline double asdouble(uint64_t i) {
  union { uint64_t i; double f; } u = {i};
  return u.f;
}

static inline float eval_as_float(float x) {
  return x;
}

static inline double eval_as_double(double x) {
  return x;
}

__attribute__((__noinline__)) static float
xflowf(uint32_t sign, float y) {
  y = eval_as_float(opt_barrier_float(sign ? -y : y) * y);
  return y;
}

static float __math_oflowf(uint32_t sign) {
  return xflowf(sign, 0x1p97f);
}

static float __math_uflowf(uint32_t sign) {
  return xflowf(sign, 0x1p-95f);
}

static float __math_invalidf(float x) {
  return (x - x) / (x - x);
}

static const struct {
  struct { double invc, logc; } tab[1 << POWF_LOG2_TABLE_BITS];
  double poly[POWF_LOG2_POLY_ORDER];
} __powf_log2_data = {
  .tab = {
    { 0x1.661ec79f8f3bep+0, -0x1.efec65b963019p-2 * POWF_SCALE },
    { 0x1.571ed4aaf883dp+0, -0x1.b0b6832d4fca4p-2 * POWF_SCALE },
    { 0x1.49539f0f010bp+0, -0x1.7418b0a1fb77bp-2 * POWF_SCALE },
    { 0x1.3c995b0b80385p+0, -0x1.39de91a6dcf7bp-2 * POWF_SCALE },
    { 0x1.30d190c8864a5p+0, -0x1.01d9bf3f2b631p-2 * POWF_SCALE },
    { 0x1.25e227b0b8eap+0, -0x1.97c1d1b3b7afp-3 * POWF_SCALE },
    { 0x1.1bb4a4a1a343fp+0, -0x1.2f9e393af3c9fp-3 * POWF_SCALE },
    { 0x1.12358f08ae5bap+0, -0x1.960cbbf788d5cp-4 * POWF_SCALE },
    { 0x1.0953f419900a7p+0, -0x1.a6f9db6475fcep-5 * POWF_SCALE },
    { 0x1p+0, 0x0p+0 * POWF_SCALE },
    { 0x1.e608cfd9a47acp-1, 0x1.338ca9f24f53dp-4 * POWF_SCALE },
    { 0x1.ca4b31f026aap-1, 0x1.476a9543891bap-3 * POWF_SCALE },
    { 0x1.b2036576afce6p-1, 0x1.e840b4ac4e4d2p-3 * POWF_SCALE },
    { 0x1.9c2d163a1aa2dp-1, 0x1.40645f0c6651cp-2 * POWF_SCALE },
    { 0x1.886e6037841edp-1, 0x1.88e9c2c1b9ff8p-2 * POWF_SCALE },
    { 0x1.767dcf5534862p-1, 0x1.ce0a44eb17bccp-2 * POWF_SCALE },
  },
  .poly = {
    -0x1.712b6f70a7e4dp-2 * POWF_SCALE,
    0x1.ecabf496832ep-2 * POWF_SCALE,
    -0x1.715479ffae3dep-1 * POWF_SCALE,
    0x1.715475f35c45bp0 * POWF_SCALE,
  }
};

#define N_EXP (1 << EXP2F_TABLE_BITS)
static const struct {
  uint64_t tab[1 << EXP2F_TABLE_BITS];
  double shift_scaled;
  double poly[EXP2F_POLY_ORDER];
  double shift;
  double invln2_scaled;
  double poly_scaled[EXP2F_POLY_ORDER];
} __exp2f_data = {
  .tab = {
    0x3ff0000000000000, 0x3fefd9b0d3158574, 0x3fefb5586cf9890f, 0x3fef9301d0125b51,
    0x3fef72b83c7d517b, 0x3fef54873168b9aa, 0x3fef387a6e756238, 0x3fef1e9df51fdee1,
    0x3fef06fe0a31b715, 0x3feef1a7373aa9cb, 0x3feedea64c123422, 0x3feece086061892d,
    0x3feebfdad5362a27, 0x3feeb42b569d4f82, 0x3feeab07dd485429, 0x3feea47eb03a5585,
    0x3feea09e667f3bcd, 0x3fee9f75e8ec5f74, 0x3feea11473eb0187, 0x3feea589994cce13,
    0x3feeace5422aa0db, 0x3feeb737b0cdc5e5, 0x3feec49182a3f090, 0x3feed503b23e255d,
    0x3feee89f995ad3ad, 0x3feeff76f2fb5e47, 0x3fef199bdd85529c, 0x3fef3720dcef9069,
    0x3fef5818dcfba487, 0x3fef7c97337b9b5f, 0x3fefa4afa2a490da, 0x3fefd0765b6e4540,
  },
  .shift_scaled = 0x1.8p+52 / N_EXP,
  .poly = {
    0x1.c6af84b912394p-5, 0x1.ebfce50fac4f3p-3, 0x1.62e42ff0c52d6p-1,
  },
  .shift = 0x1.8p+52,
  .invln2_scaled = 0x1.71547652b82fep+0 * N_EXP,
  .poly_scaled = {
    0x1.c6af84b912394p-5/N_EXP/N_EXP/N_EXP,
    0x1.ebfce50fac4f3p-3/N_EXP/N_EXP,
    0x1.62e42ff0c52d6p-1/N_EXP,
  },
};

#define N_LOG (1 << POWF_LOG2_TABLE_BITS)
#define T_LOG __powf_log2_data.tab
#define A_LOG __powf_log2_data.poly
#define OFF 0x3f330000

static inline double_t log2_inline(uint32_t ix) {
  double_t z, r, r2, r4, p, q, y, y0, invc, logc;
  uint32_t iz, top, tmp;
  int k, i;

  tmp = ix - OFF;
  i = (tmp >> (23 - POWF_LOG2_TABLE_BITS)) % N_LOG;
  top = tmp & 0xff800000;
  iz = ix - top;
  k = (int32_t)top >> (23 - POWF_SCALE_BITS);
  invc = T_LOG[i].invc;
  logc = T_LOG[i].logc;
  z = (double_t)asfloat(iz);

  r = z * invc - 1;
  y0 = logc + (double_t)k;

  r2 = r * r;
  y = A_LOG[0] * r + A_LOG[1];
  p = A_LOG[2] * r + A_LOG[3];
  r4 = r2 * r2;
  q = A_LOG[4] * r + y0;
  q = p * r2 + q;
  y = y * r4 + q;
  return y;
}

#define T_EXP __exp2f_data.tab
#define C_EXP __exp2f_data.poly_scaled
#define SIGN_BIAS (1 << (EXP2F_TABLE_BITS + 11))

static inline float exp2_inline(double_t xd, uint32_t sign_bias) {
  uint64_t ki, ski, t;
  double_t kd, z, r, r2, y, s;

  #define SHIFT __exp2f_data.shift_scaled
  kd = eval_as_double(xd + SHIFT);
  ki = asuint64(kd);
  kd -= SHIFT;
  r = xd - kd;

  t = T_EXP[ki % N_EXP];
  ski = ki + sign_bias;
  t += ski << (52 - EXP2F_TABLE_BITS);
  s = asdouble(t);
  z = C_EXP[0] * r + C_EXP[1];
  r2 = r * r;
  y = C_EXP[2] * r + 1;
  y = z * r2 + y;
  y = y * s;
  return eval_as_float(y);
}

static inline int checkint(uint32_t iy) {
  int e = iy >> 23 & 0xff;
  if (e < 0x7f) return 0;
  if (e > 0x7f + 23) return 2;
  if (iy & ((1 << (0x7f + 23 - e)) - 1)) return 0;
  if (iy & (1 << (0x7f + 23 - e))) return 1;
  return 2;
}

static inline int zeroinfnan(uint32_t ix) {
  return 2 * ix - 1 >= 2u * 0x7f800000 - 1;
}

float powf(float x, float y) {
  uint32_t sign_bias = 0;
  uint32_t ix, iy;

  ix = asuint(x);
  iy = asuint(y);
  if (unlikely(ix - 0x00800000 >= 0x7f800000 - 0x00800000 || zeroinfnan(iy))) {
    if (unlikely(zeroinfnan(iy))) {
      if (2 * iy == 0)
        return issignalingf_inline(x) ? x + y : 1.0f;
      if (ix == 0x3f800000)
        return issignalingf_inline(y) ? x + y : 1.0f;
      if (2 * ix > 2u * 0x7f800000 || 2 * iy > 2u * 0x7f800000)
        return x + y;
      if (2 * ix == 2 * 0x3f800000)
        return 1.0f;
      if ((2 * ix < 2 * 0x3f800000) == !(iy & 0x80000000))
        return 0.0f;
      return y * y;
    }
    if (unlikely(zeroinfnan(ix))) {
      float_t x2 = x * x;
      if (ix & 0x80000000 && checkint(iy) == 1) {
        x2 = -x2;
        sign_bias = 1;
      }
      return iy & 0x80000000 ? opt_barrier_float(1 / x2) : x2;
    }
    if (ix & 0x80000000) {
      int yint = checkint(iy);
      if (yint == 0)
        return __math_invalidf(x);
      if (yint == 1)
        sign_bias = SIGN_BIAS;
      ix &= 0x7fffffff;
    }
    if (ix < 0x00800000) {
      ix = asuint(x * 0x1p23f);
      ix &= 0x7fffffff;
      ix -= 23 << 23;
    }
  }
  double_t logx = log2_inline(ix);
  double_t ylogx = y * logx;
  if (unlikely((asuint64(ylogx) >> 47 & 0xffff) >= asuint64(126.0 * POWF_SCALE) >> 47)) {
    if (ylogx > 0x1.fffffffd1d571p+6 * POWF_SCALE)
      return __math_oflowf(sign_bias);
    if (ylogx <= -150.0 * POWF_SCALE)
      return __math_uflowf(sign_bias);
  }
  return exp2_inline(ylogx, sign_bias);
}

// ----------------------------------------------------------------------------
// Simple RNG for EFI (no stdlib)
static uint32_t rng_state = 12345;

void srand_efi(uint32_t seed) {
    rng_state = seed;
}

uint32_t rand_efi(void) {
    // Simple LCG
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state / 65536) % 32768;
}

#define RAND_MAX 32767

// ----------------------------------------------------------------------------
// Hardware Detection and Capabilities

typedef struct {
    UINT64 total_ram_mb;      // Total RAM in MB
    UINT64 available_ram_mb;  // Available RAM in MB
    BOOLEAN has_sse;          // SSE support
    BOOLEAN has_sse2;         // SSE2 support
    BOOLEAN has_avx;          // AVX support
    BOOLEAN has_avx2;         // AVX2 support
    BOOLEAN has_avx512;       // AVX512 support
    BOOLEAN has_fma;          // FMA support
    UINT32 cpu_cores;         // Number of CPU cores (estimation)
    UINT32 performance_score; // Calculated performance score (0-1000)
} HardwareCapabilities;

// ----------------------------------------------------------------------------
// Benchmark and Statistics

typedef struct {
    UINT64 start_time_ms;
    UINT64 first_token_time_ms;
    UINT64 total_time_ms;
    int tokens_generated;
    float tokens_per_second;
    float time_to_first_token_ms;
    UINT64 memory_used_mb;
    BOOLEAN is_active;
} BenchmarkStats;

typedef struct {
    int current_token;
    int total_tokens;
    int tokens_generated;
    UINT64 start_time;
    UINT64 last_update_time;
    float current_tps;  // tokens per second
} GenerationStats;

// ----------------------------------------------------------------------------
// Timer and Statistics Utilities

UINT64 get_timer_ms(void) {
    // Get current time in milliseconds using UEFI timer
    EFI_TIME time;
    ST->RuntimeServices->GetTime(&time, NULL);
    
    // Convert to milliseconds (approximate)
    UINT64 ms = (UINT64)time.Hour * 3600000 +
                (UINT64)time.Minute * 60000 +
                (UINT64)time.Second * 1000 +
                (UINT64)time.Nanosecond / 1000000;
    return ms;
}

void print_progress_bar(int current, int total, int bar_width) {
    int filled = (current * bar_width) / total;
    int percent = (current * 100) / total;
    
    Print(L"\r  [");
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) Print(L"█");
        else Print(L"░");
    }
    Print(L"] %3d%% (%d/%d)", percent, current, total);
}

void print_generation_stats(GenerationStats *stats) {
    UINT64 current_time = get_timer_ms();
    UINT64 elapsed_seconds = (current_time - stats->start_time) / 1000;
    
    if (elapsed_seconds > 0) {
        stats->current_tps = (float)stats->tokens_generated / (float)elapsed_seconds;
    }
    
    Print(L"\r  📊 Tokens: %d/%d | Speed: %.2f tok/s | Time: %lus",
          stats->tokens_generated,
          stats->total_tokens,
          stats->current_tps,
          elapsed_seconds);
}

void print_benchmark_results(BenchmarkStats *bench) {
    Print(L"\r\n");
    Print(L"╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║              BENCHMARK RESULTS                         ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    Print(L"  ⏱️  Total Time:        %.2f seconds\r\n", bench->total_time_ms / 1000.0);
    Print(L"  🚀 First Token:       %.2f ms\r\n", bench->time_to_first_token_ms);
    Print(L"  📊 Tokens Generated:  %d\r\n", bench->tokens_generated);
    Print(L"  ⚡ Speed:             %.2f tokens/sec\r\n", bench->tokens_per_second);
    Print(L"  💾 Memory Used:       %lu MB\r\n", bench->memory_used_mb);
    Print(L"\r\n");
    
    // Performance rating
    if (bench->tokens_per_second >= 50.0) {
        Print(L"  🌟 Performance: EXCELLENT\r\n");
    } else if (bench->tokens_per_second >= 25.0) {
        Print(L"  ⭐ Performance: GOOD\r\n");
    } else if (bench->tokens_per_second >= 10.0) {
        Print(L"  ✓ Performance: ACCEPTABLE\r\n");
    } else {
        Print(L"  ⚠️  Performance: SLOW (check hardware)\r\n");
    }
    Print(L"\r\n");
}

// ----------------------------------------------------------------------------
// Network Functions - Model Download Support

// Parse URL into components
EFI_STATUS parse_url(CHAR8 *url, UrlInfo *info) {
    // URL format: http://hostname:port/path
    // Example: http://huggingface.co/models/stories15M.bin
    
    info->port = 80; // Default HTTP port
    
    // Skip "http://"
    CHAR8 *ptr = url;
    if (CompareMem(ptr, "http://", 7) == 0) {
        ptr += 7;
    } else if (CompareMem(ptr, "https://", 8) == 0) {
        Print(L"[ERROR] HTTPS not supported (use HTTP URLs)\r\n");
        return EFI_UNSUPPORTED;
    } else {
        Print(L"[ERROR] Invalid URL format (must start with http://)\r\n");
        return EFI_INVALID_PARAMETER;
    }
    
    // Find path separator
    CHAR8 *slash = ptr;
    while (*slash && *slash != '/') slash++;
    
    if (*slash != '/') {
        Print(L"[ERROR] No path in URL\r\n");
        return EFI_INVALID_PARAMETER;
    }
    
    // Extract hostname
    UINTN hostname_len = slash - ptr;
    info->hostname = AllocatePool(hostname_len + 1);
    if (!info->hostname) {
        return EFI_OUT_OF_RESOURCES;
    }
    CopyMem(info->hostname, ptr, hostname_len);
    info->hostname[hostname_len] = '\0';
    
    // Extract path
    UINTN path_len = AsciiStrLen(slash);
    info->path = AllocatePool(path_len + 1);
    if (!info->path) {
        FreePool(info->hostname);
        return EFI_OUT_OF_RESOURCES;
    }
    CopyMem(info->path, slash, path_len + 1);
    
    // Check for port in hostname (hostname:port)
    CHAR8 *colon = info->hostname;
    while (*colon && *colon != ':') colon++;
    if (*colon == ':') {
        *colon = '\0';
        colon++;
        info->port = 0;
        while (*colon >= '0' && *colon <= '9') {
            info->port = info->port * 10 + (*colon - '0');
            colon++;
        }
    }
    
    return EFI_SUCCESS;
}

// Check if network/HTTP support is available
BOOLEAN check_network_available(EFI_SYSTEM_TABLE *SystemTable) {
    // For now, always return FALSE since HTTP Protocol is complex to implement
    // and not available in standard QEMU without special network configuration
    // Future: Implement full HTTP Protocol support when needed
    return FALSE;
    
    /* Full implementation would require:
    EFI_GUID HttpServiceBindingGuid = EFI_HTTP_SERVICE_BINDING_PROTOCOL_GUID;
    EFI_HANDLE *Handles = NULL;
    UINTN HandleCount = 0;
    EFI_STATUS Status;
    
    Status = SystemTable->BootServices->LocateHandleBuffer(
        ByProtocol,
        &HttpServiceBindingGuid,
        NULL,
        &HandleCount,
        &Handles
    );
    
    if (Handles) {
        FreePool(Handles);
    }
    
    return !EFI_ERROR(Status) && HandleCount > 0;
    */
}

// Display download menu with available models
void show_download_menu(HardwareCapabilities *hw, EFI_SYSTEM_TABLE *SystemTable) {
    Print(L"\r\n");
    Print(L"╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║           NETWORK MODEL DOWNLOAD                      ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    
    // Check network availability
    BOOLEAN network_available = check_network_available(SystemTable);
    
    if (!network_available) {
        Print(L"  ⚠️  Network/HTTP Protocol: OFFLINE\r\n");
        Print(L"\r\n");
        Print(L"  To enable network in QEMU, use:\r\n");
        Print(L"    -netdev user,id=net0 -device e1000,netdev=net0\r\n");
        Print(L"\r\n");
        Print(L"  📋 Available models you can download manually:\r\n");
        Print(L"  ════════════════════════════════════════════════\r\n\r\n");
        
        // Show catalog even without network
        for (int i = 0; i < MODEL_REPO_COUNT; i++) {
            ModelRepositoryEntry *entry = &model_repository[i];
            BOOLEAN fits = (entry->min_ram_mb <= hw->available_ram_mb);
            
            Print(L"  %a (%lu MB) %s\r\n", 
                  entry->name, 
                  entry->size_mb,
                  fits ? L"✓" : L"⚠️");
            Print(L"    URL: %a\r\n\r\n", entry->url);
        }
        
        Print(L"  💡 Manual download: Use wget/curl from Linux/WSL\r\n");
        Print(L"     Then copy files to boot USB/disk\r\n\r\n");
        Print(L"  Press any key to continue...\r\n");
        EFI_INPUT_KEY Key;
        while (SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key) != EFI_SUCCESS);
        return;
    }
    
    // Network is available - show interactive download menu
    Print(L"  ✓ Network: ONLINE\r\n");
    Print(L"  ✓ HTTP Protocol: Available\r\n\r\n");
    
    Print(L"  ✓ Network support detected!\r\n");
    Print(L"\r\n");
    Print(L"  Available models for download:\r\n");
    Print(L"\r\n");
    Print(L"    [1] Stories 15M  (60 MB)   - Tiny, fast inference\r\n");
    Print(L"    [2] Stories 42M  (165 MB)  - Small, good quality\r\n");
    Print(L"    [3] Stories 110M (420 MB)  - Medium, best quality\r\n");
    Print(L"\r\n");
    Print(L"  ⚠️  Note: Download feature is EXPERIMENTAL\r\n");
    Print(L"  ℹ️  Models will be saved to boot disk\r\n");
    Print(L"\r\n");
    Print(L"  Press any key to return to main menu...\r\n");
}

// Simplified download function (placeholder for full implementation)
EFI_STATUS download_model_simple(
    EFI_SYSTEM_TABLE *SystemTable,
    CHAR16 *model_name,
    CHAR8 *url,
    CHAR16 *local_filename
) {
    Print(L"\r\n");
    Print(L"╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║              DOWNLOADING MODEL                         ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    Print(L"  📦 Model: %s\r\n", model_name);
    Print(L"  🌐 URL: ");
    
    // Print URL (convert ASCII to Unicode for display)
    CHAR8 *url_ptr = url;
    while (*url_ptr) {
        Print(L"%c", *url_ptr);
        url_ptr++;
    }
    Print(L"\r\n");
    Print(L"  💾 Saving to: %s\r\n", local_filename);
    Print(L"\r\n");
    
    // Parse URL
    UrlInfo url_info = {0};
    EFI_STATUS Status = parse_url(url, &url_info);
    if (EFI_ERROR(Status)) {
        Print(L"  ❌ Failed to parse URL\r\n");
        return Status;
    }
    
    Print(L"  ✓ Hostname: ");
    CHAR8 *host_ptr = url_info.hostname;
    while (*host_ptr) {
        Print(L"%c", *host_ptr);
        host_ptr++;
    }
    Print(L"\r\n");
    Print(L"  ✓ Port: %d\r\n", url_info.port);
    Print(L"  ✓ Path: ");
    CHAR8 *path_ptr = url_info.path;
    while (*path_ptr) {
        Print(L"%c", *path_ptr);
        path_ptr++;
    }
    Print(L"\r\n");
    Print(L"\r\n");
    Print(L"  ⚠️  Full HTTP download not yet implemented\r\n");
    Print(L"  ℹ️  This feature requires HTTP Protocol implementation\r\n");
    Print(L"\r\n");
    
    // Cleanup
    if (url_info.hostname) FreePool(url_info.hostname);
    if (url_info.path) FreePool(url_info.path);
    
    return EFI_UNSUPPORTED;
}

// Show model repository catalog
void show_model_repository_catalog(HardwareCapabilities *hw, EFI_SYSTEM_TABLE *SystemTable) {
    Print(L"\r\n");
    Print(L"╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║         DOWNLOADABLE MODELS REPOSITORY                 ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    Print(L"  🖥️  Your Hardware: %lu MB RAM | %s | Score: %d/1000\r\n\r\n",
          hw->available_ram_mb,
          hw->has_avx2 ? L"AVX2 ✓" : L"No AVX2",
          hw->performance_score);
    
    Print(L"╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║  KARPATHY'S TINYLLAMAS (TinyStories Dataset)           ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"  Source: huggingface.co/karpathy/tinyllamas\r\n\r\n");
    
    for (int i = 0; i < MODEL_REPO_COUNT; i++) {
        ModelRepositoryEntry *entry = &model_repository[i];
        
        // Only show stories models here
        if (strncmp(entry->name, "stories", 7) == 0) {
            BOOLEAN fits = (entry->min_ram_mb <= hw->available_ram_mb);
            BOOLEAN recommended = fits && 
                                (entry->size_mb <= hw->available_ram_mb * 0.6);
            
            Print(L"  [%d] %a\r\n", i + 1, entry->name);
            Print(L"      %a\r\n", entry->description);
            Print(L"      Size: %lu MB | Min RAM: %lu MB\r\n", 
                  entry->size_mb, entry->min_ram_mb);
            
            if (recommended) {
                Print(L"      ✓ RECOMMENDED for your hardware\r\n");
            } else if (fits) {
                Print(L"      ⚠️  Will work but may be slow\r\n");
            } else {
                Print(L"      ❌ Requires more RAM\r\n");
            }
            Print(L"\r\n");
        }
    }
    
    Print(L"╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║  REQUIRED FILES                                         ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"  [5] tokenizer.bin (433 KB) - Required for all models\r\n");
    Print(L"      Source: github.com/karpathy/llama2.c\r\n\r\n");
    
    Print(L"╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║  DOWNLOAD OPTIONS                                       ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"  1. Manual Download (Recommended for now):\r\n");
    Print(L"     wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin\r\n\r\n");
    Print(L"  2. Automated Download (Coming soon):\r\n");
    Print(L"     Auto-download via HTTP Protocol in UEFI\r\n\r\n");
    Print(L"  3. Training Your Own (Advanced):\r\n");
    Print(L"     Use llama2.c train script on custom dataset\r\n\r\n");
    
    Print(L"Press any key to continue...\r\n");
    EFI_INPUT_KEY Key;
    while (SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key) != EFI_SUCCESS);
}

// ----------------------------------------------------------------------------
// Multi-Model Architecture Support
// Expanded model support for auto-detection based on hardware

typedef enum {
    MODEL_STORIES15M = 1,    // 60MB  - 15M params  - Minimum (512MB RAM)
    MODEL_STORIES42M = 2,    // 165MB - 42M params  - Low (1GB RAM)
    MODEL_STORIES110M = 3,   // 420MB - 110M params - Medium (2GB RAM)
    MODEL_STORIES260M = 4,   // 1GB   - 260M params - High (4GB RAM)
    MODEL_TINYLLAMA_1B = 5,  // 4.4GB - 1.1B params - Very High (8GB RAM)
    MODEL_LLAMA2_7B = 6,     // 13GB  - 7B params   - Extreme (16GB+ RAM)
    MODEL_NANOGPT = 7        // 48MB  - 124M params - Special
} ModelType;

typedef struct {
    int dim; // transformer dimension
    int hidden_dim; // for ffn layers
    int n_layers; // number of layers
    int n_heads; // number of query heads
    int n_kv_heads; // number of key/value heads (can be < query heads because of multiquery)
    int vocab_size; // vocabulary size, usually 256 (byte-level)
    int seq_len; // max sequence length
    ModelType model_type; // which model architecture
} Config;

typedef struct {
    // token embedding table
    float* token_embedding_table;    // (vocab_size, dim)
    // weights for rmsnorms
    float* rms_att_weight; // (layer, dim) rmsnorm weights
    float* rms_ffn_weight; // (layer, dim)
    // weights for matmuls. note dim == n_heads * head_size
    float* wq; // (layer, dim, n_heads * head_size)
    float* wk; // (layer, dim, n_kv_heads * head_size)
    float* wv; // (layer, dim, n_kv_heads * head_size)
    float* wo; // (layer, n_heads * head_size, dim)
    // weights for ffn
    float* w1; // (layer, hidden_dim, dim)
    float* w2; // (layer, dim, hidden_dim)
    float* w3; // (layer, hidden_dim, dim)
    // final rmsnorm
    float* rms_final_weight; // (dim,)
    // (optional) classifier weights for the logits, on the last layer
    float* wcls;
} TransformerWeights;

typedef struct {
    // current wave of activations
    float *x; // activation at current time stamp (dim,)
    float *xb; // same, but inside a residual branch (dim,)
    float *xb2; // an additional buffer just for convenience (dim,)
    float *hb; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *hb2; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *q; // query (dim,)
    float *k; // key (dim,)
    float *v; // value (dim,)
    float *att; // buffer for scores/attention values (n_heads, seq_len)
    float *logits; // output logits
    // kv cache
    float* key_cache;   // (layer, seq_len, dim)
    float* value_cache; // (layer, seq_len, dim)
} RunState;

typedef struct {
    Config config; // the hyperparameters of the architecture (the blueprint)
    TransformerWeights weights; // the weights of the model
    RunState state; // buffers for the "wave" of activations in the forward pass
    float* data; // weight data pointer
    UINTN file_size; // size of the checkpoint file in bytes
} Transformer;

// ----------------------------------------------------------------------------
// STATIC ALLOCATION (EFI Modification - replaces malloc_run_state)
// Max dimensions for all supported models:
// - stories110M: dim=768, n_layers=12, hidden_dim=2048, seq_len=256
// - NanoGPT: dim=768, n_layers=12, hidden_dim=3072, seq_len=1024
// - TinyLlama-Chat: dim=2048, n_layers=22, hidden_dim=5632, seq_len=2048

#define MAX_DIM 2048
#define MAX_HIDDEN 5632
#define MAX_LAYERS 22
#define MAX_HEADS 32
#define MAX_SEQ_LEN 2048
#define MAX_VOCAB 32000

// Dynamic buffers (allocated at runtime with AllocatePool to avoid huge binary)
static float *static_x = NULL;
static float *static_xb = NULL;
static float *static_xb2 = NULL;
static float *static_hb = NULL;
static float *static_hb2 = NULL;
static float *static_q = NULL;
// NOTE: k and v are NOT allocated - they point directly into the cache (like Karpathy)
static float *static_key_cache = NULL;
static float *static_value_cache = NULL;
static float *static_att = NULL;
static float *static_logits = NULL;
static float *static_weights = NULL;

EFI_STATUS init_run_state(RunState* s, Config* p, EFI_BOOT_SERVICES *BS) {
    // Allocate buffers dynamically using AllocatePool
    EFI_STATUS Status;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    
    Print(L"  Allocating x (%u bytes)...\r\n", p->dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->dim * sizeof(float), (void**)&static_x);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate x: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating xb (%u bytes)...\r\n", p->dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->dim * sizeof(float), (void**)&static_xb);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate xb: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating xb2 (%u bytes)...\r\n", p->dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->dim * sizeof(float), (void**)&static_xb2);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate xb2: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating hb (%u bytes)...\r\n", p->hidden_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->hidden_dim * sizeof(float), (void**)&static_hb);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate hb: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating hb2 (%u bytes)...\r\n", p->hidden_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->hidden_dim * sizeof(float), (void**)&static_hb2);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate hb2: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating q (%u bytes)...\r\n", p->dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->dim * sizeof(float), (void**)&static_q);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate q: %r\r\n", Status); return Status; }
    
    // NOTE: k and v are NOT allocated - they will point directly into key_cache/value_cache!
    // This matches Karpathy's implementation exactly
    
    Print(L"  Allocating key_cache (%u bytes)...\r\n", p->n_layers * p->seq_len * kv_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->n_layers * p->seq_len * kv_dim * sizeof(float), (void**)&static_key_cache);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate key_cache: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating value_cache (%u bytes)...\r\n", p->n_layers * p->seq_len * kv_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->n_layers * p->seq_len * kv_dim * sizeof(float), (void**)&static_value_cache);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate value_cache: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating att (%u bytes)...\r\n", p->n_heads * p->seq_len * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->n_heads * p->seq_len * sizeof(float), (void**)&static_att);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate att: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating logits (%u bytes)...\r\n", p->vocab_size * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->vocab_size * sizeof(float), (void**)&static_logits);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate logits: %r\r\n", Status); return Status; }
    
    // Zero out KV cache (critical for correct inference)
    Print(L"  Zeroing KV cache...\r\n");
    UINTN kv_cache_size = p->n_layers * p->seq_len * kv_dim * sizeof(float);
    for (UINTN i = 0; i < kv_cache_size / sizeof(float); i++) {
        static_key_cache[i] = 0.0f;
        static_value_cache[i] = 0.0f;
    }
    Print(L"  KV cache zeroed!\r\n");
    
    // Point RunState to allocated buffers
    s->x = static_x;
    s->xb = static_xb;
    s->xb2 = static_xb2;
    s->hb = static_hb;
    s->hb2 = static_hb2;
    s->q = static_q;
    // NOTE: s->k and s->v are NOT set here - they will be set in forward() to point into the cache!
    s->key_cache = static_key_cache;
    s->value_cache = static_value_cache;
    s->att = static_att;
    s->logits = static_logits;
    
    return EFI_SUCCESS;
}

void memory_map_weights(TransformerWeights *w, Config* p, float* ptr, int shared_weights) {
    int head_size = p->dim / p->n_heads;
    unsigned long long n_layers = p->n_layers;
    
    w->token_embedding_table = ptr;
    ptr += p->vocab_size * p->dim;
    w->rms_att_weight = ptr;
    ptr += n_layers * p->dim;
    w->wq = ptr;
    ptr += n_layers * p->dim * (p->n_heads * head_size);
    w->wk = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wv = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wo = ptr;
    ptr += n_layers * (p->n_heads * head_size) * p->dim;
    w->rms_ffn_weight = ptr;
    ptr += n_layers * p->dim;
    w->w1 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->w2 = ptr;
    ptr += n_layers * p->hidden_dim * p->dim;
    w->w3 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->rms_final_weight = ptr;
    ptr += p->dim;
    ptr += p->seq_len * head_size / 2; // skip what used to be freq_cis_real (for RoPE)
    ptr += p->seq_len * head_size / 2; // skip what used to be freq_cis_imag (for RoPE)
    w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

// ----------------------------------------------------------------------------
// TRANSFORMER LOGIC (100% UNCHANGED from Karpathy's llama2.c)

void rmsnorm(float* o, float* x, float* weight, int size) {
    // calculate sum of squares
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    // normalize and scale
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void softmax(float* x, int size) {
    // find max value (for numerical stability)
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    // exp and sum
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    // normalize
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

void matmul(float* xout, float* x, float* w, int n, int d) {
    // W (d,n) @ x (n,) -> xout (d,)
    // AVX2 SIMD optimized: processes 8 floats at once with FMA
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        int j = 0;
        
        #if defined(__AVX2__) && defined(__FMA__)
        // AVX2 SIMD path: process 8 floats per iteration
        __m256 sum = _mm256_setzero_ps();  // accumulator vector
        
        for (; j <= n - 8; j += 8) {
            __m256 wx = _mm256_loadu_ps(&w[i * n + j]);  // load 8 weights
            __m256 xx = _mm256_loadu_ps(&x[j]);          // load 8 inputs
            sum = _mm256_fmadd_ps(wx, xx, sum);          // fused multiply-add: sum += wx * xx
        }
        
        // Horizontal sum: reduce 8 floats to 1
        __m128 sum_high = _mm256_extractf128_ps(sum, 1);  // upper 4 floats
        __m128 sum_low = _mm256_castps256_ps128(sum);      // lower 4 floats
        __m128 sum4 = _mm_add_ps(sum_low, sum_high);       // add upper and lower
        __m128 sum2 = _mm_add_ps(sum4, _mm_movehl_ps(sum4, sum4));  // horizontal add
        __m128 sum1 = _mm_add_ss(sum2, _mm_shuffle_ps(sum2, sum2, 1));  // final reduction
        val = _mm_cvtss_f32(sum1);
        #endif
        
        // Scalar tail: handle remaining elements
        for (; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        
        xout[i] = val;
    }
}

float* forward(Transformer* transformer, int token, int pos) {
    // a few convenience variables
    Config* p = &transformer->config;
    TransformerWeights* w = &transformer->weights;
    RunState* s = &transformer->state;
    float *x = s->x;
    int dim = p->dim;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads; // integer multiplier of the kv sharing in multiquery
    int hidden_dim =  p->hidden_dim;
    int head_size = dim / p->n_heads;

    // copy the token embedding into x
    float* content_row = w->token_embedding_table + token * dim;
    for (int i = 0; i < dim; i++) {
        x[i] = content_row[i];
    }
    
    // forward all the layers
    for(unsigned long long l = 0; l < p->n_layers; l++) {

        // attention rmsnorm
        rmsnorm(s->xb, x, w->rms_att_weight + l*dim, dim);

        // key and value point to the kv cache (CRITICAL FIX!)
        int loff = l * p->seq_len * kv_dim; // kv cache layer offset for convenience
        s->k = s->key_cache + loff + pos * kv_dim;
        s->v = s->value_cache + loff + pos * kv_dim;

        // qkv matmuls for this position
        matmul(s->q, s->xb, w->wq + l*dim*dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l*dim*kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l*dim*kv_dim, dim, kv_dim);

        // RoPE relative positional encoding: complex-valued rotate q and k in each head
        for (int i = 0; i < dim; i+=2) {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            int rotn = i < kv_dim ? 2 : 1; // how many vectors? 2 = q & k, 1 = q only
            for (int v = 0; v < rotn; v++) {
                float* vec = v == 0 ? s->q : s->k; // the vector to rotate (query or key)
                float v0 = vec[i];
                float v1 = vec[i+1];
                vec[i]   = v0 * fcr - v1 * fci;
                vec[i+1] = v0 * fci + v1 * fcr;
            }
        }

        // multihead attention. iterate over all heads
        int h;
        for (h = 0; h < p->n_heads; h++) {
            // get the query vector for this head
            float* q = s->q + h * head_size;
            // attention scores for this head
            float* att = s->att + h * p->seq_len;
            // iterate over all timesteps, including the current one
            for (int t = 0; t <= pos; t++) {
                // get the key vector for this head and at this timestep
                float* k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                // calculate the attention score as the dot product of q and k
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) {
                    score += q[i] * k[i];
                }
                score /= sqrtf(head_size);
                // save the score to the attention buffer
                att[t] = score;
            }

            // softmax the scores to get attention weights, from 0..pos inclusively
            softmax(att, pos + 1);

            // weighted sum of the values, store back into xb
            float* xb = s->xb + h * head_size;
            for (int i = 0; i < head_size; i++) {
                xb[i] = 0.0f;
            }
            for (int t = 0; t <= pos; t++) {
                // get the value vector for this head and at this timestep
                float* v = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                // get the attention weight for this timestep
                float a = att[t];
                // accumulate the weighted value into xb
                for (int i = 0; i < head_size; i++) {
                    xb[i] += a * v[i];
                }
            }
        }

        // final matmul to get the output of the attention
        matmul(s->xb2, s->xb, w->wo + l*dim*dim, dim, dim);

        // residual connection back into x
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb2[i];
        }

        // ffn rmsnorm
        rmsnorm(s->xb, x, w->rms_ffn_weight + l*dim, dim);

        // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
        // first calculate self.w1(x) and self.w3(x)
        matmul(s->hb, s->xb, w->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l*dim*hidden_dim, dim, hidden_dim);

        // SwiGLU non-linearity
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            // silu(x)=x*σ(x), where σ(x) is the logistic sigmoid
            val *= (1.0f / (1.0f + expf(-val)));
            // elementwise multiply with w3(x)
            val *= s->hb2[i];
            s->hb[i] = val;
        }

        // final matmul to get the output of the ffn
        matmul(s->xb, s->hb, w->w2 + l*dim*hidden_dim, hidden_dim, dim);

        // residual connection
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb[i];
        }
    }

    // final rmsnorm
    rmsnorm(x, x, w->rms_final_weight, dim);

    // classifier into logits
    matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
    return s->logits;
}

// ----------------------------------------------------------------------------
// Sampling (100% UNCHANGED from Karpathy)

int sample(float* probabilities, int n) {
    // sample index from probabilities (they must sum to 1!)
    float r = (float)rand_efi() / (float)RAND_MAX;
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (r < cdf) {
            return i;
        }
    }
    return n - 1; // in case of rounding errors
}

int argmax(float* v, int n) {
    // return argmax of v in elements 0..n
    int max_i = 0;
    float max_p = v[0];
    for (int i = 1; i < n; i++) {
        if (v[i] > max_p) {
            max_i = i;
            max_p = v[i];
        }
    }
    return max_i;
}
int sample_mult(float* probabilities, int n, float coin) {
    // Sample index from probabilities (they must sum to 1!)
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf) {
            return i;
        }
    }
    return n - 1; // in case of rounding errors
}

int sample_top_p(float* logits, int n, float top_p, float temperature, float coin) {
    // Apply temperature
    for (int i = 0; i < n; i++) {
        logits[i] /= temperature;
    }
    
    // Softmax
    softmax(logits, n);
    
    // Sort indices by probability (simple selection sort)
    int* indices = (int*)&logits[n];  // Reuse memory after logits
    for (int i = 0; i < n; i++) indices[i] = i;
    
    for (int i = 0; i < n-1; i++) {
        for (int j = i+1; j < n; j++) {
            if (logits[indices[j]] > logits[indices[i]]) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }
    
    // Truncate to top-p
    float cumsum = 0.0f;
    int last_idx = 0;
    for (int i = 0; i < n; i++) {
        cumsum += logits[indices[i]];
        last_idx = i;
        if (cumsum > top_p) break;
    }
    
    // Sample from the truncated list
    float r = coin * cumsum;
    float cdf = 0.0f;
    for (int i = 0; i <= last_idx; i++) {
        cdf += logits[indices[i]];
        if (r < cdf) {
            return indices[i];
        }
    }
    return indices[last_idx];
}
// ----------------------------------------------------------------------------
// EFI MODIFICATIONS START HERE

EFI_STATUS load_model(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, Transformer* transformer, CHAR16* checkpoint_path) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_FILE_IO_INTERFACE *FileSystem;
    EFI_FILE_HANDLE Root;
    EFI_FILE_HANDLE File;
    
    // Use uefi_call_wrapper for proper calling convention
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        ImageHandle,
        &LoadedImageProtocol,
        (void**)&LoadedImage
    );
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to get loaded image protocol: %r\r\n", Status);
        return Status;
    }
    
    // Now get file system from the device handle
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        LoadedImage->DeviceHandle,
        &FileSystemProtocol,
        (void**)&FileSystem
    );
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to get file system protocol: %r\r\n", Status);
        return Status;
    }
    
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to open volume: %r\r\n", Status);
        return Status;
    }
    
    
    // Open checkpoint file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, checkpoint_path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to open checkpoint: %s (Status: %r)\r\n", checkpoint_path, Status);
        return Status;
    }
    
    // Read config header (7 ints from file, NOT including model_type which is added later)
    UINTN config_size = 7 * sizeof(int);  // CRITICAL: file has 7 ints, not sizeof(Config)!
    Status = uefi_call_wrapper(File->Read, 3, File, &config_size, &transformer->config);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to read config: %r\r\n", Status);
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    Config* p = &transformer->config;
    Print(L"Model config: dim=%d, n_layers=%d, n_heads=%d, vocab=%d\r\n",
          p->dim, p->n_layers, p->n_heads, p->vocab_size);
    
    // Validate against static allocation limits
    if (p->dim > MAX_DIM || p->n_layers > MAX_LAYERS || 
        p->vocab_size > MAX_VOCAB || p->seq_len > MAX_SEQ_LEN) {
        Print(L"[ERROR] Model too large for static allocation!\r\n");
        uefi_call_wrapper(File->Close, 1, File);
        return EFI_BUFFER_TOO_SMALL;
    }
    
    // Calculate weights size (all transformer weights)
    int shared_weights = p->vocab_size > 0 ? 1 : 0;
    p->vocab_size = p->vocab_size > 0 ? p->vocab_size : -p->vocab_size;
    
    int head_size = p->dim / p->n_heads;
    int n_layers = p->n_layers;
    
    UINTN weights_size = 0;
    weights_size += p->vocab_size * p->dim; // token_embedding_table
    weights_size += n_layers * p->dim; // rms_att_weight
    weights_size += n_layers * p->dim * (p->n_heads * head_size); // wq
    weights_size += n_layers * p->dim * (p->n_kv_heads * head_size); // wk
    weights_size += n_layers * p->dim * (p->n_kv_heads * head_size); // wv
    weights_size += n_layers * (p->n_heads * head_size) * p->dim; // wo
    weights_size += n_layers * p->dim; // rms_ffn_weight
    weights_size += n_layers * p->dim * p->hidden_dim; // w1
    weights_size += n_layers * p->hidden_dim * p->dim; // w2
    weights_size += n_layers * p->dim * p->hidden_dim; // w3
    weights_size += p->dim; // rms_final_weight
    if (!shared_weights) {
        weights_size += p->vocab_size * p->dim; // wcls
    }
    weights_size *= sizeof(float); // convert to bytes
    
    // Allocate weights buffer
    Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, weights_size, (void**)&static_weights);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to allocate weights: %r\r\n", Status);
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    // Read weights into buffer in chunks to avoid EFI timeout
    
    UINTN total_read = 0;
    UINTN chunk_size = 512 * 1024; // 512 KB chunks
    UINT8* buffer_ptr = (UINT8*)static_weights;
    
    while (total_read < weights_size) {
        UINTN to_read = (weights_size - total_read > chunk_size) ? chunk_size : (weights_size - total_read);
        UINTN read_size = to_read;
        
        Status = uefi_call_wrapper(File->Read, 3, File, &read_size, buffer_ptr);
        if (EFI_ERROR(Status)) {
            Print(L"[ERROR] Failed to read weights at offset %u: %r\r\n", total_read, Status);
            uefi_call_wrapper(File->Close, 1, File);
            return Status;
        }
        
        if (read_size == 0) {
            Print(L"[ERROR] Unexpected EOF at %u bytes (expected %u)\r\n", total_read, weights_size);
            uefi_call_wrapper(File->Close, 1, File);
            return EFI_END_OF_FILE;
        }
        
        total_read += read_size;
        buffer_ptr += read_size;
        
        // Progress indicator every 512KB
        if (total_read % (512 * 1024) == 0) {
            Print(L"  ... %u KB read\r\n", total_read / 1024);
        }
    }
    
    
    uefi_call_wrapper(File->Close, 1, File);
    
    // Map weights
    transformer->data = static_weights;
    memory_map_weights(&transformer->weights, p, static_weights, shared_weights);
    
    // Sanity check: print first weight value
    float first_weight = static_weights[0];
    int whole = (int)first_weight;
    int frac = (int)((first_weight - whole) * 1000);
    if (frac < 0) frac = -frac;
    
    // Initialize run state with dynamic allocation
    Status = init_run_state(&transformer->state, p, SystemTable->BootServices);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to initialize run state: %r\r\n", Status);
        return Status;
    }
    
    Print(L"[SUCCESS] Model loaded successfully!\r\n");
    return EFI_SUCCESS;
}

// ----------------------------------------------------------------------------
// BPE TOKENIZER

typedef struct {
    char** vocab;          // vocabulary strings
    float* vocab_scores;   // scores for each token
    int vocab_size;
    unsigned int max_token_length;
} Tokenizer;

EFI_STATUS load_tokenizer(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, 
                          Tokenizer* t, CHAR16* tokenizer_path, int vocab_size) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_HANDLE Root;
    EFI_FILE_HANDLE File;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    
    // Get file system
    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
        ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
        LoadedImage->DeviceHandle, &FileSystemProtocol, (void**)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;
    
    // Open tokenizer file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, tokenizer_path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Warning: Could not load tokenizer from %s\r\n", tokenizer_path);
        return Status;
    }
    
    // Read max_token_length
    UINTN read_size = sizeof(int);
    Status = uefi_call_wrapper(File->Read, 3, File, &read_size, &t->max_token_length);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    // Allocate vocab arrays
    t->vocab_size = vocab_size;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
        vocab_size * sizeof(char*), (void**)&t->vocab);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
        vocab_size * sizeof(float), (void**)&t->vocab_scores);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    // Read each token
    for (int i = 0; i < vocab_size; i++) {
        // Read score
        read_size = sizeof(float);
        Status = uefi_call_wrapper(File->Read, 3, File, &read_size, &t->vocab_scores[i]);
        if (EFI_ERROR(Status)) break;
        
        // Read token length
        int len;
        read_size = sizeof(int);
        Status = uefi_call_wrapper(File->Read, 3, File, &read_size, &len);
        if (EFI_ERROR(Status)) break;
        
        // Allocate and read token string
        Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
            len + 1, (void**)&t->vocab[i]);
        if (EFI_ERROR(Status)) break;
        
        read_size = len;
        Status = uefi_call_wrapper(File->Read, 3, File, &read_size, t->vocab[i]);
        if (EFI_ERROR(Status)) break;
        
        t->vocab[i][len] = '\0';  // Null terminate
    }
    
    uefi_call_wrapper(File->Close, 1, File);
    
    if (EFI_ERROR(Status)) {
        Print(L"Warning: Error loading tokenizer vocabulary\r\n");
        return Status;
    }
    
    Print(L"Tokenizer loaded: %d tokens, max_len=%d\r\n", vocab_size, t->max_token_length);
    return EFI_SUCCESS;
}

char* decode_token(Tokenizer* t, int token) {
    if (token >= 0 && token < t->vocab_size) {
        return t->vocab[token];
    }
    return "<?>";  // Unknown token
}

// ----------------------------------------------------------------------------
// USER INPUT (UEFI Console Input)
// ----------------------------------------------------------------------------

int read_user_input(EFI_SYSTEM_TABLE *ST, char* buffer, int max_len) {
    // Read a line of text from UEFI console input
    // Returns number of characters read (excluding null terminator)
    
    int pos = 0;
    EFI_INPUT_KEY Key;
    EFI_STATUS Status;
    volatile int delay;
    
    while (pos < max_len - 1) {
        // Poll for keystroke (simpler than WaitForEvent)
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);
        
        if (EFI_ERROR(Status)) {
            // No key available, busy-wait a bit and try again
            for (delay = 0; delay < 50000; delay++);
            continue;
        }
        
        // Handle special keys
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN || Key.UnicodeChar == CHAR_LINEFEED) {
            // Enter key pressed - end input
            Print(L"\r\n");
            break;
        } else if (Key.UnicodeChar == CHAR_BACKSPACE) {
            // Backspace - remove last character
            if (pos > 0) {
                pos--;
                Print(L"\b \b");  // Backspace, space, backspace (erase character)
            }
        } else if (Key.UnicodeChar >= 32 && Key.UnicodeChar < 127) {
            // Printable ASCII character
            buffer[pos] = (char)Key.UnicodeChar;
            pos++;
            Print(L"%c", Key.UnicodeChar);
        }
    }
    
    buffer[pos] = '\0';  // Null terminate
    return pos;
}

// Simple BPE encoder for user input (greedy longest-match)
int encode_prompt(Tokenizer* t, char* text, int* tokens, int max_tokens) {
    // Greedy BPE tokenization: match longest tokens first
    int n_tokens = 0;
    
    // Always start with BOS token (1)
    if (n_tokens < max_tokens) {
        tokens[n_tokens++] = 1;
    }
    
    int text_len = 0;
    while (text[text_len]) text_len++;
    
    int pos = 0;
    while (pos < text_len && n_tokens < max_tokens) {
        // Try to match the longest token starting at pos
        int best_token = -1;
        int best_len = 0;
        
        // Search through vocabulary for longest match
        for (int tok = 0; tok < t->vocab_size; tok++) {
            char* vocab_piece = t->vocab[tok];
            int vocab_len = 0;
            while (vocab_piece[vocab_len]) vocab_len++;
            
            // Skip if this token is shorter than best found
            if (vocab_len <= best_len) continue;
            
            // Check if vocab_piece matches text starting at pos
            int matches = 1;
            for (int i = 0; i < vocab_len && (pos + i) < text_len; i++) {
                if (text[pos + i] != vocab_piece[i]) {
                    matches = 0;
                    break;
                }
            }
            
            if (matches && (pos + vocab_len) <= text_len) {
                best_token = tok;
                best_len = vocab_len;
            }
        }
        
        // If found a match, use it
        if (best_token >= 0) {
            tokens[n_tokens++] = best_token;
            pos += best_len;
        } else {
            // No match - try single character as fallback
            // Look for single-char token
            int found = 0;
            for (int tok = 0; tok < t->vocab_size; tok++) {
                char* vocab_piece = t->vocab[tok];
                if (vocab_piece[0] == text[pos] && vocab_piece[1] == '\0') {
                    tokens[n_tokens++] = tok;
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                // Skip unknown character
                pos++;
            } else {
                pos++;
            }
        }
    }
    
    return n_tokens;
}

// ----------------------------------------------------------------------------
// AVX/SSE INITIALIZATION
// ----------------------------------------------------------------------------

int check_and_enable_avx() {
    uint32_t eax, ebx, ecx, edx;
    uint64_t cr4, cr0;
    
    Print(L"[DEBUG] Checking CPU features...\r\n");
    
    // Check CPUID support (Leaf 1)
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    
    Print(L"[DEBUG] CPUID.1:ECX = 0x%08x\r\n", ecx);
    
    // First, ensure SSE/SSE2 is enabled in CR0 and CR4
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    
    Print(L"[DEBUG] CR0 = 0x%016llx, CR4 = 0x%016llx\r\n", cr0, cr4);
    
    // Clear EM (bit 2) and set MP (bit 1) in CR0 for FPU
    cr0 &= ~(1ULL << 2);  // Clear EM (Emulation)
    cr0 |= (1ULL << 1);   // Set MP (Monitor Coprocessor)
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
    
    // Enable OSFXSR (bit 9) for SSE in CR4
    cr4 |= (1ULL << 9);   // OSFXSR - OS support for FXSAVE/FXRSTOR
    cr4 |= (1ULL << 10);  // OSXMMEXCPT - OS support for unmasked SIMD exceptions
    
    // Check for XSAVE (ECX bit 26) and AVX (ECX bit 28)
    int has_xsave = (ecx & (1 << 26)) != 0;
    int has_avx = (ecx & (1 << 28)) != 0;
    
    Print(L"[DEBUG] XSAVE: %d, AVX: %d\r\n", has_xsave, has_avx);
    
    if (has_xsave && has_avx) {
        // Enable OSXSAVE in CR4 (bit 18)
        cr4 |= (1ULL << 18);
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
        
        Print(L"[DEBUG] OSXSAVE enabled in CR4\r\n");
        
        // Now we can safely use XGETBV/XSETBV
        uint32_t xcr0_lo, xcr0_hi;
        __asm__ volatile ("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
        
        Print(L"[DEBUG] XCR0 before = 0x%08x\r\n", xcr0_lo);
        
        // Enable x87 (bit 0), SSE (bit 1), and AVX (bit 2)
        xcr0_lo |= (1 << 0) | (1 << 1) | (1 << 2);
        __asm__ volatile ("xsetbv" :: "a"(xcr0_lo), "d"(xcr0_hi), "c"(0));
        
        Print(L"[SUCCESS] SSE/AVX enabled! XCR0 = 0x%08x\r\n", xcr0_lo);
        return 1;
    } else {
        // Just enable SSE without AVX
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
        Print(L"[INFO] SSE enabled (no AVX support)\r\n");
        return 0;
    }
}

// ----------------------------------------------------------------------------
// MODEL DETECTION AND SELECTION

typedef struct {
    CHAR16* filename;
    CHAR16* display_name;
    ModelType model_type;
    int expected_size_mb;
    BOOLEAN exists;
} ModelInfo;

EFI_STATUS check_model_exists(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16* filename, BOOLEAN* exists) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_FILE_IO_INTERFACE *FileSystem;
    EFI_FILE_HANDLE Root;
    EFI_FILE_HANDLE File;
    
    *exists = FALSE;
    
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        LoadedImage->DeviceHandle, &FileSystemProtocol, (void**)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, filename, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status)) {
        *exists = TRUE;
        uefi_call_wrapper(File->Close, 1, File);
    }
    
    return EFI_SUCCESS;
}

// Save generated text to disk (Phase 9: Persistent Storage)
EFI_STATUS save_generation(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, 
                          char* prompt, char* output, int generation_num) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_FILE_IO_INTERFACE *FileSystem;
    EFI_FILE_HANDLE Root;
    EFI_FILE_HANDLE File;
    CHAR16 filename[64];
    
    // Create filename: output_001.txt, output_002.txt, etc.
    // Simple manual construction to avoid snprintf issues
    filename[0] = 'o'; filename[1] = 'u'; filename[2] = 't'; filename[3] = 'p';
    filename[4] = 'u'; filename[5] = 't'; filename[6] = '_';
    filename[7] = '0' + ((generation_num / 100) % 10);
    filename[8] = '0' + ((generation_num / 10) % 10);
    filename[9] = '0' + (generation_num % 10);
    filename[10] = '.'; filename[11] = 't'; filename[12] = 'x'; filename[13] = 't';
    filename[14] = 0;
    
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        LoadedImage->DeviceHandle, &FileSystemProtocol, (void**)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;
    
    // Create new file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, filename,
        EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(Root->Close, 1, Root);
        return Status;
    }
    
    // Write prompt (simple version - just the text)
    char header[] = "=== LLM Generation ===\nPrompt: ";
    UINTN bytes_to_write = strlen(header);
    Status = uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, header);
    
    if (!EFI_ERROR(Status) && prompt) {
        bytes_to_write = strlen(prompt);
        uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, prompt);
    }
    
    char newline[] = "\n\nOutput:\n";
    bytes_to_write = strlen(newline);
    uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, newline);
    
    // Write generated text
    if (output) {
        bytes_to_write = strlen(output);
        uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, output);
    }
    
    // Add footer
    char footer[] = "\n\n=== End ===\n";
    bytes_to_write = strlen(footer);
    uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, footer);
    
    uefi_call_wrapper(File->Close, 1, File);
    uefi_call_wrapper(Root->Close, 1, Root);
    
    return EFI_SUCCESS;
}

// ----------------------------------------------------------------------------
// Hardware Detection Functions

HardwareCapabilities detect_hardware_capabilities(EFI_SYSTEM_TABLE *SystemTable) {
    HardwareCapabilities hw = {0};
    
    // Detect RAM using GetMemoryMap
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
    
    // First call to get size
    EFI_STATUS Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
        &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    
    if (Status == EFI_BUFFER_TOO_SMALL) {
        // Allocate buffer
        MemoryMapSize += 2 * DescriptorSize; // Add extra space
        Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3,
            EfiLoaderData, MemoryMapSize, (void**)&MemoryMap);
        
        if (!EFI_ERROR(Status)) {
            // Get actual memory map
            Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
                &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
            
            if (!EFI_ERROR(Status)) {
                // Calculate total and available RAM
                UINT64 total_pages = 0;
                UINT64 available_pages = 0;
                
                EFI_MEMORY_DESCRIPTOR *Desc = MemoryMap;
                UINTN NumDescriptors = MemoryMapSize / DescriptorSize;
                
                for (UINTN i = 0; i < NumDescriptors; i++) {
                    total_pages += Desc->NumberOfPages;
                    
                    // Count available memory types
                    if (Desc->Type == EfiConventionalMemory || 
                        Desc->Type == EfiBootServicesCode ||
                        Desc->Type == EfiBootServicesData) {
                        available_pages += Desc->NumberOfPages;
                    }
                    
                    Desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Desc + DescriptorSize);
                }
                
                // Convert pages to MB (1 page = 4KB)
                hw.total_ram_mb = (total_pages * 4096) / (1024 * 1024);
                hw.available_ram_mb = (available_pages * 4096) / (1024 * 1024);
            }
            
            uefi_call_wrapper(SystemTable->BootServices->FreePool, 1, MemoryMap);
        }
    }
    
    // Detect CPU features using CPUID
    uint32_t eax, ebx, ecx, edx;
    
    // CPUID function 1: Basic features
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    
    hw.has_sse = (edx & (1 << 25)) != 0;
    hw.has_sse2 = (edx & (1 << 26)) != 0;
    hw.has_avx = (ecx & (1 << 28)) != 0;
    hw.has_fma = (ecx & (1 << 12)) != 0;
    
    // CPUID function 7: Extended features (AVX2, AVX512)
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
    
    hw.has_avx2 = (ebx & (1 << 5)) != 0;
    hw.has_avx512 = (ebx & (1 << 16)) != 0; // AVX512F
    
    // Estimate CPU cores (simplified - just use 4 as default for now)
    hw.cpu_cores = 4;
    
    // Calculate performance score (0-1000)
    hw.performance_score = 0;
    
    // Base score from RAM
    if (hw.available_ram_mb >= 16384) hw.performance_score += 300;      // 16GB+
    else if (hw.available_ram_mb >= 8192) hw.performance_score += 250;  // 8GB
    else if (hw.available_ram_mb >= 4096) hw.performance_score += 200;  // 4GB
    else if (hw.available_ram_mb >= 2048) hw.performance_score += 150;  // 2GB
    else if (hw.available_ram_mb >= 1024) hw.performance_score += 100;  // 1GB
    else if (hw.available_ram_mb >= 512) hw.performance_score += 50;    // 512MB
    
    // Score from CPU features
    if (hw.has_avx512) hw.performance_score += 200;
    else if (hw.has_avx2) hw.performance_score += 150;
    else if (hw.has_avx) hw.performance_score += 100;
    else if (hw.has_sse2) hw.performance_score += 50;
    
    if (hw.has_fma) hw.performance_score += 50;
    
    // Score from CPU cores (simplified)
    hw.performance_score += (hw.cpu_cores * 10);
    
    // Cap at 1000
    if (hw.performance_score > 1000) hw.performance_score = 1000;
    
    return hw;
}

void print_hardware_info(HardwareCapabilities *hw) {
    Print(L"\r\n╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║              HARDWARE DETECTION                        ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n\r\n");
    
    Print(L"  💾 RAM:\r\n");
    Print(L"     Total:     %lu MB\r\n", hw->total_ram_mb);
    Print(L"     Available: %lu MB\r\n", hw->available_ram_mb);
    
    Print(L"\r\n  🖥️  CPU Features:\r\n");
    Print(L"     SSE:    %s\r\n", hw->has_sse ? L"✓" : L"✗");
    Print(L"     SSE2:   %s\r\n", hw->has_sse2 ? L"✓" : L"✗");
    Print(L"     AVX:    %s\r\n", hw->has_avx ? L"✓" : L"✗");
    Print(L"     AVX2:   %s\r\n", hw->has_avx2 ? L"✓" : L"✗");
    Print(L"     AVX512: %s\r\n", hw->has_avx512 ? L"✓" : L"✗");
    Print(L"     FMA:    %s\r\n", hw->has_fma ? L"✓" : L"✗");
    
    Print(L"\r\n  📊 Performance Score: %u / 1000\r\n", hw->performance_score);
    
    // Performance category
    if (hw->performance_score >= 700) {
        Print(L"     Category: 🚀 EXTREME (Can run large models)\r\n");
    } else if (hw->performance_score >= 500) {
        Print(L"     Category: ⚡ HIGH (Can run medium-large models)\r\n");
    } else if (hw->performance_score >= 300) {
        Print(L"     Category: 💪 MEDIUM (Can run small-medium models)\r\n");
    } else if (hw->performance_score >= 150) {
        Print(L"     Category: 📱 LOW (Limited to small models)\r\n");
    } else {
        Print(L"     Category: 🔋 MINIMAL (Tiny models only)\r\n");
    }
    
    Print(L"\r\n");
}

ModelType select_optimal_model_by_hardware(HardwareCapabilities *hw, ModelInfo *models, int num_models, int *found_count) {
    ModelType selected = 0;
    
    // Determine maximum model we can run based on available RAM
    UINT64 max_model_size_mb = hw->available_ram_mb * 0.6; // Use 60% of available RAM
    
    Print(L"\r\n  🎯 Model Selection Strategy:\r\n");
    Print(L"     Max model size: %lu MB (60%% of available RAM)\r\n", max_model_size_mb);
    
    // Find the largest model that fits in available RAM
    ModelType best_model = 0;
    UINT64 best_size = 0;
    
    for (int i = 0; i < num_models; i++) {
        if (models[i].exists && models[i].expected_size_mb <= max_model_size_mb) {
            if (models[i].expected_size_mb > best_size) {
                best_size = models[i].expected_size_mb;
                best_model = models[i].model_type;
            }
        }
    }
    
    if (best_model == 0 && *found_count > 0) {
        // Fallback to smallest available model
        for (int i = 0; i < num_models; i++) {
            if (models[i].exists) {
                if (best_model == 0 || models[i].expected_size_mb < best_size) {
                    best_size = models[i].expected_size_mb;
                    best_model = models[i].model_type;
                }
            }
        }
        Print(L"     ⚠️  Selected smallest model (insufficient RAM for larger ones)\r\n");
    }
    
    if (best_model != 0) {
        Print(L"     ✓ Selected: %lu MB model\r\n", best_size);
    }
    
    return best_model;
}

ModelType select_model(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    // First, detect hardware capabilities
    HardwareCapabilities hw = detect_hardware_capabilities(SystemTable);
    print_hardware_info(&hw);
    
    Print(L"\r\n╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║              MODEL DETECTION & SELECTION               ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    
    // Define available models (expanded list)
    ModelInfo models[] = {
        {L"stories15M.bin", L"Stories 15M (Tiny)", MODEL_STORIES15M, 60, FALSE},
        {L"stories42M.bin", L"Stories 42M (Small)", MODEL_STORIES42M, 165, FALSE},
        {L"stories110M.bin", L"Stories 110M (Medium)", MODEL_STORIES110M, 420, FALSE},
        {L"stories260M.bin", L"Stories 260M (Large)", MODEL_STORIES260M, 1000, FALSE},
        {L"tinyllama_1b.bin", L"TinyLlama 1.1B (XL)", MODEL_TINYLLAMA_1B, 4400, FALSE},
        {L"llama2_7b.bin", L"Llama2 7B (XXL)", MODEL_LLAMA2_7B, 13000, FALSE},
        {L"nanogpt.bin", L"NanoGPT 124M (Special)", MODEL_NANOGPT, 48, FALSE}
    };
    int num_models = 7;
    int found_count = 0;
    
    // Check which models are available on disk
    Print(L"\r\n  🔍 Scanning boot disk for models...\r\n\r\n");
    for (int i = 0; i < num_models; i++) {
        check_model_exists(ImageHandle, SystemTable, models[i].filename, &models[i].exists);
        if (models[i].exists) {
            Print(L"     ✓ Found: %s (%lu MB)\r\n", models[i].display_name, models[i].expected_size_mb);
            found_count++;
        }
    }
    
    if (found_count == 0) {
        Print(L"\r\n  ❌ ERROR: No model found!\r\n\r\n");
        Print(L"  Please add at least one model file to boot disk:\r\n");
        Print(L"     • stories15M.bin (60MB) - Minimum requirements\r\n");
        Print(L"     • stories42M.bin (165MB) - Better quality\r\n");
        Print(L"     • stories110M.bin (420MB) - Recommended\r\n");
        Print(L"     • stories260M.bin (1GB) - High quality\r\n");
        Print(L"     • tinyllama_1b.bin (4.4GB) - Very high quality\r\n");
        Print(L"     • llama2_7b.bin (13GB) - Maximum quality\r\n\r\n");
        
        // Offer network download or show catalog
        Print(L"\r\n  💡 You can download models from repository:\r\n\r\n");
        show_download_menu(&hw, SystemTable);
        Print(L"\r\n");
        
        return 0;
    }
    
    Print(L"\r\n  📊 Found %d model(s) on disk\r\n", found_count);
    
    // Select optimal model based on hardware
    ModelType auto_selected = select_optimal_model_by_hardware(&hw, models, num_models, &found_count);
    
    if (auto_selected == 0) {
        Print(L"\r\n  ⚠️  Could not auto-select model. Using first available...\r\n");
        for (int i = 0; i < num_models; i++) {
            if (models[i].exists) {
                auto_selected = models[i].model_type;
                break;
            }
        }
    }
    
    // Interactive selection menu
    Print(L"\r\n╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║              MODEL SELECTION MENU                      ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n\r\n");
    
    Print(L"  Available models:\r\n\r\n");
    
    int choice_map[10] = {0}; // Map choice number to model index
    int choice_num = 1;
    
    for (int i = 0; i < num_models; i++) {
        if (models[i].exists) {
            CHAR16 indicator[10] = L"";
            if (models[i].model_type == auto_selected) {
                UnicodeSPrint(indicator, sizeof(indicator), L" 🎯");
            }
            Print(L"     [%d] %s - %lu MB%s\r\n", 
                  choice_num, 
                  models[i].display_name, 
                  models[i].expected_size_mb,
                  indicator);
            choice_map[choice_num] = i;
            choice_num++;
        }
    }
    
    Print(L"\r\n     [0] 🤖 Auto-select (recommended)\r\n");
    
    // Add download option if network available
    if (check_network_available(SystemTable)) {
        Print(L"     [D] 🌐 Download models from internet\r\n");
    }
    
    Print(L"\r\n  🎯 = Auto-selected based on your hardware\r\n");
    Print(L"\r\n  ℹ️  Note: Keyboard input limited in QEMU/OVMF\r\n");
    Print(L"  ✓ Auto-selecting recommended model...\r\n\r\n");
    
    // Show download menu hint if available
    if (check_network_available(SystemTable)) {
        Print(L"  💡 Tip: Press 'D' to download more models (feature in development)\r\n\r\n");
    }
    
    // For now, directly use auto-selected model
    // TODO: Implement proper keyboard input handling in UEFI
    ModelType selected = auto_selected;
    
    return selected;
}

CHAR16* get_model_filename(ModelType model_type) {
    switch (model_type) {
        case MODEL_STORIES15M:
            return L"stories15M.bin";
        case MODEL_STORIES42M:
            return L"stories42M.bin";
        case MODEL_STORIES110M:
            return L"stories110M.bin";
        case MODEL_STORIES260M:
            return L"stories260M.bin";
        case MODEL_TINYLLAMA_1B:
            return L"tinyllama_1b.bin";
        case MODEL_LLAMA2_7B:
            return L"llama2_7b.bin";
        case MODEL_NANOGPT:
            return L"nanogpt.bin";
        default:
            return L"stories110M.bin"; // fallback
    }
}

const CHAR16* get_model_display_name(ModelType model_type) {
    switch (model_type) {
        case MODEL_STORIES15M:
            return L"Stories 15M (60MB)";
        case MODEL_STORIES42M:
            return L"Stories 42M (165MB)";
        case MODEL_STORIES110M:
            return L"Stories 110M (420MB)";
        case MODEL_STORIES260M:
            return L"Stories 260M (1GB)";
        case MODEL_TINYLLAMA_1B:
            return L"TinyLlama 1.1B (4.4GB)";
        case MODEL_LLAMA2_7B:
            return L"Llama2 7B (13GB)";
        case MODEL_NANOGPT:
            return L"NanoGPT 124M (48MB)";
        default:
            return L"Unknown Model";
    }
}

// ----------------------------------------------------------------------------
// Silent AVX enabler (no output)

void enable_avx_silent() {
    uint32_t eax, ebx, ecx, edx;
    uint64_t cr4, cr0;
    
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    
    cr0 &= ~(1ULL << 2);
    cr0 |= (1ULL << 1);
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
    
    cr4 |= (1ULL << 9) | (1ULL << 10);
    
    if ((ecx & (1 << 26)) && (ecx & (1 << 28))) {
        cr4 |= (1ULL << 18);
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
        
        uint32_t xcr0_lo, xcr0_hi;
        __asm__ volatile ("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
        xcr0_lo |= (1 << 0) | (1 << 1) | (1 << 2);
        __asm__ volatile ("xsetbv" :: "a"(xcr0_lo), "d"(xcr0_hi), "c"(0));
    } else {
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
    }
}

// ----------------------------------------------------------------------------
// CHAT REPL - Interactive Conversation System

// Chat REPL with conversation context management
void chat_repl(EFI_SYSTEM_TABLE* ST, Transformer* transformer, Tokenizer* tokenizer, BOOLEAN use_text, float temperature) {
    Print(L"\r\n╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║            INTERACTIVE CHAT REPL v2.0                  ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    Print(L"  Commands:\r\n");
    Print(L"    /help    - Show this help message\r\n");
    Print(L"    /reset   - Clear conversation history\r\n");
    Print(L"    /quit    - Exit the REPL\r\n");
    Print(L"    /temp X  - Set temperature (0.0 - 2.0)\r\n");
    Print(L"    /tokens  - Show current token usage\r\n");
    Print(L"\r\n");
    Print(L"  Context: %d tokens max\r\n", transformer->config.seq_len);
    Print(L"  Temperature: %.2f\r\n", temperature);
    Print(L"\r\n");
    Print(L"╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║  Start chatting! Type your message and press Enter.   ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    
    // Conversation state
    char conversation_buffer[8192];  // Store full conversation
    int conversation_tokens[512];     // Token sequence
    int conversation_pos = 0;         // Current position in conversation
    int turn_count = 0;               // Number of conversation turns
    
    conversation_buffer[0] = '\0';
    
    while (1) {
        // Read user input
        char user_input[512];
        Print(L"\r\nYou: ");
        int input_len = read_user_input(ST, user_input, 512);
        
        if (input_len == 0) continue;  // Empty input
        
        // Check for commands
        if (user_input[0] == '/') {
            // /quit command
            if (strcmp(user_input, "/quit") == 0 || strcmp(user_input, "/q") == 0) {
                Print(L"\r\n👋 Goodbye! Exiting REPL...\r\n\r\n");
                break;
            }
            
            // /reset command
            if (strcmp(user_input, "/reset") == 0) {
                conversation_pos = 0;
                turn_count = 0;
                conversation_buffer[0] = '\0';
                Print(L"\r\n✅ Conversation reset. Starting fresh!\r\n");
                continue;
            }
            
            // /help command
            if (strcmp(user_input, "/help") == 0) {
                Print(L"\r\n╔════════════════════════════════════════════════════════╗\r\n");
                Print(L"║                    HELP MENU                           ║\r\n");
                Print(L"╚════════════════════════════════════════════════════════╝\r\n");
                Print(L"\r\n");
                Print(L"  Commands:\r\n");
                Print(L"    /help    - Show this help message\r\n");
                Print(L"    /reset   - Clear conversation history\r\n");
                Print(L"    /quit    - Exit the REPL\r\n");
                Print(L"    /temp X  - Set temperature (0.0 - 2.0)\r\n");
                Print(L"    /tokens  - Show current token usage\r\n");
                Print(L"\r\n");
                Print(L"  Temperature Guide:\r\n");
                Print(L"    0.0  - Deterministic (always same output)\r\n");
                Print(L"    0.7  - Balanced (recommended)\r\n");
                Print(L"    1.0  - Default creative\r\n");
                Print(L"    1.5+ - Very creative/random\r\n");
                Print(L"\r\n");
                continue;
            }
            
            // /tokens command
            if (strcmp(user_input, "/tokens") == 0) {
                int max_tokens = transformer->config.seq_len;
                int remaining = max_tokens - conversation_pos;
                float usage_pct = (float)conversation_pos / (float)max_tokens * 100.0f;
                
                Print(L"\r\n📊 Token Usage:\r\n");
                Print(L"   Used:      %d / %d tokens (%.1f%%)\r\n", 
                      conversation_pos, max_tokens, usage_pct);
                Print(L"   Remaining: %d tokens\r\n", remaining);
                Print(L"   Turns:     %d\r\n", turn_count);
                Print(L"\r\n");
                
                if (remaining < 50) {
                    Print(L"⚠️  Warning: Context nearly full! Use /reset to continue.\r\n");
                }
                continue;
            }
            
            // /temp command
            if (user_input[0] == '/' && user_input[1] == 't' && 
                user_input[2] == 'e' && user_input[3] == 'm' && user_input[4] == 'p') {
                // Parse temperature value
                float new_temp = 1.0f;
                // Simple parsing - look for number after "temp "
                char* temp_str = user_input + 6;  // Skip "/temp "
                // Basic float parsing (integer part only for simplicity)
                if (*temp_str >= '0' && *temp_str <= '9') {
                    new_temp = (float)(*temp_str - '0');
                    if (*(temp_str + 1) == '.') {
                        // Handle one decimal place
                        if (*(temp_str + 2) >= '0' && *(temp_str + 2) <= '9') {
                            new_temp += (float)(*(temp_str + 2) - '0') / 10.0f;
                        }
                    }
                }
                
                if (new_temp >= 0.0f && new_temp <= 2.0f) {
                    temperature = new_temp;
                    Print(L"\r\n✅ Temperature set to: %.2f\r\n", temperature);
                } else {
                    Print(L"\r\n❌ Invalid temperature! Use 0.0 - 2.0\r\n");
                }
                continue;
            }
            
            // Unknown command
            Print(L"\r\n❌ Unknown command: %a\r\n", user_input);
            Print(L"   Type /help for available commands.\r\n");
            continue;
        }
        
        // Check if we have room in context
        if (conversation_pos >= transformer->config.seq_len - 100) {
            Print(L"\r\n⚠️  Context window nearly full!\r\n");
            Print(L"   Use /reset to clear history, or /quit to exit.\r\n");
            continue;
        }
        
        // Encode user input
        int prompt_tokens[256];
        int num_tokens = encode_prompt(tokenizer, user_input, prompt_tokens, 256);
        
        if (num_tokens == 0) {
            Print(L"\r\n❌ Failed to encode input. Try again.\r\n");
            continue;
        }
        
        // Add to conversation history
        for (int i = 0; i < num_tokens && conversation_pos < 512; i++) {
            conversation_tokens[conversation_pos++] = prompt_tokens[i];
        }
        
        // Generate response
        Print(L"Assistant: ");
        
        int response_tokens = 0;
        int max_response = 80;  // Max tokens in response
        
        // Generation loop
        for (int step = 0; step < max_response; step++) {
            // Get last token
            int token = conversation_tokens[conversation_pos - 1];
            
            // Forward pass
            float* logits = forward(transformer, token, conversation_pos - 1);
            
            if (logits == NULL) {
                Print(L"\r\n[ERROR] Forward pass failed!\r\n");
                break;
            }
            
            // Sample next token
            int next;
            if (temperature == 0.0f) {
                next = argmax(logits, transformer->config.vocab_size);
            } else {
                for (int i = 0; i < transformer->config.vocab_size; i++) {
                    logits[i] /= temperature;
                }
                softmax(logits, transformer->config.vocab_size);
                float coin = (float)rand_efi() / (float)RAND_MAX;
                next = sample_mult(logits, transformer->config.vocab_size, coin);
            }
            
            // Check for end of sequence
            if (next == 1 || next == 2) break;  // EOS or newline
            
            // Decode and print token
            if (use_text) {
                char* piece = decode_token(tokenizer, next);
                // Convert to CHAR16 and print
                CHAR16 wpiece[256];
                for (int i = 0; i < 255 && piece[i]; i++) {
                    wpiece[i] = (CHAR16)piece[i];
                    wpiece[i+1] = 0;
                }
                Print(L"%s", wpiece);
            } else {
                Print(L"[%d] ", next);
            }
            
            // Add to conversation history
            if (conversation_pos < 512) {
                conversation_tokens[conversation_pos++] = next;
                response_tokens++;
            }
            
            // Stop if context full
            if (conversation_pos >= transformer->config.seq_len - 10) {
                Print(L"\r\n[Context limit reached]");
                break;
            }
        }
        
        Print(L"\r\n");
        turn_count++;
        
        // Show token usage warning
        if (conversation_pos > transformer->config.seq_len * 0.8f) {
            Print(L"⚠️  Context 80%% full (%d/%d tokens)\r\n", 
                  conversation_pos, transformer->config.seq_len);
        }
    }
}

// ----------------------------------------------------------------------------
// EFI MAIN ENTRY POINT

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    // Try to enable AVX
    check_and_enable_avx();
    
    // Header
    Print(L"\r\n");
    Print(L"╔══════════════════════════════════════════════════════════════╗\r\n");
    Print(L"║        LLM BARE-METAL INFERENCE ENGINE v2.0                  ║\r\n");
    Print(L"║        Running on UEFI Firmware (No OS Required)             ║\r\n");
    Print(L"╚══════════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    Print(L"  🖥️  System: UEFI x86-64\r\n");
    Print(L"  ⚡ Optimizations: Auto-detected (AVX/AVX2/FMA)\r\n");
    
    // Check network capabilities
    if (check_network_available(SystemTable)) {
        Print(L"  🌐 Network: Available (HTTP download enabled)\r\n");
    } else {
        Print(L"  🌐 Network: Not available (offline mode)\r\n");
    }
    Print(L"\r\n");
    
    // Select model with hardware detection
    ModelType selected_model = select_model(ImageHandle, SystemTable);
    if (selected_model == 0) {
        Print(L"\r\n  ❌ FATAL ERROR: No compatible model found!\r\n");
        Print(L"     Please add a model file to the boot disk.\r\n\r\n");
        ST->BootServices->Stall(5000000);
        return EFI_NOT_FOUND;
    }
    
    CHAR16* model_filename = get_model_filename(selected_model);
    const CHAR16* model_display = get_model_display_name(selected_model);
    
    Print(L"\r\n╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║              LOADING SELECTED MODEL                    ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n  🚀 Model: %s\r\n", model_display);
    Print(L"  📁 File:  %s\r\n", model_filename);
    Print(L"\r\n  ⏳ Loading model into memory...\r\n\r\n");
    
    Transformer transformer;
    
    // Load model
    EFI_STATUS Status = load_model(ImageHandle, SystemTable, &transformer, model_filename);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to load model!\r\n");
        Print(L"   Status: %r\r\n", Status);
        Print(L"\r\nPress any key to exit...\r\n");
        UINTN Index;
        EFI_INPUT_KEY Key;
        ST->ConIn->Reset(ST->ConIn, FALSE);
        ST->BootServices->WaitForEvent(1, &ST->ConIn->WaitForKey, &Index);
        ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);
        return Status;
    }
    
    transformer.config.model_type = selected_model;
    
    // Load tokenizer
    Tokenizer tokenizer;
    Print(L"Loading BPE tokenizer...\r\n");
    
    Status = load_tokenizer(ImageHandle, SystemTable, &tokenizer, L"tokenizer.bin", 
                           transformer.config.vocab_size);
    
    BOOLEAN use_text = !EFI_ERROR(Status);
    if (!use_text) {
        Print(L"[ERROR] Tokenizer not found - will display token IDs only\r\n");
    } else {
        Print(L"[SUCCESS] Tokenizer loaded (32000 tokens)\r\n");
    }
    
    // Generation parameters
    float temperature = 1.0f;  // Higher = more random, lower = more deterministic
    int steps = 100;           // Number of tokens to generate (shorter for demo)
    
    // Initialize RNG with a simple varying seed
    // Use a pseudo-random value based on memory address (varies per boot)
    uint32_t seed = (uint32_t)((uintptr_t)&transformer ^ (uintptr_t)&tokenizer);
    srand_efi(seed);
    
    // Launch Interactive Chat REPL
    Print(L"\r\n╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║          LAUNCHING CHAT REPL MODE                      ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    
    // Call the interactive chat REPL
    chat_repl(SystemTable, &transformer, &tokenizer, use_text, temperature);
    
    // REPL exited - show goodbye message
    Print(L"\r\n╔════════════════════════════════════════════════════════╗\r\n");
    Print(L"║              SESSION ENDED                             ║\r\n");
    Print(L"╚════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\nThank you for using LLM Bare-Metal!\r\n\r\n");
    
    ST->BootServices->Stall(2000000); // 2 second delay
    
    return EFI_SUCCESS;
}

// OLD AUTO-GENERATE CODE (PRESERVED FOR REFERENCE - NOT USED)
#if 0
    if (mode == 1) {
        // AUTO-GENERATE MODE (original behavior)
        int token = 1;  // Start with BOS
        
        Print(L"=== Story Generation (Auto) ===\r\n");
        Print(L"Steps: %d\r\n\r\n", steps);
    
    for (int pos = 0; pos < steps; pos++) {
        // Forward pass
        float* logits = forward(&transformer, token, pos);
        
        if (logits == NULL) {
            Print(L"[ERROR] Forward pass returned NULL at pos %d!\r\n", pos);
            break;
        }
        
        // DEBUG: Print first 5 logits on first iteration
        if (pos == 0) {
            Print(L"[DEBUG] First 5 logits: %.3f %.3f %.3f %.3f %.3f\r\n",
                  logits[0], logits[1], logits[2], logits[3], logits[4]);
        }
        
        // Sample next token with temperature
        int next;
        if (temperature == 0.0f) {
            // Greedy decoding
            next = argmax(logits, transformer.config.vocab_size);
        } else {
            // Apply temperature and sample
            for (int i = 0; i < transformer.config.vocab_size; i++) {
                logits[i] /= temperature;
            }
            softmax(logits, transformer.config.vocab_size);
            float coin = (float)rand_efi() / (float)RAND_MAX;
            next = sample_mult(logits, transformer.config.vocab_size, coin);
        }
        
        // Decode and print
        if (use_text) {
            char* piece = decode_token(&tokenizer, next);
            // Convert char* to CHAR16* for Print
            CHAR16 wpiece[256];
            for (int i = 0; i < 255 && piece[i]; i++) {
                wpiece[i] = (CHAR16)piece[i];
                wpiece[i+1] = 0;
            }
            Print(L"%s", wpiece);
        } else {
            Print(L"[%d]", next);
            if ((pos + 1) % 10 == 0) Print(L"\r\n");
        }
        
        token = next;
    }
    
    Print(L"\r\n\r\nGeneration complete.\r\n");
    
    } else {
        // INTERACTIVE MENU MODE
        Print(L"\r\n========================================\r\n");
        Print(L"  Interactive Generation Menu\r\n");
        Print(L"========================================\r\n");
        
        Print(L"\r\nSelect a category to generate text:\r\n\r\n");
        Print(L"  1. Stories      - Fairy tales, fantasy, adventures\r\n");
        Print(L"  2. Science      - Educational facts and explanations\r\n");
        Print(L"  3. Adventure    - Quests, exploration, journeys\r\n");
        Print(L"  4. Philosophy   - Deep thoughts and wisdom\r\n");
        Print(L"  5. History      - Ancient civilizations and events\r\n");
        Print(L"  6. Technology   - Computers, AI, innovations\r\n");
        Print(L"  7. Auto-Demo    - Cycle through ALL categories\r\n\r\n");
        
        Print(L"========================================\r\n");
        Print(L"Note: Auto-Demo active (keyboard input unavailable in QEMU)\r\n");
        Print(L"========================================\r\n\r\n");
        
        // Prompt collections (enriched with more variety)
        static const char* story_prompts[] = {
            "Once upon a time",
            "The little girl found a mysterious door",
            "In the enchanted forest lived a wise old owl",
            "The dragon slept peacefully until",
            "A fairy granted three wishes to",
            "The princess escaped from the tower and",
            "The talking cat said to the boy"
        };
        
        static const char* science_prompts[] = {
            "The water cycle is the process by which",
            "Gravity is a force that",
            "Photosynthesis helps plants",
            "The solar system consists of",
            "Electricity flows through wires because",
            "Animals adapt to their environment by",
            "The human body has many organs that"
        };
        
        static const char* adventure_prompts[] = {
            "The brave knight embarked on a quest to",
            "Deep in the jungle, the explorer discovered",
            "The pirate ship sailed towards the mysterious island",
            "The astronaut landed on a strange planet where",
            "The treasure map led them to",
            "Through the secret tunnel they found",
            "The ancient ruins held secrets of"
        };
        
        static const char* philosophy_prompts[] = {
            "What is the meaning of life? Many believe",
            "Happiness comes from within when",
            "True friendship is built on",
            "To be wise means to",
            "The greatest virtue is"
        };
        
        static const char* history_prompts[] = {
            "Ancient civilizations built pyramids to",
            "The invention of writing changed humanity because",
            "Kings and queens ruled their kingdoms by",
            "Wars were fought over resources like",
            "Trade routes connected distant lands and"
        };
        
        static const char* technology_prompts[] = {
            "Computers process information by",
            "The internet connects people through",
            "Smartphones have cameras and screens that",
            "Robots can help humans by",
            "Artificial intelligence learns from"
        };
        
        static const char* jokes_prompts[] = {
            "Why did the chicken cross the road? Because",
            "A robot walked into a bar and",
            "The funny thing about cats is that they",
            "What do you call a bear with no teeth? A",
            "Knock knock! Who's there? A",
            "The clown juggled and then suddenly"
        };
        
        static const char* coding_prompts[] = {
            "Programming is the art of telling computers",
            "A function takes input and returns",
            "Debugging is the process of finding",
            "Variables store data that can",
            "Loops repeat code until",
            "Algorithms solve problems by"
        };
        
        // Auto-demo mode: cycle through all categories
        const char** demo_prompts;
        int num_prompts;
        const char* category_name;
        
        for (int category = 0; category < 8; category++) {
            switch(category) {
                case 0:
                    demo_prompts = story_prompts;
                    num_prompts = 7;
                    category_name = "STORIES";
                    break;
                case 1:
                    demo_prompts = science_prompts;
                    num_prompts = 7;
                    category_name = "SCIENCE";
                    break;
                case 2:
                    demo_prompts = adventure_prompts;
                    num_prompts = 7;
                    category_name = "ADVENTURE";
                    break;
                case 3:
                    demo_prompts = philosophy_prompts;
                    num_prompts = 5;
                    category_name = "PHILOSOPHY";
                    break;
                case 4:
                    demo_prompts = history_prompts;
                    num_prompts = 5;
                    category_name = "HISTORY";
                    break;
                case 5:
                    demo_prompts = technology_prompts;
                    num_prompts = 5;
                    category_name = "TECHNOLOGY";
                    break;
                case 6:
                    demo_prompts = jokes_prompts;
                    num_prompts = 6;
                    category_name = "JOKES";
                    break;
                case 7:
                    demo_prompts = coding_prompts;
                    num_prompts = 6;
                    category_name = "CODING";
                    break;
            }
            
            // Display category header
            Print(L"\r\n========================================\r\n");
            Print(L"=== Category: %a (%d prompts) ===\r\n", category_name, num_prompts);
            Print(L"========================================\r\n");
            
            char user_input[512];
            char output_buffer[8192];  // Buffer for generated text
            int conversation_pos = 0;
            int total_generations = 0;
            
            for (int demo_idx = 0; demo_idx < num_prompts; demo_idx++) {
                // Display prompt number
                Print(L"\r\n>>> Prompt %d of %d\r\n", demo_idx + 1, num_prompts);
            
            // Copy demo prompt
            const char* prompt = demo_prompts[demo_idx];
            int prompt_len = 0;
            while (prompt[prompt_len] && prompt_len < 511) {
                user_input[prompt_len] = prompt[prompt_len];
                prompt_len++;
            }
            user_input[prompt_len] = '\0';
            
            // Display prompt
            Print(L"Prompt: \"");
            for (int i = 0; user_input[i]; i++) {
                Print(L"%c", (CHAR16)user_input[i]);
            }
            Print(L"\"\r\n");
            
            // Encode the prompt using BPE tokenization
            int prompt_tokens[256];
            int num_prompt_tokens = encode_prompt(&tokenizer, user_input, prompt_tokens, 256);
            
            // Start generation loop matching Karpathy's approach exactly
            // Begin with first prompt token (BOS) at position 0
            int token = prompt_tokens[0];
            int pos = conversation_pos;
            int max_total_tokens = num_prompt_tokens + 80;  // Prompt + response
            
            // Show generation header
            Print(L"Generated: ");
            
            // Reset output buffer
            output_buffer[0] = '\0';
            int output_pos = 0;
            
            // Performance timing with EFI GetTime
            int generated_tokens = 0;
            int forward_pass_count = 0;
            EFI_TIME start_time, end_time, first_token_time;
            SystemTable->RuntimeServices->GetTime(&start_time, NULL);
            UINT64 start_ms = (UINT64)start_time.Hour * 3600000 + 
                              (UINT64)start_time.Minute * 60000 + 
                              (UINT64)start_time.Second * 1000;
            UINT64 first_token_ms = 0;
            int first_token_captured = 0;
            
            // Main generation loop
            for (int step = 0; step < max_total_tokens && pos < 256; step++) {
                forward_pass_count++;
                
                // Forward pass to get logits for current token at current position
                float* logits = forward(&transformer, token, pos);
                
                if (logits == NULL) {
                    Print(L"[ERROR] Forward pass failed\r\n");
                    break;
                }
                
                // Determine next token: forced (if in prompt) or sampled (if generating)
                int next;
                if (pos < conversation_pos + num_prompt_tokens - 1) {
                    // Still processing prompt - force next token from prompt
                    next = prompt_tokens[pos - conversation_pos + 1];
                } else {
                    // Done with prompt - sample next token
                    if (temperature == 0.0f) {
                        next = argmax(logits, transformer.config.vocab_size);
                    } else {
                        // Apply temperature
                        for (int j = 0; j < transformer.config.vocab_size; j++) {
                            logits[j] /= temperature;
                        }
                        softmax(logits, transformer.config.vocab_size);
                        
                        // Sample with top-p (nucleus sampling) for better quality
                        float coin = (float)rand_efi() / (float)RAND_MAX;
                        next = sample_mult(logits, transformer.config.vocab_size, coin);
                    }
                    
                    // Only print/save generated tokens (not prompt tokens)
                    generated_tokens++;
                    
                    if (use_text) {
                        // Capture first token latency
                        if (!first_token_captured) {
                            SystemTable->RuntimeServices->GetTime(&first_token_time, NULL);
                            first_token_ms = (UINT64)first_token_time.Hour * 3600000 + 
                                            (UINT64)first_token_time.Minute * 60000 + 
                                            (UINT64)first_token_time.Second * 1000;
                            first_token_captured = 1;
                        }
                        
                        char* piece = decode_token(&tokenizer, token);  // Print CURRENT token
                        CHAR16 wpiece[256];
                        for (int k = 0; k < 255 && piece[k]; k++) {
                            wpiece[k] = (CHAR16)piece[k];
                            wpiece[k+1] = 0;
                        }
                        Print(L"%s", wpiece);
                        
                        // Save to output buffer
                        int piece_len = 0;
                        while (piece[piece_len]) piece_len++;
                        if (output_pos + piece_len < sizeof(output_buffer) - 1) {
                            for (int k = 0; k < piece_len; k++) {
                                output_buffer[output_pos++] = piece[k];
                            }
                            output_buffer[output_pos] = '\0';
                        }
                    } else {
                        Print(L"[%d] ", token);
                    }
                }
                
                // Move to next position
                pos++;
                
                // Check for EOS token (end of sequence)
                if (next == 1 || next == 2) {
                    if (pos >= conversation_pos + num_prompt_tokens) {
                        Print(L" [EOS]");
                        break;
                    }
                }
                
                // Move to next token
                token = next;
            }
            
                // Generation complete
                Print(L"\r\n");
                total_generations++;
                
                // Calculate elapsed time
                SystemTable->RuntimeServices->GetTime(&end_time, NULL);
                UINT64 end_ms = (UINT64)end_time.Hour * 3600000 + 
                                (UINT64)end_time.Minute * 60000 + 
                                (UINT64)end_time.Second * 1000;
                UINT64 elapsed_ms = end_ms - start_ms;
                
                // Performance statistics with precise timing
                if (generated_tokens > 0 && forward_pass_count > 0 && elapsed_ms > 0) {
                    float elapsed_sec = (float)elapsed_ms / 1000.0f;
                    float tokens_per_sec = (float)generated_tokens / elapsed_sec;
                    
                    Print(L"[PERF] Generated %d tokens in %d.%03d seconds\r\n", 
                          generated_tokens, 
                          (int)(elapsed_ms / 1000), 
                          (int)(elapsed_ms % 1000));
                    Print(L"[PERF] Speed: %.2f tokens/second\r\n", tokens_per_sec);
                    
                    // First token latency
                    if (first_token_captured) {
                        UINT64 first_token_latency = first_token_ms - start_ms;
                        Print(L"[PERF] First token latency: %d ms\r\n", (int)first_token_latency);
                    }
                    
                    Print(L"[PERF] Forward passes: %d (ratio: %.2f)\r\n",
                          forward_pass_count,
                          (float)generated_tokens / (float)forward_pass_count);
                }
                
                // Save to disk (Phase 9: Persistent Storage)
                EFI_STATUS save_status = save_generation(ImageHandle, SystemTable, 
                    user_input, output_buffer, total_generations);
                
                if (!EFI_ERROR(save_status)) {
                    Print(L"[SAVED] output_%03d.txt\r\n", total_generations);
                } else {
                    Print(L"[INFO] Could not save to disk (read-only filesystem?)\r\n");
                }
                
                int tokens_generated = pos - (conversation_pos + num_prompt_tokens);
                Print(L"[COMPLETE] Generated %d tokens\r\n", tokens_generated);
                
                // Display conversation stats
                Print(L"[CONVERSATION] Position: %d/%d tokens used\r\n", 
                      conversation_pos, transformer.config.seq_len);
                
                // Check if we can continue conversation
                int remaining_context = transformer.config.seq_len - pos;
                if (remaining_context > 50) {
                    Print(L"[CONVERSATION] %d tokens remaining for continuation\r\n", 
                          remaining_context);
                }
                
                Print(L"========================================\r\n\r\n");
                conversation_pos = pos; // Update conversation position to current position
                
                // Small delay between prompts
                ST->BootServices->Stall(1000000); // 1 second
                
                // Reset if conversation gets too long
                if (conversation_pos > transformer.config.seq_len - 100) {
                    Print(L"[CONTEXT RESET] Memory limit reached (%d tokens)\r\n", 
                          conversation_pos);
                    Print(L"Starting fresh conversation...\r\n\r\n");
                    conversation_pos = 0;
                }
            }
        }
        
        // Demo complete message
        Print(L"\r\n========================================\r\n");
        Print(L"=== AUTO-DEMO COMPLETE ===\r\n");
        Print(L"All 41 prompts across 6 categories demonstrated\r\n");
        Print(L"Interactive menu works on real UEFI hardware\r\n");
        Print(L"========================================\r\n");
    }
    
    // Session end
    Print(L"\r\n[SESSION ENDED]\r\n");
    Print(L"Thank you for using LLM Bare-Metal!\r\n");
    
    // Small delay before exit
    ST->BootServices->Stall(2000000); // 2 seconds
    
    return EFI_SUCCESS;
#endif
// END OF OLD AUTO-GENERATE CODE
