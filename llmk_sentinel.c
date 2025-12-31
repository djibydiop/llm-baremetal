#include "llmk_sentinel.h"

static UINT64 rdtsc_llmk(void) {
    UINT32 lo, hi;
    __asm__ __volatile__("lfence\nrdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return ((UINT64)hi << 32) | (UINT64)lo;
}

static void set_reason(CHAR16 dst[64], const CHAR16 *src) {
    for (int i = 0; i < 63; i++) {
        if (!src || !src[i]) {
            dst[i] = 0;
            return;
        }
        dst[i] = src[i];
    }
    dst[63] = 0;
}

EFI_STATUS llmk_sentinel_init(LlmkSentinel *s, const LlmkZones *zones, const LlmkSentinelConfig *cfg) {
    if (!s || !zones || !cfg) return EFI_INVALID_PARAMETER;
    s->cfg = *cfg;
    s->zones = zones;
    s->cycle_start = 0;
    s->last_error = LLMK_OK;
    set_reason(s->last_reason, L"");
    return EFI_SUCCESS;
}

void llmk_sentinel_cycle_start(LlmkSentinel *s) {
    if (!s || !s->cfg.enabled) return;
    if (s->cfg.max_cycles == 0) return;
    s->cycle_start = rdtsc_llmk();
}

BOOLEAN llmk_sentinel_cycle_end(LlmkSentinel *s) {
    if (!s || !s->cfg.enabled) return TRUE;
    if (s->cfg.max_cycles == 0) return TRUE;

    UINT64 end = rdtsc_llmk();
    UINT64 dt = (end >= s->cycle_start) ? (end - s->cycle_start) : 0;
    if (dt > s->cfg.max_cycles) {
        s->last_error = LLMK_ERR_BUDGET;
        set_reason(s->last_reason, L"budget cycles exceeded");
        if (s->cfg.log_violations) {
            Print(L"[llmk][sentinel] budget exceeded: cycles=%lu max=%lu\r\n", dt, s->cfg.max_cycles);
        }
        return FALSE;
    }

    return TRUE;
}

static BOOLEAN ptr_in_any_arena(const LlmkZones *z, UINT64 ptr, UINT64 size, int *out_arena) {
    for (int i = 0; i < LLMK_ARENA_COUNT; i++) {
        if (llmk_ptr_in_arena(z, (LlmkArenaId)i, ptr, size)) {
            if (out_arena) *out_arena = i;
            return TRUE;
        }
    }
    return FALSE;
}

BOOLEAN llmk_sentinel_check_read(const LlmkSentinel *s, UINT64 ptr, UINT64 size) {
    if (!s || !s->cfg.enabled) return TRUE;
    if (!s->zones) return FALSE;

    int arena = -1;
    if (!ptr_in_any_arena(s->zones, ptr, size, &arena)) {
        if (s->cfg.log_violations) {
            Print(L"[llmk][sentinel] read OOB: ptr=0x%lx size=%lu\r\n", ptr, size);
        }
        return FALSE;
    }
    (void)arena;
    return TRUE;
}

BOOLEAN llmk_sentinel_check_write(const LlmkSentinel *s, UINT64 ptr, UINT64 size) {
    if (!s || !s->cfg.enabled) return TRUE;
    if (!s->zones) return FALSE;

    int arena = -1;
    if (!ptr_in_any_arena(s->zones, ptr, size, &arena)) {
        if (s->cfg.log_violations) {
            Print(L"[llmk][sentinel] write OOB: ptr=0x%lx size=%lu\r\n", ptr, size);
        }
        return FALSE;
    }

    const LlmkArena *a = &s->zones->arenas[arena];
    if ((a->flags & LLMK_ARENA_FLAG_READONLY) != 0) {
        if (s->cfg.log_violations) {
            Print(L"[llmk][sentinel] write denied (RO arena %s): ptr=0x%lx size=%lu\r\n", a->name, ptr, size);
        }
        return FALSE;
    }

    return TRUE;
}

void llmk_sentinel_fail_safe(LlmkSentinel *s, const CHAR16 *reason) {
    if (!s) return;
    s->last_error = (s->last_error == LLMK_OK) ? LLMK_ERR_OOB : s->last_error;
    set_reason(s->last_reason, reason ? reason : L"fail-safe");

    // Minimal fail-safe behavior for now: just print and return control.
    // (Next step: wipe scratch/activations + log into Zone C ring buffer.)
    Print(L"[llmk][fail-safe] %s\r\n", s->last_reason);
}

void llmk_sentinel_print_status(const LlmkSentinel *s) {
    if (!s) return;
    Print(L"[llmk][sentinel] enabled=%d strict=%d max_cycles=%lu last_err=%d reason=%s\r\n",
          s->cfg.enabled ? 1 : 0,
          s->cfg.strict_mode ? 1 : 0,
          s->cfg.max_cycles,
          (int)s->last_error,
          s->last_reason);
}
