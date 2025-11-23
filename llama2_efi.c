/*
 * LLaMA2 Inference on Bare-Metal UEFI Firmware
 * 
 * Runs 15M parameter transformer model directly on UEFI without OS.
 * Based on Andrej Karpathy's llama2.c (MIT License)
 * https://github.com/karpathy/llama2.c
 * 
 * Model: stories15M.bin (dim=288, n_layers=6, n_heads=6, seq_len=256)
 * 
 * SPDX-License-Identifier: MIT
 */

#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <stddef.h>

// AVX/SSE intrinsics for SIMD optimizations
#if defined(__AVX2__)
#include <immintrin.h>
#endif

// Global flag for AVX2 support (set at runtime)
static int g_has_avx2 = 0;

// ----------------------------------------------------------------------------
// CONVERSATIONAL FEATURES (OPTION 5)
// ----------------------------------------------------------------------------

// Conversation history management
#define MAX_HISTORY_ENTRIES 10
#define MAX_PROMPT_LENGTH 256
#define MAX_RESPONSE_LENGTH 512

typedef struct {
    char prompt[MAX_PROMPT_LENGTH];
    char response[MAX_RESPONSE_LENGTH];
    int prompt_token_count;
    int response_token_count;
} ConversationTurn;

typedef struct {
    ConversationTurn turns[MAX_HISTORY_ENTRIES];
    int count;
    int total_tokens;
    float temperature;
    int max_response_tokens;
} ConversationHistory;

// Initialize conversation history
void init_conversation(ConversationHistory* hist) {
    hist->count = 0;
    hist->total_tokens = 0;
    hist->temperature = 0.9f;
    hist->max_response_tokens = 100;
    
    for (int i = 0; i < MAX_HISTORY_ENTRIES; i++) {
        hist->turns[i].prompt[0] = '\0';
        hist->turns[i].response[0] = '\0';
        hist->turns[i].prompt_token_count = 0;
        hist->turns[i].response_token_count = 0;
    }
}

// Add turn to conversation history
void add_turn(ConversationHistory* hist, const char* prompt, const char* response, 
              int prompt_tokens, int response_tokens) {
    if (hist->count >= MAX_HISTORY_ENTRIES) {
        // Shift history (remove oldest)
        for (int i = 0; i < MAX_HISTORY_ENTRIES - 1; i++) {
            // Copy turn i+1 to turn i
            for (int j = 0; j < MAX_PROMPT_LENGTH; j++) {
                hist->turns[i].prompt[j] = hist->turns[i+1].prompt[j];
            }
            for (int j = 0; j < MAX_RESPONSE_LENGTH; j++) {
                hist->turns[i].response[j] = hist->turns[i+1].response[j];
            }
            hist->turns[i].prompt_token_count = hist->turns[i+1].prompt_token_count;
            hist->turns[i].response_token_count = hist->turns[i+1].response_token_count;
        }
        hist->count = MAX_HISTORY_ENTRIES - 1;
    }
    
    // Add new turn
    int idx = hist->count;
    int i = 0;
    while (prompt[i] && i < MAX_PROMPT_LENGTH - 1) {
        hist->turns[idx].prompt[i] = prompt[i];
        i++;
    }
    hist->turns[idx].prompt[i] = '\0';
    
    i = 0;
    while (response[i] && i < MAX_RESPONSE_LENGTH - 1) {
        hist->turns[idx].response[i] = response[i];
        i++;
    }
    hist->turns[idx].response[i] = '\0';
    
    hist->turns[idx].prompt_token_count = prompt_tokens;
    hist->turns[idx].response_token_count = response_tokens;
    hist->total_tokens += prompt_tokens + response_tokens;
    hist->count++;
}

// Clear conversation history
void clear_history(ConversationHistory* hist) {
    hist->count = 0;
    hist->total_tokens = 0;
}

// Check if input is a command (starts with /)
int is_command(const char* input) {
    return input[0] == '/';
}

