/*
 * UIV - Unité d'Intention et de Valeurs
 * Pourquoi agir ? Hiérarchie d'objectifs et alignement
 */

#ifndef DRC_UIV_H
#define DRC_UIV_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// UIV - INTENTION AND VALUES
// ═══════════════════════════════════════════════════════════════

typedef enum {
    PRIORITY_CRITICAL,      // Safety, security
    PRIORITY_HIGH,          // User goals
    PRIORITY_MEDIUM,        // Optimization
    PRIORITY_LOW            // Nice to have
} ObjectivePriority;

typedef enum {
    VALUE_SAFETY,           // Ne pas nuire
    VALUE_TRUTHFULNESS,     // Vérité
    VALUE_HELPFULNESS,      // Utilité
    VALUE_RESPECT,          // Respect des règles
    VALUE_TRANSPARENCY      // Transparence
} CoreValue;

typedef struct {
    CHAR8 description[64];
    ObjectivePriority priority;
    BOOLEAN achieved;
    float completion;           // 0.0 - 1.0
} Objective;

typedef struct {
    CoreValue value;
    float weight;               // Importance (0.0 - 1.0)
    BOOLEAN violated;
} ValueConstraint;

typedef struct {
    Objective objectives[8];
    UINT32 objective_count;
    
    ValueConstraint values[5];
    UINT32 value_count;
    
    // Conflict resolution
    UINT32 conflicts_detected;
    UINT32 conflicts_resolved;
    
    // Alignment score
    float alignment_score;      // How well aligned with values
    BOOLEAN aligned;
} UIVContext;

// ═══════════════════════════════════════════════════════════════
// UIV FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UIV
 */
EFI_STATUS uiv_init(UIVContext* uiv);

/**
 * Add objective
 */
EFI_STATUS uiv_add_objective(UIVContext* uiv,
                             const CHAR8* description,
                             ObjectivePriority priority);

/**
 * Add value constraint
 */
EFI_STATUS uiv_add_value(UIVContext* uiv, CoreValue value, float weight);

/**
 * Check if action aligns with values
 */
BOOLEAN uiv_check_alignment(UIVContext* uiv, const CHAR8* action);

/**
 * Resolve conflict between objectives
 */
ObjectivePriority uiv_resolve_conflict(UIVContext* uiv,
                                        ObjectivePriority obj1,
                                        ObjectivePriority obj2);

/**
 * Calculate alignment score
 */
float uiv_calculate_alignment(UIVContext* uiv);

/**
 * Get highest priority objective
 */
Objective* uiv_get_top_objective(UIVContext* uiv);

/**
 * Print values report
 */
void uiv_print_report(UIVContext* uiv);

#endif // DRC_UIV_H
