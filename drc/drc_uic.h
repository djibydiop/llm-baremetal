/*
 * UIC - Unité d'Incohérence Cognitive
 * Détecte quand "ça a l'air juste" mais ne l'est pas
 */

#ifndef DRC_UIC_H
#define DRC_UIC_H

#include <efi.h>
#include <efilib.h>
#include "drc_urs.h"

// ═══════════════════════════════════════════════════════════════
// UIC - INCOHERENCE DETECTION
// ═══════════════════════════════════════════════════════════════

typedef enum {
    INCOHERENCE_NONE,
    INCOHERENCE_CONTRADICTION,      // Affirmation A + ¬A
    INCOHERENCE_TEMPORAL,           // Événement impossible dans le temps
    INCOHERENCE_CAUSAL,             // Effet avant cause
    INCOHERENCE_CIRCULAR,           // A dépend de B dépend de A
    INCOHERENCE_IMPLICIT,           // Prémisse cachée invalide
    INCOHERENCE_LOGICAL_JUMP        // Saut logique non justifié
} IncoherenceType;

typedef enum {
    SEVERITY_LOW,       // Avertissement
    SEVERITY_MEDIUM,    // Bloquant sauf override
    SEVERITY_HIGH,      // Bloquant absolu
    SEVERITY_CRITICAL   // Arrêt système
} IncoherenceSeverity;

typedef struct {
    IncoherenceType type;
    IncoherenceSeverity severity;
    UINT32 node_id;                 // Nœud du graphe problématique
    CHAR8 reason[128];
    float confidence;               // 0.0 - 1.0
    BOOLEAN blocking;               // TRUE si doit bloquer output
} IncoherenceDetection;

typedef struct {
    IncoherenceDetection detections[16];
    UINT32 detection_count;
    
    // Statistiques
    UINT32 total_checks;
    UINT32 contradictions_found;
    UINT32 temporal_violations;
    UINT32 circular_deps;
    
    // Configuration
    float sensitivity;              // 0.5 = normal, 1.0 = paranoid
    BOOLEAN strict_mode;            // TRUE = bloquer sur MEDIUM
} UICContext;

// ═══════════════════════════════════════════════════════════════
// UIC FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UIC context
 */
EFI_STATUS uic_init(UICContext* uic);

/**
 * Analyze solution path for incoherences
 * Called AFTER URS but BEFORE output
 */
EFI_STATUS uic_analyze_path(UICContext* uic, SolutionPath* path);

/**
 * Check reasoning graph for contradictions
 */
EFI_STATUS uic_check_contradictions(UICContext* uic, void* graph);

/**
 * Detect circular dependencies
 */
EFI_STATUS uic_detect_cycles(UICContext* uic, void* graph);

/**
 * Verify temporal coherence (cause → effect)
 */
EFI_STATUS uic_check_temporal(UICContext* uic, void* graph);

/**
 * Check if blocking incoherence exists
 * Returns TRUE if output should be blocked
 */
BOOLEAN uic_should_block(UICContext* uic);

/**
 * Get most severe incoherence
 */
IncoherenceDetection* uic_get_worst(UICContext* uic);

/**
 * Print UIC report
 */
void uic_print_report(UICContext* uic);

#endif // DRC_UIC_H