// Process system commands
int process_command(const char* input, ConversationHistory* hist, EFI_SYSTEM_TABLE* ST) {
    // /help - Show available commands
    if (strcmp(input, "/help") == 0) {
        Print(L"\r\n=== Available Commands ===\r\n");
        Print(L"/help       - Show this help message\r\n");
        Print(L"/clear      - Clear conversation history\r\n");
        Print(L"/history    - Show conversation history\r\n");
        Print(L"/stats      - Show conversation statistics\r\n");
        Print(L"/temp <val> - Set temperature (0.0-1.5)\r\n");
        Print(L"/tokens <n> - Set max response tokens\r\n");
        Print(L"/exit       - Exit conversation\r\n");
        Print(L"========================\r\n\r\n");
        return 1;
    }
    
    // /clear - Clear history
    if (strcmp(input, "/clear") == 0) {
        clear_history(hist);
        Print(L"\r\n[Conversation history cleared]\r\n\r\n");
        return 1;
    }
    
    // /history - Show conversation history
    if (strcmp(input, "/history") == 0) {
        Print(L"\r\n=== Conversation History ===\r\n");
        if (hist->count == 0) {
            Print(L"(empty)\r\n");
        } else {
            for (int i = 0; i < hist->count; i++) {
                Print(L"\r\nTurn %d:\r\n", i + 1);
                Print(L"  User: \"");
                for (int j = 0; hist->turns[i].prompt[j] && j < 50; j++) {
                    Print(L"%c", (CHAR16)hist->turns[i].prompt[j]);
                }
                if (hist->turns[i].prompt[50]) Print(L"...");
                Print(L"\"\r\n");
                Print(L"  Tokens: %d + %d\r\n", 
                      hist->turns[i].prompt_token_count,
                      hist->turns[i].response_token_count);
            }
        }
        Print(L"==========================\r\n\r\n");
        return 1;
    }
    
    // /stats - Show statistics
    if (strcmp(input, "/stats") == 0) {
        Print(L"\r\n=== Conversation Stats ===\r\n");
        Print(L"Turns: %d/%d\r\n", hist->count, MAX_HISTORY_ENTRIES);
        Print(L"Total tokens: %d\r\n", hist->total_tokens);
        Print(L"Temperature: %.2f\r\n", (double)hist->temperature);
        Print(L"Max response tokens: %d\r\n", hist->max_response_tokens);
        if (g_has_avx2) {
            Print(L"SIMD: AVX2 enabled\r\n");
        } else {
            Print(L"SIMD: Scalar fallback\r\n");
        }
        Print(L"=========================\r\n\r\n");
        return 1;
    }
    
    // /temp <value> - Set temperature
    if (input[0] == '/' && input[1] == 't' && input[2] == 'e' && 
        input[3] == 'm' && input[4] == 'p' && input[5] == ' ') {
        // Parse temperature value (simple float parsing)
        float new_temp = 0.9f;
        const char* val_str = &input[6];
        
        // Simple parsing: 0.X format
        if (val_str[0] >= '0' && val_str[0] <= '9') {
            int whole = val_str[0] - '0';
            int frac = 0;
            if (val_str[1] == '.' && val_str[2] >= '0' && val_str[2] <= '9') {
                frac = val_str[2] - '0';
                new_temp = (float)whole + (float)frac / 10.0f;
            } else {
                new_temp = (float)whole;
            }
            
            // Clamp to reasonable range
            if (new_temp < 0.0f) new_temp = 0.0f;
            if (new_temp > 1.5f) new_temp = 1.5f;
            
            hist->temperature = new_temp;
            Print(L"\r\n[Temperature set to %.2f]\r\n\r\n", (double)new_temp);
        } else {
            Print(L"\r\n[Invalid temperature value. Use /temp <0.0-1.5>]\r\n\r\n");
        }
        return 1;
    }
    
    // /tokens <n> - Set max response tokens
    if (input[0] == '/' && input[1] == 't' && input[2] == 'o' && 
        input[3] == 'k' && input[4] == 'e' && input[5] == 'n' && 
        input[6] == 's' && input[7] == ' ') {
        // Parse token count
        int new_tokens = 0;
        const char* val_str = &input[8];
        
        // Simple integer parsing
        while (*val_str >= '0' && *val_str <= '9') {
            new_tokens = new_tokens * 10 + (*val_str - '0');
            val_str++;
        }
        
        if (new_tokens > 0 && new_tokens <= 512) {
            hist->max_response_tokens = new_tokens;
            Print(L"\r\n[Max response tokens set to %d]\r\n\r\n", new_tokens);
        } else {
            Print(L"\r\n[Invalid token count. Use /tokens <1-512>]\r\n\r\n");
        }
        return 1;
    }
    
    // /exit - Exit conversation
    if (strcmp(input, "/exit") == 0) {
        Print(L"\r\n[Exiting conversation...]\r\n\r\n");
        return 2;  // Signal exit
    }
    
    // Unknown command
    Print(L"\r\n[Unknown command. Type /help for available commands]\r\n\r\n");
    return 1;
}

