/* DjibMark - Omnipresent execution tracing system
 * Made in Senegal 🇸🇳 by Djiby Diop
 * 
 * A lightweight, zero-overhead tracing system that marks every critical
 * execution path with a unique signature. Useful for performance analysis,
 * debugging, and post-mortem diagnostics.
 */

#ifndef DJIBMARK_H
#define DJIBMARK_H

#include <efi.h>
#include <efilib.h>

// Magic number: 0xD31B2026 = "DJIB" + year 2026
#define DJIBMARK_MAGIC 0xD31B2026

// Ring buffer size for marks (power of 2 for fast modulo)
#define DJIBMARK_RING_SIZE 256

// DjibMark entry - minimal overhead (24 bytes)
typedef struct {
    UINT32 magic;              // 0xD31B2026 (validation)
    UINT32 sequence;           // Global sequence number
    UINT64 timestamp_tsc;      // TSC timestamp (for performance analysis)
    const CHAR8* location;     // Function name (__func__)
    UINT16 line;               // Line number (__LINE__)
    UINT16 phase;              // Execution phase (0=boot, 1=prefill, 2=decode, 3=repl)
} DjibMark;

// Global state
typedef struct {
    DjibMark ring[DJIBMARK_RING_SIZE];
    UINT32 idx;                // Current write position
    UINT32 total_marks;        // Total marks recorded (wraps at UINT32_MAX)
    BOOLEAN enabled;           // Can be disabled for performance
} DjibMarkState;

// Global instance
extern DjibMarkState g_djibmark_state;

// Phase identifiers
#define DJIBMARK_PHASE_BOOT     0
#define DJIBMARK_PHASE_PREFILL  1
#define DJIBMARK_PHASE_DECODE   2
#define DJIBMARK_PHASE_REPL     3

// Core API
static inline void djibmark_init(void) {
    for (int i = 0; i < DJIBMARK_RING_SIZE; i++) {
        g_djibmark_state.ring[i].magic = 0;
        g_djibmark_state.ring[i].sequence = 0;
        g_djibmark_state.ring[i].timestamp_tsc = 0;
        g_djibmark_state.ring[i].location = NULL;
        g_djibmark_state.ring[i].line = 0;
        g_djibmark_state.ring[i].phase = 0;
    }
    g_djibmark_state.idx = 0;
    g_djibmark_state.total_marks = 0;
    g_djibmark_state.enabled = TRUE;
}

static inline UINT64 djibmark_rdtsc(void) {
    UINT32 lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((UINT64)hi << 32) | lo;
}

static inline void djibmark_record(const CHAR8* func, UINT16 line, UINT16 phase) {
    if (!g_djibmark_state.enabled) return;
    
    UINT32 idx = g_djibmark_state.idx % DJIBMARK_RING_SIZE;
    DjibMark* m = &g_djibmark_state.ring[idx];
    
    m->magic = DJIBMARK_MAGIC;
    m->sequence = g_djibmark_state.total_marks++;
    m->timestamp_tsc = djibmark_rdtsc();
    m->location = func;
    m->line = line;
    m->phase = phase;
    
    g_djibmark_state.idx++;
}

// Convenience macros (omnipresent in code)
#define DJIBMARK_BOOT()     djibmark_record(__func__, __LINE__, DJIBMARK_PHASE_BOOT)
#define DJIBMARK_PREFILL()  djibmark_record(__func__, __LINE__, DJIBMARK_PHASE_PREFILL)
#define DJIBMARK_DECODE()   djibmark_record(__func__, __LINE__, DJIBMARK_PHASE_DECODE)
#define DJIBMARK_REPL()     djibmark_record(__func__, __LINE__, DJIBMARK_PHASE_REPL)
#define DJIBMARK()          djibmark_record(__func__, __LINE__, DJIBMARK_PHASE_REPL)

// Query API
static inline UINT32 djibmark_count(void) {
    return (g_djibmark_state.total_marks < DJIBMARK_RING_SIZE) 
           ? g_djibmark_state.total_marks 
           : DJIBMARK_RING_SIZE;
}

static inline DjibMark* djibmark_get(UINT32 index) {
    if (index >= djibmark_count()) return NULL;
    
    // Get from ring buffer (newest first)
    UINT32 offset = (g_djibmark_state.idx - 1 - index) % DJIBMARK_RING_SIZE;
    return &g_djibmark_state.ring[offset];
}

static inline const CHAR16* djibmark_phase_name(UINT16 phase) {
    switch (phase) {
        case DJIBMARK_PHASE_BOOT:    return L"BOOT";
        case DJIBMARK_PHASE_PREFILL: return L"PREFILL";
        case DJIBMARK_PHASE_DECODE:  return L"DECODE";
        case DJIBMARK_PHASE_REPL:    return L"REPL";
        default:                     return L"UNKNOWN";
    }
}

#endif // DJIBMARK_H
