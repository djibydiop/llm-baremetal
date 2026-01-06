#ifndef LLMK_LOG_H
#define LLMK_LOG_H

#include <efi.h>
#include <efilib.h>

#include "llmk_zones.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LLMK_EVT_INFO = 0,
    LLMK_EVT_ALLOC_FAIL = 1,
    LLMK_EVT_OOB = 2,
    LLMK_EVT_RO_WRITE = 3,
    LLMK_EVT_BUDGET = 4,
    LLMK_EVT_FAILSAFE = 5,
    LLMK_EVT_TOKEN = 6,
} LlmkEventCode;

typedef struct {
    UINT64 tsc;
    UINT32 code;
    INT32 arena;
    UINT64 ptr;
    UINT64 size;
    CHAR16 msg[48];
} LlmkLogEntry;

typedef struct LlmkLog {
    LlmkLogEntry *entries;
    UINT32 capacity;
    UINT32 write_idx;
} LlmkLog;

EFI_STATUS llmk_log_init(LlmkZones *zones, LlmkLog *out);
void llmk_log_event(LlmkLog *log, UINT32 code, INT32 arena, UINT64 ptr, UINT64 size, const CHAR16 *msg);
void llmk_log_dump(const LlmkLog *log, UINT32 max_entries);

#ifdef __cplusplus
}
#endif

#endif
