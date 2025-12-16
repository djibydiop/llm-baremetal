/*
 * UPE - Unité de Plausibilité Expérientielle
 * Vérifier si ça peut exister dans le monde réel
 */

#ifndef DRC_UPE_H
#define DRC_UPE_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// UPE - EXPERIENTIAL PLAUSIBILITY
// ═══════════════════════════════════════════════════════════════

typedef enum {
    PLAUSIBLE,              // Plausible dans le monde réel
    IMPLAUSIBLE,            // Très peu probable
    IMPOSSIBLE,             // Physiquement impossible
    UNKNOWN                 // Incapable de déterminer
} PlausibilityLevel;

typedef enum {
    VIOLATION_NONE,
    VIOLATION_PHYSICS,          // Lois physiques (énergie, vitesse)
    VIOLATION_CAUSALITY,        // Causalité temporelle
    VIOLATION_CONSERVATION,     // Conservation (masse, énergie)
    VIOLATION_SCALE,            // Échelle (trop petit/grand)
    VIOLATION_RESOURCE          // Ressources insuffisantes
} PhysicsViolation;

typedef struct {
    PlausibilityLevel level;
    PhysicsViolation violation;
    BOOLEAN feasible;
    float plausibility_score;       // 0.0 - 1.0
    CHAR8 reason[128];
} PlausibilityCheck;

typedef struct {
    PlausibilityCheck current;
    
    // Statistics
    UINT32 total_checks;
    UINT32 impossible_detected;
    UINT32 implausible_detected;
    UINT32 physics_violations;
    
    // Configuration
    BOOLEAN strict_physics;         // Strict physical laws
    float plausibility_threshold;   // 0.5 par défaut
} UPEContext;

// ═══════════════════════════════════════════════════════════════
// UPE FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UPE
 */
EFI_STATUS upe_init(UPEContext* upe);

/**
 * Check plausibility of statement
 */
PlausibilityLevel upe_check_plausibility(UPEContext* upe, const CHAR8* statement);

/**
 * Check for physics violations
 */
BOOLEAN upe_check_physics(UPEContext* upe, const CHAR8* statement);

/**
 * Check resource feasibility
 */
BOOLEAN upe_check_resources(UPEContext* upe, const CHAR8* statement);

/**
 * Check scale feasibility
 */
BOOLEAN upe_check_scale(UPEContext* upe, const CHAR8* statement);

/**
 * Get plausibility score
 */
float upe_get_score(UPEContext* upe);

/**
 * Print plausibility report
 */
void upe_print_report(UPEContext* upe);

#endif // DRC_UPE_H
