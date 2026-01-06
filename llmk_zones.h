#ifndef LLMK_ZONES_H
#define LLMK_ZONES_H

#include <efi.h>
#include <efilib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration to avoid a circular include with llmk_log.h.
typedef struct LlmkLog LlmkLog;

// Zone B sub-arenas
typedef enum {
    LLMK_ARENA_WEIGHTS = 0,
    LLMK_ARENA_KV_CACHE = 1,
    LLMK_ARENA_SCRATCH = 2,
    LLMK_ARENA_ACTIVATIONS = 3,
    LLMK_ARENA_ZONE_C = 4,
    LLMK_ARENA_COUNT = 5,
} LlmkArenaId;

typedef enum {
    LLMK_ARENA_FLAG_NONE = 0,
    LLMK_ARENA_FLAG_READONLY = 1 << 0,
} LlmkArenaFlags;

typedef struct {
    UINT64 base;
    UINT64 size;
    UINT64 cursor;
    UINT32 flags;
    CHAR16 name[16];
} LlmkArena;

typedef struct {
    EFI_PHYSICAL_ADDRESS zone_b_base;
    UINT64 zone_b_size;
    LlmkArena arenas[LLMK_ARENA_COUNT];
} LlmkZones;

typedef struct {
    // If total_bytes == 0, llmk_zones_init uses a default.
    UINT64 total_bytes;

    // If any of these are 0, llmk_zones_init uses a default split.
    UINT64 weights_bytes;
    UINT64 kv_bytes;
    UINT64 scratch_bytes;
    UINT64 activations_bytes;
    UINT64 zone_c_bytes;
} LlmkZonesConfig;

EFI_STATUS llmk_zones_init(EFI_BOOT_SERVICES *BS, const LlmkZonesConfig *cfg, LlmkZones *out);

// Validate that arenas are within Zone B, non-overlapping, and ordered.
BOOLEAN llmk_zones_validate(const LlmkZones *zones);

UINT64 llmk_arena_used_bytes(const LlmkZones *zones, LlmkArenaId arena);
UINT64 llmk_arena_remaining_bytes(const LlmkZones *zones, LlmkArenaId arena);

void *llmk_arena_alloc(LlmkZones *zones, LlmkArenaId arena, UINT64 size, UINT64 align);

// Like llmk_arena_alloc(), but optionally logs alloc failures.
void *llmk_arena_alloc_checked(LlmkZones *zones, LlmkArenaId arena, UINT64 size, UINT64 align, LlmkLog *log, const CHAR16 *tag);
void llmk_arena_reset(LlmkZones *zones, LlmkArenaId arena);

// Wipe the used region of an arena with a byte pattern (0 = zero), then reset cursor to 0.
void llmk_arena_wipe_and_reset(LlmkZones *zones, LlmkArenaId arena, UINT8 pattern);

BOOLEAN llmk_ptr_in_arena(const LlmkZones *zones, LlmkArenaId arena, UINT64 ptr, UINT64 size);

void llmk_zones_print(const LlmkZones *zones);

#ifdef __cplusplus
}
#endif

#endif