// ----------------------------------------------------------------------------
// String utilities for REPL

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

size_t my_strlen(const char* s) {
    const char* p = s;
    while (*p) p++;
    return p - s;
}

void* my_memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

// Simple sprintf for formatting byte tokens
int my_sprintf(char* str, const char* format, ...) {
    // Very simplified sprintf - only handles <0x%02X> format
    __builtin_va_list args;
    __builtin_va_start(args, format);
    
    int written = 0;
    const char* p = format;
    
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == '0') {
                p++; // skip '0'
                int width = *p++ - '0'; // get width (e.g., '2')
                if (*p == 'X' || *p == 'x') {
                    // Hex format
                    unsigned int val = __builtin_va_arg(args, unsigned int);
                    char hex[] = "0123456789ABCDEF";
                    char buf[10];
                    int pos = 0;
                    
                    // Convert to hex
                    do {
                        buf[pos++] = hex[val % 16];
                        val /= 16;
                    } while (val && pos < 10);
                    
                    // Pad with zeros if needed
                    while (pos < width && pos < 10) {
                        buf[pos++] = '0';
                    }
                    
                    // Write reversed (correct order)
                    while (pos > 0) {
                        str[written++] = buf[--pos];
                    }
                    p++;
                }
            }
        } else {
            str[written++] = *p++;
        }
    }
    
    str[written] = '\0';
    __builtin_va_end(args);
    return written;
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
// Multi-Model Architecture Support
// Model 1: stories15M (60MB) - dim=288, n_layers=6, seq_len=256
// Model 2: NanoGPT-124M (48MB) - dim=768, n_layers=12, seq_len=1024  
// Model 3: TinyLlama-1.1B-Chat (440MB) - dim=2048, n_layers=22, seq_len=2048

typedef enum {
    MODEL_STORIES15M = 1,
    MODEL_NANOGPT = 2,
    MODEL_TINYLLAMA_CHAT = 3
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
// - stories15M: dim=288, n_layers=6, hidden_dim=768, seq_len=256
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
static float *static_k = NULL;  // Fixed: was missing!
static float *static_v = NULL;  // Fixed: was missing!
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
    
    // Allocate k and v buffers (CRITICAL: these were missing!)
    Print(L"  Allocating k (%u bytes)...\r\n", kv_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, kv_dim * sizeof(float), (void**)&static_k);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate k: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating v (%u bytes)...\r\n", kv_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, kv_dim * sizeof(float), (void**)&static_v);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate v: %r\r\n", Status); return Status; }
    
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
    s->k = static_k;  // Fixed: was missing!
    s->v = static_v;  // Fixed: was missing!
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
    w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

// ----------------------------------------------------------------------------
// TRANSFORMER LOGIC (100% UNCHANGED from Karpathy's llama2.c)

