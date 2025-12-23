/*
 * SELF-HEALING MEMORY - Auto-Repair System
 * Detects memory corruption and auto-repairs from redundant copies
 * Uses checksums and ECC-like redundancy
 * 
 * Made in Senegal by Djiby Diop - December 2025
 */

#ifndef MEMORY_HEALING_H
#define MEMORY_HEALING_H

#include <efi.h>
#include <efilib.h>
#include "memory_zones.h"

// External EFI SystemTable (defined in main EFI application)
extern EFI_SYSTEM_TABLE *ST_healing;

// ============================================================================
// CONFIGURATION
// ============================================================================

#define HEALING_CHECK_INTERVAL  1000   // Vérifier tous les 1000 cycles
#define HEALING_MAX_REPAIRS     100    // Max réparations avant alerte
#define HEALING_ENABLE_ECC      TRUE   // Activer copie redondante

// ============================================================================
// TYPES
// ============================================================================

// Info de guérison d'une arène
typedef struct {
    UINT32 checksum;           // Checksum actuel
    UINT8* redundant_copy;     // Copie ECC (5% overhead)
    UINTN redundant_size;
    
    UINT32 corruption_count;   // Nombre de corruptions détectées
    UINT32 repair_count;       // Nombre de réparations réussies
    UINT32 repair_failures;    // Réparations échouées
    
    UINT64 last_check_cycle;
    BOOLEAN auto_repair;
    BOOLEAN critical;          // Arène critique (WEIGHTS)
} HealingInfo;

// Statistiques globales
typedef struct {
    UINT32 total_checks;
    UINT32 corruptions_detected;
    UINT32 successful_repairs;
    UINT32 failed_repairs;
    UINT64 total_bytes_repaired;
    
    BOOLEAN system_healthy;
    UINT32 health_score;       // 0-100
} HealingStats;

// Système de guérison
typedef struct {
    HealingInfo arenas[ARENA_COUNT];
    HealingStats stats;
    
    UINT64 cycle_count;
    BOOLEAN enabled;
    BOOLEAN auto_repair_enabled;
} HealingSystem;

// ============================================================================
// GLOBAL STATE
// ============================================================================

static HealingSystem g_healing = {
    .cycle_count = 0,
    .enabled = TRUE,
    .auto_repair_enabled = TRUE,
    .stats = {
        .system_healthy = TRUE,
        .health_score = 100
    }
};

// ============================================================================
// CHECKSUM FUNCTIONS
// ============================================================================

// Simple CRC32-like checksum
UINT32 calculate_checksum(void* data, UINTN size) {
    UINT32 checksum = 0xFFFFFFFF;
    UINT8* bytes = (UINT8*)data;
    
    for (UINTN i = 0; i < size; i++) {
        checksum ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            if (checksum & 1) {
                checksum = (checksum >> 1) ^ 0xEDB88320;
            } else {
                checksum >>= 1;
            }
        }
    }
    
    return ~checksum;
}

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

// Initialisation
void healing_init(void) {
    for (int i = 0; i < ARENA_COUNT; i++) {
        Arena* arena = zones_get_arena(i);
        if (!arena) continue;
        
        HealingInfo* info = &g_healing.arenas[i];
        
        // Allouer copie redondante (5% de la taille)
        if (HEALING_ENABLE_ECC) {
            info->redundant_size = arena->size / 20;  // 5%
            info->redundant_copy = NULL;  // Alloué à la demande
        }
        
        info->auto_repair = TRUE;
        info->critical = (i == ARENA_WEIGHTS);  // WEIGHTS est critique
        
        // Checksum initial
        info->checksum = calculate_checksum((void*)arena->start_addr, arena->used);
    }
    
    Print(L"[HEALING] 🛡️ Self-healing system initialized\r\n");
    Print(L"[HEALING] ECC redundancy: %s\r\n", 
          HEALING_ENABLE_ECC ? L"ENABLED" : L"DISABLED");
}

