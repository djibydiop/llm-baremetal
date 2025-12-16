/*
 * UIC - Unité d'Incohérence Cognitive
 * Implementation
 */

#include "drc_uic.h"
#include "drc_verification.h"

// ═══════════════════════════════════════════════════════════════
// UIC IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

/**
 * Helper: Copy string safely
 */
static void uic_strcpy(CHAR8* dest, const CHAR8* src, UINT32 max_len) {
    UINT32 i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * Initialize UIC context
 */
EFI_STATUS uic_init(UICContext* uic) {
    if (!uic) return EFI_INVALID_PARAMETER;
    
    uic->detection_count = 0;
    uic->total_checks = 0;
    uic->contradictions_found = 0;
    uic->temporal_violations = 0;
    uic->circular_deps = 0;
    
    uic->sensitivity = 0.7f;
    uic->strict_mode = FALSE;
    
    return EFI_SUCCESS;
}

/**
 * Add detection to UIC context
 */
static EFI_STATUS uic_add_detection(UICContext* uic,
                                     IncoherenceType type,
                                     IncoherenceSeverity severity,
                                     UINT32 node_id,
                                     const CHAR8* reason,
                                     float confidence) {
    if (uic->detection_count >= 16) return EFI_OUT_OF_RESOURCES;
    
    IncoherenceDetection* det = &uic->detections[uic->detection_count];
    det->type = type;
    det->severity = severity;
    det->node_id = node_id;
    uic_strcpy(det->reason, reason, 128);
    det->confidence = confidence;
    det->blocking = (severity >= SEVERITY_MEDIUM && uic->strict_mode) ||
                    (severity >= SEVERITY_HIGH);
    
    uic->detection_count++;
    return EFI_SUCCESS;
}

/**
 * Analyze solution path for incoherences
 */
EFI_STATUS uic_analyze_path(UICContext* uic, SolutionPath* path) {
    if (!uic || !path) return EFI_INVALID_PARAMETER;
    
    uic->total_checks++;
    uic->detection_count = 0;  // Reset detections
    
    // Check 1: Vérifier que toutes les étapes sont stables
    for (UINT32 i = 0; i < path->step_count; i++) {
        if (!path->steps[i].stable) {
            uic_add_detection(uic, INCOHERENCE_IMPLICIT, SEVERITY_LOW,
                            i, "Step marked as unstable", 0.9f);
        }
    }
    
    // Check 2: Vérifier cohérence des types d'hypothèses
    for (UINT32 i = 1; i < path->step_count; i++) {
        HypothesisType prev = path->steps[i-1].type;
        HypothesisType curr = path->steps[i].type;
        
        // Détection de sauts logiques suspects
        if (prev == HYPO_FACTORIZATION && curr == HYPO_GEOMETRIC) {
            uic_add_detection(uic, INCOHERENCE_LOGICAL_JUMP, SEVERITY_MEDIUM,
                            i, "Jump from factorization to geometric", 0.7f);
        }
    }
    
    // Check 3: Vérifier score global
    if (path->score < 0.3f) {
        uic_add_detection(uic, INCOHERENCE_IMPLICIT, SEVERITY_HIGH,
                        0, "Overall path score too low", 0.95f);
    }
    
    // Check 4: Vérifier validité
    if (!path->valid) {
        uic_add_detection(uic, INCOHERENCE_CONTRADICTION, SEVERITY_CRITICAL,
                        0, "Path marked as invalid", 1.0f);
    }
    
    return EFI_SUCCESS;
}

/**
 * Check reasoning graph for contradictions
 */
EFI_STATUS uic_check_contradictions(UICContext* uic, void* graph_ptr) {
    if (!uic || !graph_ptr) return EFI_INVALID_PARAMETER;
    
    ReasoningGraph* graph = (ReasoningGraph*)graph_ptr;
    
    // Chercher des relations "contradicts"
    for (UINT32 i = 0; i < graph->edge_count; i++) {
        ReasoningEdge* edge = &graph->edges[i];
        
        // Simple string comparison for "contradicts"
        BOOLEAN is_contradiction = FALSE;
        const char* rel = (const char*)edge->relation;
        const char* target = "contradicts";
        UINT32 j = 0;
        while (target[j] != '\0' && rel[j] == target[j]) j++;
        if (target[j] == '\0') is_contradiction = TRUE;
        
        if (is_contradiction) {
            uic->contradictions_found++;
            uic_add_detection(uic, INCOHERENCE_CONTRADICTION, SEVERITY_HIGH,
                            edge->from_id, "Contradiction detected in graph", 0.9f);
        }
    }
    
    return EFI_SUCCESS;
}

/**
 * Detect circular dependencies
 */
EFI_STATUS uic_detect_cycles(UICContext* uic, void* graph_ptr) {
    if (!uic || !graph_ptr) return EFI_INVALID_PARAMETER;
    
    ReasoningGraph* graph = (ReasoningGraph*)graph_ptr;
    
    if (graph->has_cycle) {
        uic->circular_deps++;
        uic_add_detection(uic, INCOHERENCE_CIRCULAR, SEVERITY_HIGH,
                        0, "Circular dependency in reasoning", 0.95f);
    }
    
    return EFI_SUCCESS;
}

/**
 * Verify temporal coherence
 */
EFI_STATUS uic_check_temporal(UICContext* uic, void* graph_ptr) {
    if (!uic || !graph_ptr) return EFI_INVALID_PARAMETER;
    
    ReasoningGraph* graph = (ReasoningGraph*)graph_ptr;
    
    // Chercher des relations causales inversées
    for (UINT32 i = 0; i < graph->edge_count; i++) {
        ReasoningEdge* edge = &graph->edges[i];
        
        // Check if "requires" relation goes backward in node order
        if (edge->from_id > edge->to_id) {
            const char* rel = (const char*)edge->relation;
            const char* target = "requires";
            UINT32 j = 0;
            while (target[j] != '\0' && rel[j] == target[j]) j++;
            if (target[j] == '\0') {
                uic->temporal_violations++;
                uic_add_detection(uic, INCOHERENCE_TEMPORAL, SEVERITY_MEDIUM,
                                edge->from_id, "Temporal order violated", 0.8f);
            }
        }
    }
    
    return EFI_SUCCESS;
}

/**
 * Check if blocking incoherence exists
 */
BOOLEAN uic_should_block(UICContext* uic) {
    if (!uic) return FALSE;
    
    for (UINT32 i = 0; i < uic->detection_count; i++) {
        if (uic->detections[i].blocking) {
            return TRUE;
        }
    }
    
    return FALSE;
}

/**
 * Get most severe incoherence
 */
IncoherenceDetection* uic_get_worst(UICContext* uic) {
    if (!uic || uic->detection_count == 0) return NULL;
    
    IncoherenceDetection* worst = &uic->detections[0];
    for (UINT32 i = 1; i < uic->detection_count; i++) {
        if (uic->detections[i].severity > worst->severity) {
            worst = &uic->detections[i];
        }
    }
    
    return worst;
}

/**
 * Print UIC report
 */
void uic_print_report(UICContext* uic) {
    if (!uic) return;
    
    Print(L"\n[UIC] Incoherence Detection Report\n");
    Print(L"  Total checks: %u\n", uic->total_checks);
    Print(L"  Detections: %u\n", uic->detection_count);
    
    if (uic->detection_count > 0) {
        Print(L"  ⚠ Issues found:\n");
        for (UINT32 i = 0; i < uic->detection_count; i++) {
            IncoherenceDetection* det = &uic->detections[i];
            const CHAR16* severity_str = L"?";
            switch (det->severity) {
                case SEVERITY_LOW: severity_str = L"LOW"; break;
                case SEVERITY_MEDIUM: severity_str = L"MED"; break;
                case SEVERITY_HIGH: severity_str = L"HIGH"; break;
                case SEVERITY_CRITICAL: severity_str = L"CRIT"; break;
            }
            Print(L"    [%s] Node %u: %a (conf=%.2f)%s\n",
                  severity_str, det->node_id, det->reason, det->confidence,
                  det->blocking ? L" [BLOCKING]" : L"");
        }
    }
    
    Print(L"  Contradictions: %u\n", uic->contradictions_found);
    Print(L"  Temporal violations: %u\n", uic->temporal_violations);
    Print(L"  Circular dependencies: %u\n", uic->circular_deps);
    
    if (uic_should_block(uic)) {
        Print(L"  ⛔ OUTPUT BLOCKED\n");
    } else {
        Print(L"  ✓ Output allowed\n");
    }
}