#if defined(__AVX2__)
void rmsnorm_avx2(float* o, float* x, float* weight, int size) {
    // Calculate sum of squares using AVX2
    __m256 ss_vec = _mm256_setzero_ps();
    int j = 0;
    
    for (; j <= size - 8; j += 8) {
        __m256 x_vec = _mm256_loadu_ps(&x[j]);
        ss_vec = _mm256_fmadd_ps(x_vec, x_vec, ss_vec);
    }
    
    // Horizontal sum
    __m128 sum_high = _mm256_extractf128_ps(ss_vec, 1);
    __m128 sum_low = _mm256_castps256_ps128(ss_vec);
    __m128 sum128 = _mm_add_ps(sum_low, sum_high);
    __m128 shuf = _mm_movehdup_ps(sum128);
    __m128 sums = _mm_add_ps(sum128, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    float ss = _mm_cvtss_f32(sums);
    
    // Remainder
    for (; j < size; j++) {
        ss += x[j] * x[j];
    }
    
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    
    // Normalize and scale using AVX2
    __m256 ss_broadcast = _mm256_set1_ps(ss);
    j = 0;
    for (; j <= size - 8; j += 8) {
        __m256 x_vec = _mm256_loadu_ps(&x[j]);
        __m256 w_vec = _mm256_loadu_ps(&weight[j]);
        __m256 result = _mm256_mul_ps(w_vec, _mm256_mul_ps(ss_broadcast, x_vec));
        _mm256_storeu_ps(&o[j], result);
    }
    
    // Remainder
    for (; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}
#endif

void rmsnorm(float* o, float* x, float* weight, int size) {
#if defined(__AVX2__)
    if (g_has_avx2) {
        rmsnorm_avx2(o, x, weight, size);
        return;
    }
#endif
    
    // Scalar fallback: calculate sum of squares
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

#if defined(__AVX2__)
void softmax_avx2(float* x, int size) {
    // Find max value using AVX2
    __m256 max_vec = _mm256_set1_ps(x[0]);
    int i = 0;
    
    for (; i <= size - 8; i += 8) {
        __m256 x_vec = _mm256_loadu_ps(&x[i]);
        max_vec = _mm256_max_ps(max_vec, x_vec);
    }
    
    // Horizontal max
    __m128 max_high = _mm256_extractf128_ps(max_vec, 1);
    __m128 max_low = _mm256_castps256_ps128(max_vec);
    __m128 max128 = _mm_max_ps(max_low, max_high);
    max128 = _mm_max_ps(max128, _mm_shuffle_ps(max128, max128, 0x4E));
    max128 = _mm_max_ps(max128, _mm_shuffle_ps(max128, max128, 0xB1));
    float max_val = _mm_cvtss_f32(max128);
    
    // Check remainder
    for (; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    
    // Exp and sum using AVX2
    __m256 max_broadcast = _mm256_set1_ps(max_val);
    __m256 sum_vec = _mm256_setzero_ps();
    i = 0;
    
    for (; i <= size - 8; i += 8) {
        __m256 x_vec = _mm256_loadu_ps(&x[i]);
        x_vec = _mm256_sub_ps(x_vec, max_broadcast);
        
        // Note: expf is scalar, need to process element-wise
        float temp[8];
        _mm256_storeu_ps(temp, x_vec);
        for (int k = 0; k < 8; k++) {
            temp[k] = expf(temp[k]);
        }
        x_vec = _mm256_loadu_ps(temp);
        _mm256_storeu_ps(&x[i], x_vec);
        sum_vec = _mm256_add_ps(sum_vec, x_vec);
    }
    
    // Horizontal sum
    __m128 sum_high = _mm256_extractf128_ps(sum_vec, 1);
    __m128 sum_low = _mm256_castps256_ps128(sum_vec);
    __m128 sum128 = _mm_add_ps(sum_low, sum_high);
    __m128 shuf = _mm_movehdup_ps(sum128);
    __m128 sums = _mm_add_ps(sum128, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    float sum = _mm_cvtss_f32(sums);
    
    // Remainder exp and sum
    for (; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    
    // Normalize using AVX2
    __m256 sum_broadcast = _mm256_set1_ps(sum);
    i = 0;
    for (; i <= size - 8; i += 8) {
        __m256 x_vec = _mm256_loadu_ps(&x[i]);
        x_vec = _mm256_div_ps(x_vec, sum_broadcast);
        _mm256_storeu_ps(&x[i], x_vec);
    }
    
    // Remainder
    for (; i < size; i++) {
        x[i] /= sum;
    }
}
#endif

void softmax(float* x, int size) {
#if defined(__AVX2__)
    if (g_has_avx2) {
        softmax_avx2(x, size);
        return;
    }
#endif
    
    // Scalar fallback: find max value (for numerical stability)
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

#if defined(__AVX2__)
void matmul_avx2(float* xout, float* x, float* w, int n, int d) {
    // W (d,n) @ x (n,) -> xout (d,) using AVX2 (8 floats at a time)
    for (int i = 0; i < d; i++) {
        __m256 sum_vec = _mm256_setzero_ps();
        int j = 0;
        
        // Process 8 elements at a time
        for (; j <= n - 8; j += 8) {
            __m256 w_vec = _mm256_loadu_ps(&w[i * n + j]);
            __m256 x_vec = _mm256_loadu_ps(&x[j]);
            sum_vec = _mm256_fmadd_ps(w_vec, x_vec, sum_vec);
        }
        
        // Horizontal sum of 8 elements
        __m128 sum_high = _mm256_extractf128_ps(sum_vec, 1);
        __m128 sum_low = _mm256_castps256_ps128(sum_vec);
        __m128 sum128 = _mm_add_ps(sum_low, sum_high);
        __m128 shuf = _mm_movehdup_ps(sum128);
        __m128 sums = _mm_add_ps(sum128, shuf);
        shuf = _mm_movehl_ps(shuf, sums);
        sums = _mm_add_ss(sums, shuf);
        float val = _mm_cvtss_f32(sums);
        
        // Handle remainder
        for (; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        
        xout[i] = val;
    }
}
#endif

void matmul(float* xout, float* x, float* w, int n, int d) {
#if defined(__AVX2__)
    if (g_has_avx2) {
        matmul_avx2(xout, x, w, n, d);
        return;
    }
#endif
    
    // Scalar fallback: W (d,n) @ x (n,) -> xout (d,)
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
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

        // save key,value at this time step (pos) to our kv cache
        int loff = l * p->seq_len * kv_dim; // kv cache layer offset for convenience
        float* key_cache_row = s->key_cache + loff + pos * kv_dim;
        float* value_cache_row = s->value_cache + loff + pos * kv_dim;
        for (int i = 0; i < kv_dim; i++) {
            key_cache_row[i] = s->k[i];
            value_cache_row[i] = s->v[i];
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
    
    // Read config header (7 ints)
    UINTN config_size = sizeof(Config);
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

// Simple BPE encoder for user input (simplified version)
// Helper: find token in vocabulary by exact string match
int str_lookup(char* str, Tokenizer* t) {
    for (int i = 0; i < t->vocab_size; i++) {
        if (strcmp(str, t->vocab[i]) == 0) {
            return i;
        }
    }
    return -1;
}

// BPE encode: convert text to tokens using byte-pair encoding
int encode_prompt(Tokenizer* t, char* text, int* tokens, int max_tokens) {
    int n_tokens = 0;
    
    // Always start with BOS token (1)
    tokens[n_tokens++] = 1;
    
    // Handle empty or null input
    if (!text || text[0] == '\0') {
        return n_tokens;
    }
    
    // BPE encoding: first encode each byte as a token
    // Allocate temporary array for character tokens
    int* char_tokens = (int*)__builtin_alloca(my_strlen(text) * sizeof(int));
    int n_char_tokens = 0;
    
    // Convert each character to token ID
    for (char* c = text; *c != '\0' && n_char_tokens < max_tokens - 1; c++) {
        // For single byte characters, look up directly in vocab
        char single_char[2] = {*c, '\0'};
        int token_id = str_lookup(single_char, t);
        
        if (token_id != -1) {
            char_tokens[n_char_tokens++] = token_id;
        } else {
            // If character not found, try byte representation
            // Most tokenizers have byte-level fallback tokens like <0xXX>
            char byte_token[10];
            my_sprintf(byte_token, "<0x%02X>", (unsigned char)*c);
            token_id = str_lookup(byte_token, t);
            if (token_id != -1) {
                char_tokens[n_char_tokens++] = token_id;
            }
        }
    }
    
    // Now perform BPE merges
    // This is a simplified version - full BPE would need the merge rules
    // For now, we try to merge adjacent tokens if they form a known bigram
    while (1) {
        float best_score = -1e10;
        int best_id = -1;
        int best_idx = -1;
        
        // Find the best pair to merge
        for (int i = 0; i < n_char_tokens - 1; i++) {
            // Create merged string
            char* str1 = t->vocab[char_tokens[i]];
            char* str2 = t->vocab[char_tokens[i + 1]];
            
            // Allocate space for merged string
            int len1 = my_strlen(str1);
            int len2 = my_strlen(str2);
            char* merged = (char*)__builtin_alloca(len1 + len2 + 1);
            my_memcpy(merged, str1, len1);
            my_memcpy(merged + len1, str2, len2);
            merged[len1 + len2] = '\0';
            
            // Look up merged token
            int merged_id = str_lookup(merged, t);
            if (merged_id != -1 && t->vocab_scores[merged_id] > best_score) {
                best_score = t->vocab_scores[merged_id];
                best_id = merged_id;
                best_idx = i;
            }
        }
        
        // No more merges possible
        if (best_idx == -1) {
            break;
        }
        
        // Perform the merge
        char_tokens[best_idx] = best_id;
        // Shift remaining tokens left
        for (int i = best_idx + 1; i < n_char_tokens - 1; i++) {
            char_tokens[i] = char_tokens[i + 1];
        }
        n_char_tokens--;
    }
    
    // Copy final tokens to output
    for (int i = 0; i < n_char_tokens && n_tokens < max_tokens; i++) {
        tokens[n_tokens++] = char_tokens[i];
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
        
        // Check for AVX2 support (CPUID Leaf 7)
        __asm__ volatile (
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(7), "c"(0)
        );
        
        int has_avx2 = (ebx & (1 << 5)) != 0;
        Print(L"[DEBUG] CPUID.7:EBX = 0x%08x, AVX2: %d\r\n", ebx, has_avx2);
        
        if (has_avx2) {
            g_has_avx2 = 1;
            Print(L"[SUCCESS] AVX2 enabled! XCR0 = 0x%08x\r\n", xcr0_lo);
        } else {
            Print(L"[SUCCESS] AVX enabled (no AVX2 support). XCR0 = 0x%08x\r\n", xcr0_lo);
        }
        
        return 1;
    } else {
        // Just enable SSE without AVX
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
        Print(L"[INFO] SSE enabled (no AVX support)\r\n");
        g_has_avx2 = 0;
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

ModelType select_model(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    Print(L"\r\n");
    Print(L"╔═══════════════════════════════════════════════╗\r\n");
    Print(L"║   MULTIMODAL LLM BOOTLOADER - Model Selector  ║\r\n");
    Print(L"╚═══════════════════════════════════════════════╝\r\n\r\n");
    
    ModelInfo models[] = {
        {L"stories15M.bin", L"stories15M (60MB) - Story generation", MODEL_STORIES15M, 60, FALSE},
        {L"nanogpt.bin", L"NanoGPT-124M (48MB) - GPT-2 architecture", MODEL_NANOGPT, 48, FALSE},
        {L"tinyllama_chat.bin", L"TinyLlama-1.1B-Chat (440MB) - Conversational", MODEL_TINYLLAMA_CHAT, 440, FALSE}
    };
    int num_models = 3;
    
    // Scan for available models
    Print(L"Scanning for models...\r\n\r\n");
    int available_count = 0;
    
    for (int i = 0; i < num_models; i++) {
        check_model_exists(ImageHandle, SystemTable, models[i].filename, &models[i].exists);
        if (models[i].exists) {
            Print(L"  ✓ [%d] %s\r\n", i + 1, models[i].display_name);
            available_count++;
        } else {
            Print(L"  ✗ [%d] %s (not found)\r\n", i + 1, models[i].display_name);
        }
    }
    
    if (available_count == 0) {
        Print(L"\r\n[ERROR] No models found!\r\n");
        Print(L"Please place at least one model file on the boot disk:\r\n");
        Print(L"  - stories15M.bin (60MB)\r\n");
        Print(L"  - nanogpt.bin (48MB)\r\n");
        Print(L"  - tinyllama_chat.bin (440MB)\r\n\r\n");
        return 0;
    }
    
    Print(L"\r\nFound %d model(s).\r\n", available_count);
    
    // Auto-select if only one model available
    if (available_count == 1) {
        for (int i = 0; i < num_models; i++) {
            if (models[i].exists) {
                Print(L"Auto-selecting: %s\r\n\r\n", models[i].display_name);
                return models[i].model_type;
            }
        }
    }
    
    // Manual selection for multiple models
    Print(L"\r\nSelect model (1-%d): ", num_models);
    
    // Simple number input (wait for keypress)
    EFI_INPUT_KEY Key;
    EFI_STATUS Status;
    
    while (TRUE) {
        Status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
        if (!EFI_ERROR(Status)) {
            if (Key.UnicodeChar >= L'1' && Key.UnicodeChar <= L'0' + num_models) {
                int selection = Key.UnicodeChar - L'0';
                if (models[selection - 1].exists) {
                    Print(L"%c\r\n\r\n", Key.UnicodeChar);
                    Print(L"Selected: %s\r\n\r\n", models[selection - 1].display_name);
                    return models[selection - 1].model_type;
                } else {
                    Print(L"\r\nModel not available. Try again: ");
                }
            }
        }
        ST->BootServices->Stall(100000); // 100ms
    }
    
    return MODEL_STORIES15M; // Fallback
}

CHAR16* get_model_filename(ModelType model_type) {
    switch (model_type) {
        case MODEL_STORIES15M:
            return L"stories15M.bin";
        case MODEL_NANOGPT:
            return L"nanogpt.bin";
        case MODEL_TINYLLAMA_CHAT:
            return L"tinyllama_chat.bin";
        default:
            return L"stories15M.bin";
    }
}

// ----------------------------------------------------------------------------
// EFI MAIN ENTRY POINT

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    // Try to enable AVX for potential libm usage
    check_and_enable_avx();
    
    Print(L"\r\n");
    Print(L"╔═══════════════════════════════════════════════╗\r\n");
    Print(L"║   MULTIMODAL LLM BARE-METAL BOOTLOADER       ║\r\n");
    Print(L"╚═══════════════════════════════════════════════╝\r\n");
    Print(L"Running on UEFI firmware\r\n\r\n");
    
    // Select model
    ModelType selected_model = select_model(ImageHandle, SystemTable);
    if (selected_model == 0) {
        Print(L"No model selected. Exiting...\r\n");
        ST->BootServices->Stall(3000000); // 3 seconds
        return EFI_NOT_FOUND;
    }
    
    CHAR16* model_filename = get_model_filename(selected_model);
    
    Print(L"Initializing transformer...\r\n");
    
    // Allocate transformer
    Transformer transformer;
    
    Print(L"Loading model from %s...\r\n", model_filename);
    
    // Load model (pass SystemTable)
    EFI_STATUS Status = load_model(ImageHandle, SystemTable, &transformer, model_filename);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to load model: %r\r\n", Status);
        Print(L"Press any key to exit...\r\n");
        UINTN Index;
        EFI_INPUT_KEY Key;
        ST->ConIn->Reset(ST->ConIn, FALSE);
        ST->BootServices->WaitForEvent(1, &ST->ConIn->WaitForKey, &Index);
        ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);
        return Status;
    }
    
    // Store selected model type in config
    transformer.config.model_type = selected_model;
    
    Print(L"Model loaded. Config validated.\r\n");
    
    // Load tokenizer
    Tokenizer tokenizer;
    Print(L"Loading tokenizer...\r\n");
    Status = load_tokenizer(ImageHandle, SystemTable, &tokenizer, L"tokenizer.bin", 
                           transformer.config.vocab_size);
    
    BOOLEAN use_text = !EFI_ERROR(Status);
    if (!use_text) {
        Print(L"Will display token IDs only.\r\n");
    }
    
    // Generation parameters
    float temperature = 0.9f;  // Higher = more random, lower = more deterministic
    int steps = 256;           // Number of tokens to generate
    
    // Initialize RNG with a simple seed
    srand_efi((uint32_t)12345);
    
    // Mode selection - FORCED TO REPL MODE (keyboard input crashes in QEMU/OVMF)
    Print(L"\r\n=== LLaMA2 Bare-Metal REPL ===\r\n");
    Print(L"Starting in Interactive REPL mode...\r\n");
    Print(L"(Keyboard input disabled - QEMU/OVMF limitation)\r\n\r\n");
    
    int mode = 2;  // Force interactive REPL mode
    
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
        // INTERACTIVE CONVERSATIONAL MODE (OPTION 5)
        Print(L"=== Conversational Mode with Commands ===\r\n");
        Print(L"Type /help for available commands\r\n");
        
        // Initialize conversation history
        ConversationHistory history;
        init_conversation(&history);
        
        // Select prompts based on model type
        const char** demo_prompts;
        int num_prompts;
        
        if (transformer.config.model_type == MODEL_TINYLLAMA_CHAT) {
            Print(L"Chat model: TinyLlama-1.1B-Chat\r\n");
            Print(L"(Auto-demo mode - keyboard disabled in QEMU)\r\n\r\n");
            static const char* chat_prompts[] = {
                "Hello! How are you today?",
                "/stats",
                "What is the capital of France?",
                "/temp 0.7",
                "Tell me a short joke",
                "/history"
            };
            demo_prompts = chat_prompts;
            num_prompts = 6;
        } else if (transformer.config.model_type == MODEL_NANOGPT) {
            Print(L"Completion model: NanoGPT-124M\r\n");
            Print(L"(Auto-demo mode - keyboard disabled in QEMU)\r\n\r\n");
            static const char* gpt_prompts[] = {
                "The quick brown fox",
                "/stats",
                "In a distant galaxy",
                "/clear"
            };
            demo_prompts = gpt_prompts;
            num_prompts = 4;
        } else {
            Print(L"Story model: stories15M\r\n");
            Print(L"(Auto-demo mode - keyboard disabled in QEMU)\r\n\r\n");
            static const char* story_prompts[] = {
                "Once upon a time",
                "/stats",
                "The brave knight",
                "/history"
            };
            demo_prompts = story_prompts;
            num_prompts = 4;
        }
        
        char user_input[512];
        char response_buffer[MAX_RESPONSE_LENGTH];
        int conversation_pos = 0;  // Track position in conversation
        
        for (int demo_idx = 0; demo_idx < num_prompts; demo_idx++) {
            // Display turn indicator
            Print(L"\r\n[Turn %d/%d]\r\n", demo_idx + 1, num_prompts);
            
            // Copy demo prompt
            const char* prompt = demo_prompts[demo_idx];
            int prompt_len = 0;
            while (prompt[prompt_len] && prompt_len < 511) {
                user_input[prompt_len] = prompt[prompt_len];
                prompt_len++;
            }
            user_input[prompt_len] = '\0';
            
            // Display user input
            Print(L"User>>> ");
            for (int i = 0; user_input[i]; i++) {
                Print(L"%c", (CHAR16)user_input[i]);
            }
            Print(L"\r\n");
            
            // Check if it's a command
            if (is_command(user_input)) {
                int cmd_result = process_command(user_input, &history, SystemTable);
                if (cmd_result == 2) {
                    // Exit command
                    break;
                }
                continue;  // Skip generation for commands
            }
            
            Print(L"Assistant>>> ");
            
            int token;
            int max_response_tokens = history.max_response_tokens;
            int response_len = 0;
            response_buffer[0] = '\0';
            
            // Start token
            if (conversation_pos == 0) {
                token = 1;  // BOS token
            } else {
                token = 1;  // Always restart for demo (no context carryover)
            }
            
            int prompt_tokens = 0;  // Would be from encode_prompt() in real impl
            int response_tokens = 0;
            
            // Generate response
            for (int i = 0; i < max_response_tokens; i++) {
                float* logits = forward(&transformer, token, conversation_pos + i);
                
                if (logits == NULL) {
                    Print(L"[ERROR] Forward pass failed\r\n");
                    break;
                }
                
                // Sample next token with current temperature
                int next;
                if (history.temperature == 0.0f) {
                    next = argmax(logits, transformer.config.vocab_size);
                } else {
                    for (int j = 0; j < transformer.config.vocab_size; j++) {
                        logits[j] /= history.temperature;
                    }
                    softmax(logits, transformer.config.vocab_size);
                    float coin = (float)rand_efi() / (float)RAND_MAX;
                    next = sample_mult(logits, transformer.config.vocab_size, coin);
                }
                
                response_tokens++;
                
                // Check for EOS token (end of sequence)
                if (next == 2) {
                    Print(L"[EOS]\r\n");
                    break;
                }
                
                // Decode and print
                if (use_text) {
                    char* piece = decode_token(&tokenizer, next);
                    CHAR16 wpiece[256];
                    for (int k = 0; k < 255 && piece[k]; k++) {
                        wpiece[k] = (CHAR16)piece[k];
                        wpiece[k+1] = 0;
                    }
                    Print(L"%s", wpiece);
                    
                    // Store in response buffer
                    for (int k = 0; piece[k] && response_len < MAX_RESPONSE_LENGTH - 1; k++) {
                        response_buffer[response_len++] = piece[k];
                    }
                } else {
                    Print(L"[%d] ", next);
                }
                
                token = next;
            }
            response_buffer[response_len] = '\0';
            
            
            // Add to conversation history
            add_turn(&history, user_input, response_buffer, prompt_tokens, response_tokens);
            
            Print(L"\r\n");
            Print(L"[Tokens: ~%d | Temp: %.2f | Total: %d]\r\n", 
                  response_tokens, (double)history.temperature, history.total_tokens);
            Print(L"--------------------------------------------------\r\n");
            
            conversation_pos += max_response_tokens;
            
            // Small delay between turns
            SystemTable->BootServices->Stall(1500000); // 1.5 seconds
            
            // Reset if conversation gets too long
            if (conversation_pos > transformer.config.seq_len - 100) {
                conversation_pos = 0;
                Print(L"\r\n[Context window limit reached - resetting]\r\n\r\n");
            }
        }
        
        Print(L"\r\n=== Conversation Session Complete ===\r\n");
        Print(L"Total turns: %d\r\n", history.count);
        Print(L"Total tokens: %d\r\n", history.total_tokens);
        Print(L"\r\nNote: Keyboard input disabled in QEMU/OVMF.\r\n");
        Print(L"For full interactive mode, test on real UEFI hardware.\r\n\r\n");
    }
    
    Print(L"\r\nSession ended.\r\n");
    
    // Small delay before exit
    ST->BootServices->Stall(2000000); // 2 seconds
    
    return EFI_SUCCESS;
}
