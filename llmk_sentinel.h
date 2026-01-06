#ifndef LLMK_SENTINEL_H
#define LLMK_SENTINEL_H

#include <efi.h>
#include <efilib.h>

#include "llmk_zones.h"
#include "llmk_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LLMK_OK = 0,
    LLMK_ERR_ALLOC = 1,
    LLMK_ERR_OOB = 2,
    LLMK_ERR_RO_WRITE = 3,
    LLMK_ERR_BUDGET = 4,
} LlmkError;

typedef struct {
    BOOLEAN enabled;
    // Legacy: if strict_mode is TRUE, both strict_alloc and strict_budget are treated as TRUE.
    BOOLEAN strict_mode;
    BOOLEAN strict_alloc;
    BOOLEAN strict_budget;

    // Budget in cycles. 0 = disabled.
    // If max_cycles_prefill/decode are 0, max_cycles is used as fallback.
    UINT64 max_cycles;
    UINT64 max_cycles_prefill;
    UINT64 max_cycles_decode;

    // If true, print one-line logs on violation.
    BOOLEAN log_violations;
} LlmkSentinelConfig;

typedef struct {
    LlmkSentinelConfig cfg;
    const LlmkZones *zones;

    LlmkLog *log;

    // Budget tracking
    UINT64 cycle_start;

    // Last phase measurement (for adaptive budgeting)
    UINT64 last_dt_cycles;
    UINT64 last_budget_cycles;

    // Current phase (for budget enforcement + logging)
    UINT32 phase;

    // Last error
    LlmkError last_error;
    CHAR16 last_reason[64];

    BOOLEAN tripped;
} LlmkSentinel;

EFI_STATUS llmk_sentinel_init(LlmkSentinel *s, const LlmkZones *zones, LlmkLog *log, const LlmkSentinelConfig *cfg);

void llmk_sentinel_cycle_start(LlmkSentinel *s);
BOOLEAN llmk_sentinel_cycle_end(LlmkSentinel *s);

typedef enum {
    LLMK_PHASE_GENERIC = 0,
    LLMK_PHASE_PREFILL = 1,
    LLMK_PHASE_DECODE = 2,
} LlmkPhase;

void llmk_sentinel_phase_start(LlmkSentinel *s, LlmkPhase phase);
BOOLEAN llmk_sentinel_phase_end(LlmkSentinel *s);

BOOLEAN llmk_sentinel_check_read(const LlmkSentinel *s, UINT64 ptr, UINT64 size);
BOOLEAN llmk_sentinel_check_write(const LlmkSentinel *s, UINT64 ptr, UINT64 size);

// Allocation wrapper: routes through zones allocator and logs failures.
// If strict_mode is enabled, allocation failures trip fail-safe.
void *llmk_sentinel_alloc(LlmkSentinel *s, LlmkArenaId arena, UINT64 size, UINT64 align, const CHAR16 *tag);

void llmk_sentinel_fail_safe(LlmkSentinel *s, const CHAR16 *reason);
void llmk_sentinel_print_status(const LlmkSentinel *s);

#ifdef __cplusplus
}
#endif

#endif
