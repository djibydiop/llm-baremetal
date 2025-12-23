// memory_zones.h - Zone definitions for LLM-Kernel
// Simplified version - zones system disabled due to bare-metal incompatibility

#ifndef MEMORY_ZONES_H
#define MEMORY_ZONES_H

#include <efi.h>
#include <efilib.h>

typedef enum {
    ZONE_A_HOSTILE = 0,
    ZONE_B_SACRED = 1,
    ZONE_C_RESERVED = 2
} MemoryZoneType;

typedef enum {
    ARENA_WEIGHTS = 0,
    ARENA_KV_CACHE = 1,
    ARENA_SCRATCH = 2,
    ARENA_OUTPUT = 3,
    ARENA_COUNT = 4
} ArenaType;

typedef struct {
    MemoryZoneType type;
    UINTN start_addr;
    UINTN end_addr;
    UINTN size;
    const CHAR16* name;
    BOOLEAN writable;
    BOOLEAN executable;
} MemoryZone;

typedef struct {
    ArenaType type;
    UINTN start_addr;
    UINTN end_addr;
    UINTN size;
    UINTN used;
    const CHAR16* name;
    BOOLEAN read_only;
} Arena;

typedef struct {
    MemoryZone zones[3];
    Arena arenas[ARENA_COUNT];
    BOOLEAN initialized;
    UINTN zone_b_base;
    UINTN zone_b_size;
} ZoneConfig;

// API Functions - all simplified/disabled
EFI_STATUS zones_init(UINTN heap_base, UINTN heap_size);
BOOLEAN zones_is_in_zone(UINTN addr, MemoryZoneType zone);
BOOLEAN zones_is_range_in_zone(UINTN start, UINTN size, MemoryZoneType zone);
MemoryZoneType zones_get_zone(UINTN addr);
BOOLEAN zones_is_in_arena(UINTN addr, ArenaType arena);
void* zones_arena_alloc(ArenaType arena, UINTN size);
Arena* zones_get_arena(ArenaType arena);
void zones_set_arena_readonly(ArenaType arena, BOOLEAN readonly);
void zones_print_layout(void);
BOOLEAN zones_validate(void);

// Simplified implementations
EFI_STATUS zones_init(UINTN heap_base, UINTN heap_size) {
    return EFI_SUCCESS;
}

BOOLEAN zones_is_in_zone(UINTN addr, MemoryZoneType zone) {
    return TRUE;
}

BOOLEAN zones_is_range_in_zone(UINTN start, UINTN size, MemoryZoneType zone) {
    return TRUE;
}

MemoryZoneType zones_get_zone(UINTN addr) {
    return ZONE_B_SACRED;
}

BOOLEAN zones_is_in_arena(UINTN addr, ArenaType arena) {
    return TRUE;
}

void* zones_arena_alloc(ArenaType arena, UINTN size) {
    return NULL;
}

Arena* zones_get_arena(ArenaType arena) {
    return NULL;
}

void zones_set_arena_readonly(ArenaType arena, BOOLEAN readonly) {
    // No-op
}

void zones_print_layout(void) {
    // No-op
}

BOOLEAN zones_validate(void) {
    return TRUE;
}

#endif // MEMORY_ZONES_H
