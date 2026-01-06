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

EFI_STATUS llmk_sentinel_init(LlmkSentinel *s, const LlmkZones *zones, LlmkLog *log, const LlmkSentinelConfig *cfg) {
    if (!s || !zones || !cfg) return EFI_INVALID_PARAMETER;
    s->cfg = *cfg;

    // Back-compat: strict_mode implies strict_alloc + strict_budget.
    if (s->cfg.strict_mode) {
        s->cfg.strict_alloc = TRUE;
        s->cfg.strict_budget = TRUE;
    }

    s->zones = zones;
    s->log = log;
    s->cycle_start = 0;
    s->last_dt_cycles = 0;
    s->last_budget_cycles = 0;
    s->phase = LLMK_PHASE_GENERIC;
    s->last_error = LLMK_OK;
    set_reason(s->last_reason, L"");
    s->tripped = FALSE;
    return EFI_SUCCESS;
}

static UINT64 budget_for_phase(const LlmkSentinelConfig *cfg, UINT32 phase) {
    if (!cfg) return 0;
    if (phase == LLMK_PHASE_PREFILL) {
        return (cfg->max_cycles_prefill != 0) ? cfg->max_cycles_prefill : cfg->max_cycles;
    }
    if (phase == LLMK_PHASE_DECODE) {
        return (cfg->max_cycles_decode != 0) ? cfg->max_cycles_decode : cfg->max_cycles;
    }
    return cfg->max_cycles;
}

static const CHAR16 *phase_name(UINT32 phase) {
    switch (phase) {
        case LLMK_PHASE_PREFILL: return L"prefill";
        case LLMK_PHASE_DECODE: return L"decode";
        default: return L"generic";
    }
}

void llmk_sentinel_cycle_start(LlmkSentinel *s) {
    llmk_sentinel_phase_start(s, LLMK_PHASE_GENERIC);
}

BOOLEAN llmk_sentinel_cycle_end(LlmkSentinel *s) {
    return llmk_sentinel_phase_end(s);
}

void llmk_sentinel_phase_start(LlmkSentinel *s, LlmkPhase phase) {
    if (!s || !s->cfg.enabled) return;
    if (s->tripped) return;
    s->phase = (UINT32)phase;

    UINT64 budget = budget_for_phase(&s->cfg, s->phase);
    if (budget == 0) return;

    s->cycle_start = rdtsc_llmk();
}

BOOLEAN llmk_sentinel_phase_end(LlmkSentinel *s) {
    if (!s || !s->cfg.enabled) return TRUE;
    if (s->tripped) return FALSE;

    UINT64 budget = budget_for_phase(&s->cfg, s->phase);
    if (budget == 0) return TRUE;

    UINT64 end = rdtsc_llmk();
    UINT64 dt = (end >= s->cycle_start) ? (end - s->cycle_start) : 0;

    s->last_dt_cycles = dt;
    s->last_budget_cycles = budget;

    if (dt > budget) {
        s->last_error = LLMK_ERR_BUDGET;
        // Keep reason short + stable (we tag phase separately in logs).
        set_reason(s->last_reason, L"budget cycles exceeded");

        if (s->cfg.log_violations) {
            Print(L"[llmk][sentinel] budget exceeded (%s): cycles=%lu max=%lu\r\n", phase_name(s->phase), dt, budget);
        }

        if (s->log) {
            // Encode dt/max in ptr/size; store phase in arena field.
            llmk_log_event(s->log, LLMK_EVT_BUDGET, (INT32)s->phase, dt, budget, s->last_reason);
        }

        if (s->cfg.strict_budget) {
            llmk_sentinel_fail_safe(s, s->last_reason);
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
    if (s->tripped) return FALSE;

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
    if (s->tripped) return FALSE;

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

void *llmk_sentinel_alloc(LlmkSentinel *s, LlmkArenaId arena, UINT64 size, UINT64 align, const CHAR16 *tag) {
    if (!s) return NULL;
    if (!s->zones) return NULL;
    if (s->tripped) return NULL;

    // Cast away const: allocation mutates cursors.
    LlmkZones *z = (LlmkZones *)(UINTN)s->zones;

    void *p = llmk_arena_alloc_checked(z, arena, size, align, s->log, tag);
    if (p) return p;

    s->last_error = LLMK_ERR_ALLOC;
    set_reason(s->last_reason, tag ? tag : L"alloc failed");

    if (s->cfg.log_violations) {
        UINT64 rem = llmk_arena_remaining_bytes(z, arena);
        Print(L"[llmk][sentinel] alloc failed arena=%d size=%lu remaining=%lu tag=%s\r\n",
              (int)arena, size, rem, s->last_reason);
    }

    if (s->cfg.strict_alloc) {
        llmk_sentinel_fail_safe(s, s->last_reason);
    }

    return NULL;
}

void llmk_sentinel_fail_safe(LlmkSentinel *s, const CHAR16 *reason) {
    if (!s) return;
    if (s->tripped) return;
    s->tripped = TRUE;

    s->last_error = (s->last_error == LLMK_OK) ? LLMK_ERR_OOB : s->last_error;
    set_reason(s->last_reason, reason ? reason : L"fail-safe");

    // Log event into Zone C ring buffer if available.
    if (s->log) {
        llmk_log_event(s->log, LLMK_EVT_FAILSAFE, -1, 0, 0, s->last_reason);
    }

    // Wipe volatile arenas and reset cursors (controlled shutdown behavior).
    if (s->zones) {
        // Cast away const: fail-safe is allowed to wipe volatile arenas.
        LlmkZones *z = (LlmkZones *)(UINTN)s->zones;
        llmk_arena_wipe_and_reset(z, LLMK_ARENA_SCRATCH, 0);
        llmk_arena_wipe_and_reset(z, LLMK_ARENA_ACTIVATIONS, 0);
    }

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
