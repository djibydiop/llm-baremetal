#ifndef LLMK_SENTINEL_H
#define LLMK_SENTINEL_H

#include <efi.h>
#include <efilib.h>

#include "llmk_zones.h"

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
    BOOLEAN strict_mode;

    // 0 = disabled
    UINT64 max_cycles;

    // If true, print one-line logs on violation.
    BOOLEAN log_violations;
} LlmkSentinelConfig;

typedef struct {
    LlmkSentinelConfig cfg;
    const LlmkZones *zones;

    // Budget tracking
    UINT64 cycle_start;

    // Last error
    LlmkError last_error;
    CHAR16 last_reason[64];
} LlmkSentinel;

EFI_STATUS llmk_sentinel_init(LlmkSentinel *s, const LlmkZones *zones, const LlmkSentinelConfig *cfg);

void llmk_sentinel_cycle_start(LlmkSentinel *s);
BOOLEAN llmk_sentinel_cycle_end(LlmkSentinel *s);

BOOLEAN llmk_sentinel_check_read(const LlmkSentinel *s, UINT64 ptr, UINT64 size);
BOOLEAN llmk_sentinel_check_write(const LlmkSentinel *s, UINT64 ptr, UINT64 size);

void llmk_sentinel_fail_safe(LlmkSentinel *s, const CHAR16 *reason);
void llmk_sentinel_print_status(const LlmkSentinel *s);

#ifdef __cplusplus
}
#endif

#endif