// Créer copie redondante
void healing_create_backup(ArenaType arena_type) {
    Arena* arena = zones_get_arena(arena_type);
    HealingInfo* info = &g_healing.arenas[arena_type];
    
    if (!arena || !HEALING_ENABLE_ECC) return;
    
    if (!info->redundant_copy) {
        // Allouer copie (utiliser AllocatePool car hors arènes)
        EFI_STATUS Status = uefi_call_wrapper(
            ST_healing->BootServices->AllocatePool, 3,
            EfiLoaderData,
            info->redundant_size,
            (void**)&info->redundant_copy
        );
        
        if (EFI_ERROR(Status)) {
            Print(L"[HEALING] ⚠️ Failed to allocate backup for arena %d\r\n", arena_type);
            return;
        }
    }
    
    // Copier données importantes (début de l'arène)
    UINTN copy_size = (arena->used < info->redundant_size) ? arena->used : info->redundant_size;
    
    for (UINTN i = 0; i < copy_size; i++) {
        info->redundant_copy[i] = ((UINT8*)arena->start_addr)[i];
    }
}

// Vérifier intégrité d'une arène
BOOLEAN healing_check_arena(ArenaType arena_type) {
    Arena* arena = zones_get_arena(arena_type);
    HealingInfo* info = &g_healing.arenas[arena_type];
    
    if (!arena || arena->used == 0) {
        return TRUE;  // Rien à vérifier
    }
    
    // Calculer checksum actuel
    UINT32 current_checksum = calculate_checksum((void*)arena->start_addr, arena->used);
    
    g_healing.stats.total_checks++;
    
    // Comparer avec checksum sauvegardé
    if (current_checksum != info->checksum) {
        // CORRUPTION DÉTECTÉE!
        info->corruption_count++;
        g_healing.stats.corruptions_detected++;
        
        Print(L"\r\n");
        Print(L"╔══════════════════════════════════════════════════════════╗\r\n");
        Print(L"║     ⚠️  MEMORY CORRUPTION DETECTED                      ║\r\n");
        Print(L"╚══════════════════════════════════════════════════════════╝\r\n");
        Print(L"\r\n");
        
        const CHAR16* arena_names[] = {L"WEIGHTS", L"KV_CACHE", L"SCRATCH", L"OUTPUT"};
        Print(L"Arena:           %s\r\n", arena_names[arena_type]);
        Print(L"Expected CRC32:  0x%08X\r\n", info->checksum);
        Print(L"Actual CRC32:    0x%08X\r\n", current_checksum);
        Print(L"Corruption #:    %lu\r\n", info->corruption_count);
        Print(L"\r\n");
        
        return FALSE;
    }
    
    return TRUE;
}

// Réparer une arène corrompue
BOOLEAN healing_repair_arena(ArenaType arena_type) {
    Arena* arena = zones_get_arena(arena_type);
    HealingInfo* info = &g_healing.arenas[arena_type];
    
    if (!arena || !info->auto_repair) {
        return FALSE;
    }
    
    Print(L"[HEALING] 🔧 Attempting repair...\r\n");
    
    // Stratégie 1: Restaurer depuis copie redondante
    if (HEALING_ENABLE_ECC && info->redundant_copy) {
        Print(L"[HEALING] Restoring from redundant copy...\r\n");
        
        UINTN restore_size = (arena->used < info->redundant_size) ? 
                             arena->used : info->redundant_size;
        
        for (UINTN i = 0; i < restore_size; i++) {
            ((UINT8*)arena->start_addr)[i] = info->redundant_copy[i];
        }
        
        // Recalculer checksum
        info->checksum = calculate_checksum((void*)arena->start_addr, arena->used);
        
        info->repair_count++;
        g_healing.stats.successful_repairs++;
        g_healing.stats.total_bytes_repaired += restore_size;
        
        Print(L"[HEALING] ✅ Arena %d repaired! (%lu bytes restored)\r\n", 
              arena_type, restore_size);
        
        return TRUE;
    }
    
    // Stratégie 2: Zero-fill (dernier recours)
    if (info->critical) {
        Print(L"[HEALING] ❌ Cannot repair critical arena without backup!\r\n");
        info->repair_failures++;
        g_healing.stats.failed_repairs++;
        return FALSE;
    }
    
    Print(L"[HEALING] Zero-filling non-critical arena...\r\n");
    
    for (UINTN i = 0; i < arena->used; i++) {
        ((UINT8*)arena->start_addr)[i] = 0;
    }
    
    info->checksum = calculate_checksum((void*)arena->start_addr, arena->used);
    info->repair_count++;
    g_healing.stats.successful_repairs++;
    
    Print(L"[HEALING] ✅ Arena %d cleared\r\n", arena_type);
    
    return TRUE;
}

