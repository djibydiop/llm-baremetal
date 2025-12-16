/*
 * DRC Verification - Extended anti-hallucination checks
 * Graph analysis, circular dependency detection, type coherence
 */

#ifndef DRC_VERIFICATION_H
#define DRC_VERIFICATION_H

#include <efi.h>
#include <efilib.h>
#include "drc_urs.h"

// ═══════════════════════════════════════════════════════════════
// VERIFICATION STRUCTURES
// ═══════════════════════════════════════════════════════════════

#define MAX_NODES 64
#define MAX_EDGES 128

typedef enum {
    TYPE_NUMERIC,
    TYPE_SYMBOLIC,
    TYPE_GEOMETRIC,
    TYPE_LOGICAL,
    TYPE_UNKNOWN
} ReasoningType;

typedef struct {
    UINT32 id;
    ReasoningType type;
    CHAR8 label[64];
    float confidence;
    BOOLEAN visited;
} ReasoningNode;

typedef struct {
    UINT32 from_id;
    UINT32 to_id;
    CHAR8 relation[32];  // "requires", "implies", "contradicts"
    float weight;
} ReasoningEdge;

typedef struct {
    ReasoningNode nodes[MAX_NODES];
    UINT32 node_count;
    ReasoningEdge edges[MAX_EDGES];
    UINT32 edge_count;
    
    // Detection results
    BOOLEAN has_cycle;
    UINT32 cycle_nodes[MAX_NODES];
    UINT32 cycle_length;
    
    BOOLEAN has_contradiction;
    UINT32 contradiction_nodes[2];
    
    BOOLEAN has_type_mismatch;
    UINT32 mismatch_nodes[2];
} ReasoningGraph;

typedef struct {
    ReasoningGraph graph;
    
    // Verification flags
    BOOLEAN passed_cycle_check;
    BOOLEAN passed_type_check;
    BOOLEAN passed_contradiction_check;
    BOOLEAN passed_assumption_check;
    
    // Metrics
    float graph_coherence;      // 0.0 - 1.0
    float type_consistency;     // 0.0 - 1.0
    UINT32 total_checks;
    UINT32 failed_checks;
} VerificationContext;

// ═══════════════════════════════════════════════════════════════
// VERIFICATION FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize verification context
 */
EFI_STATUS verification_init(VerificationContext* ctx);

/**
 * Build reasoning graph from solution path
 */
EFI_STATUS verification_build_graph(VerificationContext* ctx, SolutionPath* path);

/**
 * Detect circular dependencies (A requires B requires A)
 */
EFI_STATUS verification_check_cycles(VerificationContext* ctx);

/**
 * Check type coherence (numeric step shouldn't follow geometric without bridge)
 */
EFI_STATUS verification_check_types(VerificationContext* ctx);

/**
 * Detect contradictory assumptions
 */
EFI_STATUS verification_check_contradictions(VerificationContext* ctx);

/**
 * Track assumption propagation through reasoning chain
 */
EFI_STATUS verification_track_assumptions(VerificationContext* ctx);

/**
 * Calculate overall coherence score
 */
float verification_calculate_coherence(VerificationContext* ctx);

/**
 * Print verification report
 */
void verification_print_report(VerificationContext* ctx);

/**
 * Full verification pipeline (all checks)
 */
EFI_STATUS verification_run_all(VerificationContext* ctx, SolutionPath* path);

#endif // DRC_VERIFICATION_H
