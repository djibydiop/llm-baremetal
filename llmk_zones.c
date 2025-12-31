#include "llmk_zones.h"

static UINT64 align_up_u64(UINT64 x, UINT64 a) {
    if (a == 0) return x;
    return (x + (a - 1)) & ~(a - 1);
}

static void set_name(CHAR16 dst[16], const CHAR16 *src) {
    for (int i = 0; i < 15; i++) {
        if (!src[i]) { dst[i] = 0; return; }
        dst[i] = src[i];
    }
    dst[15] = 0;
}

static void init_arena(LlmkArena *a, UINT64 base, UINT64 size, UINT32 flags, const CHAR16 *name) {
    a->base = base;
    a->size = size;
    a->cursor = 0;
    a->flags = flags;
    set_name(a->name, name);
}

static void compute_default_split(UINT64 total,
                                  UINT64 *weights,
                                  UINT64 *kv,
                                  UINT64 *scratch,
                                  UINT64 *acts,
                                  UINT64 *zonec) {
    // Conservative defaults for LLM inference.
    // weights: 70%, kv: 15%, scratch: 10%, acts: 4%, zonec: 1%
    UINT64 w = (total * 70) / 100;
    UINT64 k = (total * 15) / 100;
    UINT64 s = (total * 10) / 100;
    UINT64 a = (total * 4) / 100;
    UINT64 c = total - (w + k + s + a);
    *weights = w;
    *kv = k;
    *scratch = s;
    *acts = a;
    *zonec = c;
}

EFI_STATUS llmk_zones_init(EFI_BOOT_SERVICES *BS, const LlmkZonesConfig *cfg_in, LlmkZones *out) {
    if (!BS || !out) return EFI_INVALID_PARAMETER;

    LlmkZonesConfig cfg = {0};
    if (cfg_in) cfg = *cfg_in;

    // Default Zone B size: 768 MiB
    if (cfg.total_bytes == 0) {
        cfg.total_bytes = 768ULL * 1024ULL * 1024ULL;
    }

    // If per-arena sizes are not provided, compute a default split.
    if (cfg.weights_bytes == 0 || cfg.kv_bytes == 0 || cfg.scratch_bytes == 0 || cfg.activations_bytes == 0 || cfg.zone_c_bytes == 0) {
        compute_default_split(cfg.total_bytes, &cfg.weights_bytes, &cfg.kv_bytes, &cfg.scratch_bytes, &cfg.activations_bytes, &cfg.zone_c_bytes);
    }

    UINT64 sum = cfg.weights_bytes + cfg.kv_bytes + cfg.scratch_bytes + cfg.activations_bytes + cfg.zone_c_bytes;
    if (sum > cfg.total_bytes) {
        return EFI_INVALID_PARAMETER;
    }

    EFI_PHYSICAL_ADDRESS base = 0;
    UINTN pages = (UINTN)((cfg.total_bytes + 4095ULL) / 4096ULL);

    EFI_STATUS st = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages, &base);
    if (EFI_ERROR(st)) {
        return st;
    }

    out->zone_b_base = base;
    out->zone_b_size = cfg.total_bytes;

    UINT64 cur = (UINT64)base;

    init_arena(&out->arenas[LLMK_ARENA_WEIGHTS], cur, cfg.weights_bytes, LLMK_ARENA_FLAG_READONLY, L"WEIGHTS");
    cur += cfg.weights_bytes;

    init_arena(&out->arenas[LLMK_ARENA_KV_CACHE], cur, cfg.kv_bytes, LLMK_ARENA_FLAG_NONE, L"KV");
    cur += cfg.kv_bytes;

    init_arena(&out->arenas[LLMK_ARENA_SCRATCH], cur, cfg.scratch_bytes, LLMK_ARENA_FLAG_NONE, L"SCRATCH");
    cur += cfg.scratch_bytes;

    init_arena(&out->arenas[LLMK_ARENA_ACTIVATIONS], cur, cfg.activations_bytes, LLMK_ARENA_FLAG_NONE, L"ACTS");
    cur += cfg.activations_bytes;

    init_arena(&out->arenas[LLMK_ARENA_ZONE_C], cur, cfg.zone_c_bytes, LLMK_ARENA_FLAG_NONE, L"ZONEC");

    return EFI_SUCCESS;
}

void *llmk_arena_alloc(LlmkZones *zones, LlmkArenaId arena, UINT64 size, UINT64 align) {
    if (!zones) return NULL;
    if ((int)arena < 0 || arena >= LLMK_ARENA_COUNT) return NULL;
    if (size == 0) return NULL;

    LlmkArena *a = &zones->arenas[arena];
    UINT64 cur = a->base + a->cursor;
    UINT64 aligned = align_up_u64(cur, (align == 0 ? 16ULL : align));
    UINT64 new_cursor = (aligned + size) - a->base;

    if (new_cursor > a->size) {
        return NULL;
    }

    a->cursor = new_cursor;
    return (void *)(UINTN)aligned;
}

void llmk_arena_reset(LlmkZones *zones, LlmkArenaId arena) {
    if (!zones) return;
    if ((int)arena < 0 || arena >= LLMK_ARENA_COUNT) return;
    zones->arenas[arena].cursor = 0;
}

BOOLEAN llmk_ptr_in_arena(const LlmkZones *zones, LlmkArenaId arena, UINT64 ptr, UINT64 size) {
    if (!zones) return FALSE;
    if ((int)arena < 0 || arena >= LLMK_ARENA_COUNT) return FALSE;

    const LlmkArena *a = &zones->arenas[arena];
    UINT64 start = a->base;
    UINT64 end = a->base + a->size;

    if (ptr < start) return FALSE;
    if (size > (end - ptr)) return FALSE;
    return TRUE;
}

void llmk_zones_print(const LlmkZones *zones) {
    if (!zones) return;

    Print(L"[llmk] Zone B: base=0x%lx size=%lu MiB\r\n", (UINT64)zones->zone_b_base, zones->zone_b_size / (1024ULL * 1024ULL));
    for (int i = 0; i < LLMK_ARENA_COUNT; i++) {
        const LlmkArena *a = &zones->arenas[i];
        Print(L"  [%s] base=0x%lx size=%lu MiB used=%lu MiB flags=0x%x\r\n",
              a->name,
              a->base,
              a->size / (1024ULL * 1024ULL),
              a->cursor / (1024ULL * 1024ULL),
              (unsigned)a->flags);
    }
}