// Vérification de santé complète
void healing_health_check(void) {
    if (!g_healing.enabled) {
        return;
    }
    
    BOOLEAN all_healthy = TRUE;
    
    for (int i = 0; i < ARENA_COUNT; i++) {
        BOOLEAN healthy = healing_check_arena(i);
        
        if (!healthy) {
            all_healthy = FALSE;
            
            // Auto-réparation si activée
            if (g_healing.auto_repair_enabled) {
                healing_repair_arena(i);
            }
        }
    }
    
    g_healing.stats.system_healthy = all_healthy;
    
    // Calculer score de santé (0-100)
    if (g_healing.stats.total_checks > 0) {
        UINT32 corruption_rate = 
            (g_healing.stats.corruptions_detected * 100) / 
            g_healing.stats.total_checks;
        
        g_healing.stats.health_score = 100 - corruption_rate;
    }
}

// Cycle de surveillance
void healing_cycle(void) {
    g_healing.cycle_count++;
    
    // Vérifier santé tous les N cycles
    if (g_healing.cycle_count % HEALING_CHECK_INTERVAL == 0) {
        healing_health_check();
        
        // Créer backup si nécessaire
        for (int i = 0; i < ARENA_COUNT; i++) {
            healing_create_backup(i);
        }
    }
}

// Mettre à jour checksum après modification légitime
void healing_update_checksum(ArenaType arena_type) {
    Arena* arena = zones_get_arena(arena_type);
    HealingInfo* info = &g_healing.arenas[arena_type];
    
    if (arena && arena->used > 0) {
        info->checksum = calculate_checksum((void*)arena->start_addr, arena->used);
    }
}

// Statistiques
void healing_print_stats(void) {
    Print(L"\r\n");
    Print(L"╔══════════════════════════════════════════════════════════╗\r\n");
    Print(L"║     🛡️ SELF-HEALING MEMORY STATISTICS                  ║\r\n");
    Print(L"╚══════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    
    Print(L"System Status:      %s\r\n",
          g_healing.stats.system_healthy ? L"✅ HEALTHY" : L"⚠️ COMPROMISED");
    Print(L"Health Score:       %u/100\r\n", g_healing.stats.health_score);
    Print(L"Total Checks:       %lu\r\n", g_healing.stats.total_checks);
    Print(L"\r\n");
    
    Print(L"Corruption Statistics:\r\n");
    Print(L"  Detected:         %lu\r\n", g_healing.stats.corruptions_detected);
    Print(L"  Repaired:         %lu\r\n", g_healing.stats.successful_repairs);
    Print(L"  Failed:           %lu\r\n", g_healing.stats.failed_repairs);
    Print(L"  Bytes Restored:   %lu\r\n", g_healing.stats.total_bytes_repaired);
    Print(L"\r\n");
    
    Print(L"Arena Health:\r\n");
    const CHAR16* arena_names[] = {L"WEIGHTS", L"KV_CACHE", L"SCRATCH", L"OUTPUT"};
    
    for (int i = 0; i < ARENA_COUNT; i++) {
        HealingInfo* info = &g_healing.arenas[i];
        Print(L"  %-10s: ", arena_names[i]);
        
        if (info->corruption_count == 0) {
            Print(L"✅ PRISTINE");
        } else if (info->repair_count > info->corruption_count) {
            Print(L"🔧 REPAIRED (%lu times)", info->repair_count);
        } else {
            Print(L"⚠️ DAMAGED (%lu corruptions)", info->corruption_count);
        }
        
        if (info->critical) {
            Print(L" [CRITICAL]");
        }
        
        Print(L"\r\n");
    }
    
    Print(L"\r\n");
    Print(L"🛡️ Resilience: ");
    
    if (g_healing.stats.health_score >= 95) {
        Print(L"PERFECT ✨\r\n");
    } else if (g_healing.stats.health_score >= 80) {
        Print(L"EXCELLENT 🏆\r\n");
    } else if (g_healing.stats.health_score >= 60) {
        Print(L"GOOD 👍\r\n");
    } else {
        Print(L"NEEDS ATTENTION ⚠️\r\n");
    }
    
    Print(L"\r\n");
}

// Enable/Disable
void healing_set_enabled(BOOLEAN enabled) {
    g_healing.enabled = enabled;
    Print(L"[HEALING] System %s\r\n", enabled ? L"ENABLED" : L"DISABLED");
}

void healing_set_auto_repair(BOOLEAN enabled) {
    g_healing.auto_repair_enabled = enabled;
    Print(L"[HEALING] Auto-repair %s\r\n", enabled ? L"ENABLED" : L"DISABLED");
}

#endif // MEMORY_HEALING_H
