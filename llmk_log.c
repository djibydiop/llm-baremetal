#include "llmk_log.h"

static UINT64 rdtsc_llmk(void) {
    UINT32 lo, hi;
    __asm__ __volatile__("lfence\nrdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return ((UINT64)hi << 32) | (UINT64)lo;
}

static void set_msg(CHAR16 dst[48], const CHAR16 *src) {
    for (int i = 0; i < 47; i++) {
        if (!src || !src[i]) {
            dst[i] = 0;
            return;
        }
        dst[i] = src[i];
    }
    dst[47] = 0;
}

EFI_STATUS llmk_log_init(LlmkZones *zones, LlmkLog *out) {
    if (!zones || !out) return EFI_INVALID_PARAMETER;

    // Keep it small and predictable.
    const UINT32 cap = 128;

    void *mem = llmk_arena_alloc(zones, LLMK_ARENA_ZONE_C, (UINT64)cap * sizeof(LlmkLogEntry), 64);
    if (!mem) return EFI_OUT_OF_RESOURCES;

    // Zero-init entries.
    UINT8 *p = (UINT8 *)mem;
    for (UINT64 i = 0; i < (UINT64)cap * sizeof(LlmkLogEntry); i++) p[i] = 0;

    out->entries = (LlmkLogEntry *)mem;
    out->capacity = cap;
    out->write_idx = 0;

    llmk_log_event(out, LLMK_EVT_INFO, -1, 0, 0, L"log init");
    return EFI_SUCCESS;
}

void llmk_log_event(LlmkLog *log, UINT32 code, INT32 arena, UINT64 ptr, UINT64 size, const CHAR16 *msg) {
    if (!log || !log->entries || log->capacity == 0) return;

    UINT32 idx = log->write_idx++ % log->capacity;
    LlmkLogEntry *e = &log->entries[idx];
    e->tsc = rdtsc_llmk();
    e->code = code;
    e->arena = arena;
    e->ptr = ptr;
    e->size = size;
    set_msg(e->msg, msg);
}

void llmk_log_dump(const LlmkLog *log, UINT32 max_entries) {
    if (!log || !log->entries || log->capacity == 0) return;

    UINT32 n = log->capacity;
    if (max_entries != 0 && max_entries < n) n = max_entries;

    Print(L"[llmk][log] last %u events (ring cap=%u)\r\n", n, log->capacity);

    // Dump most recent first.
    UINT32 w = log->write_idx;
    for (UINT32 i = 0; i < n; i++) {
        UINT32 off = (w + log->capacity - 1 - i) % log->capacity;
        const LlmkLogEntry *e = &log->entries[off];
        if (e->tsc == 0 && e->code == 0 && e->msg[0] == 0) continue;
        Print(L"  #%u tsc=%lu code=%u arena=%d ptr=0x%lx size=%lu msg=%s\r\n",
              i, e->tsc, e->code, e->arena, e->ptr, e->size, e->msg);
    }
}
