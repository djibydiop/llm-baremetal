/*
 * DRC Decision Trace System
 * Audit trail for explainability and certification
 */

#ifndef DRC_TRACE_H
#define DRC_TRACE_H

#include <efi.h>
#include <efilib.h>
#include "drc_ucr.h"

// ═══════════════════════════════════════════════════════════════
// DECISION TRACE (Audit Trail)
// ═══════════════════════════════════════════════════════════════

typedef enum {
    DECISION_ACCEPTED,
    DECISION_WARNED,
    DECISION_REFUSED,
    DECISION_RESAMPLED
} DecisionType;

typedef struct {
    UINT32 token_pos;               // Position in generation
    UINT32 token_id;                // Token generated
    
    // Reasoning context
    UINT32 reasoning_mode;          // URS mode used
    float urs_score;                // URS path score
    
    // Verification results
    BOOLEAN uic_blocked;            // UIC blocked output
    UINT32 uic_detections;          // Number of incoherences
    
    // Risk assessment
    RiskLevel ucr_risk;             // UCR risk level
    BOOLEAN ucr_safe;               // UCR safety flag
    
    // Counter-reasoning
    BOOLEAN uco_survived;           // Survived attacks
    float uco_robustness;           // Robustness score
    
    // Final decision
    DecisionType decision;
    CHAR8 decision_reason[128];
    
    // Timestamp
    UINT64 timestamp_us;
} DecisionTrace;

typedef struct {
    DecisionTrace traces[256];      // Circular buffer
    UINT32 trace_count;
    UINT32 write_index;             // Next write position
    
    // Statistics
    UINT32 total_decisions;
    UINT32 accepted_count;
    UINT32 warned_count;
    UINT32 refused_count;
    UINT32 resampled_count;
    
    // Configuration
    BOOLEAN enable_tracing;
    UINT32 max_traces;              // 256 by default
} DRCTraceContext;

// ═══════════════════════════════════════════════════════════════
// TRACE FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize trace system
 */
EFI_STATUS trace_init(DRCTraceContext* trace);

/**
 * Add decision to trace
 */
EFI_STATUS trace_add_decision(DRCTraceContext* trace,
                               UINT32 token_pos,
                               UINT32 token_id,
                               UINT32 reasoning_mode,
                               float urs_score,
                               BOOLEAN uic_blocked,
                               UINT32 uic_detections,
                               RiskLevel ucr_risk,
                               BOOLEAN ucr_safe,
                               BOOLEAN uco_survived,
                               float uco_robustness,
                               DecisionType decision,
                               const CHAR8* reason);

/**
 * Get last N decisions
 */
UINT32 trace_get_recent(DRCTraceContext* trace, DecisionTrace* out_traces, UINT32 count);

/**
 * Get decision by token position
 */
DecisionTrace* trace_get_by_pos(DRCTraceContext* trace, UINT32 token_pos);

/**
 * Print trace summary
 */
void trace_print_summary(DRCTraceContext* trace);

/**
 * Print detailed trace
 */
void trace_print_detailed(DRCTraceContext* trace, UINT32 count);

/**
 * Export trace to buffer (for external analysis)
 */
void trace_export(DRCTraceContext* trace, CHAR8* buffer, UINT32 buffer_size);

#endif // DRC_TRACE_H
