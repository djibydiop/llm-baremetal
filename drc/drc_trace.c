/*
 * DRC Decision Trace Implementation
 */

#include "drc_trace.h"
#include "drc_perf.h"

// ═══════════════════════════════════════════════════════════════
// TRACE IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

/**
 * Helper: String copy
 */
static void trace_strcpy(CHAR8* dest, const CHAR8* src, UINT32 max_len) {
    UINT32 i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * Initialize trace system
 */
EFI_STATUS trace_init(DRCTraceContext* trace) {
    if (!trace) return EFI_INVALID_PARAMETER;
    
    trace->trace_count = 0;
    trace->write_index = 0;
    trace->total_decisions = 0;
    trace->accepted_count = 0;
    trace->warned_count = 0;
    trace->refused_count = 0;
    trace->resampled_count = 0;
    trace->enable_tracing = TRUE;
    trace->max_traces = 256;
    
    return EFI_SUCCESS;
}

/**
 * Add decision to trace (circular buffer)
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
                               const CHAR8* reason) {
    if (!trace || !trace->enable_tracing) return EFI_SUCCESS;
    
    DecisionTrace* t = &trace->traces[trace->write_index];
    
    t->token_pos = token_pos;
    t->token_id = token_id;
    t->reasoning_mode = reasoning_mode;
    t->urs_score = urs_score;
    t->uic_blocked = uic_blocked;
    t->uic_detections = uic_detections;
    t->ucr_risk = ucr_risk;
    t->ucr_safe = ucr_safe;
    t->uco_survived = uco_survived;
    t->uco_robustness = uco_robustness;
    t->decision = decision;
    trace_strcpy(t->decision_reason, reason, 128);
    t->timestamp_us = perf_get_timestamp_us();
    
    // Update circular buffer
    trace->write_index = (trace->write_index + 1) % trace->max_traces;
    if (trace->trace_count < trace->max_traces) {
        trace->trace_count++;
    }
    
    // Update statistics
    trace->total_decisions++;
    switch (decision) {
        case DECISION_ACCEPTED: trace->accepted_count++; break;
        case DECISION_WARNED: trace->warned_count++; break;
        case DECISION_REFUSED: trace->refused_count++; break;
        case DECISION_RESAMPLED: trace->resampled_count++; break;
    }
    
    return EFI_SUCCESS;
}

/**
 * Get last N decisions
 */
UINT32 trace_get_recent(DRCTraceContext* trace, DecisionTrace* out_traces, UINT32 count) {
    if (!trace || !out_traces || count == 0) return 0;
    
    UINT32 available = trace->trace_count < count ? trace->trace_count : count;
    
    for (UINT32 i = 0; i < available; i++) {
        UINT32 idx = (trace->write_index - 1 - i + trace->max_traces) % trace->max_traces;
        out_traces[i] = trace->traces[idx];
    }
    
    return available;
}

/**
 * Get decision by token position
 */
DecisionTrace* trace_get_by_pos(DRCTraceContext* trace, UINT32 token_pos) {
    if (!trace) return NULL;
    
    for (UINT32 i = 0; i < trace->trace_count; i++) {
        if (trace->traces[i].token_pos == token_pos) {
            return &trace->traces[i];
        }
    }
    
    return NULL;
}

/**
 * Print trace summary
 */
void trace_print_summary(DRCTraceContext* trace) {
    if (!trace) return;
    
    Print(L"\n╔══════════════════════════════════════════════════════╗\n");
    Print(L"║        DRC Decision Trace Summary                    ║\n");
    Print(L"╚══════════════════════════════════════════════════════╝\n");
    
    Print(L"\n[Statistics]\n");
    Print(L"  Total decisions: %u\n", trace->total_decisions);
    Print(L"  Accepted: %u (%.1f%%)\n", trace->accepted_count,
          trace->total_decisions > 0 ? (float)trace->accepted_count / trace->total_decisions * 100.0f : 0.0f);
    Print(L"  Warned: %u (%.1f%%)\n", trace->warned_count,
          trace->total_decisions > 0 ? (float)trace->warned_count / trace->total_decisions * 100.0f : 0.0f);
    Print(L"  Refused: %u (%.1f%%)\n", trace->refused_count,
          trace->total_decisions > 0 ? (float)trace->refused_count / trace->total_decisions * 100.0f : 0.0f);
    Print(L"  Resampled: %u (%.1f%%)\n", trace->resampled_count,
          trace->total_decisions > 0 ? (float)trace->resampled_count / trace->total_decisions * 100.0f : 0.0f);
    
    Print(L"\n[Buffer Status]\n");
    Print(L"  Traces stored: %u / %u\n", trace->trace_count, trace->max_traces);
    Print(L"  Tracing: %s\n", trace->enable_tracing ? L"ENABLED" : L"DISABLED");
}

/**
 * Print detailed trace
 */
void trace_print_detailed(DRCTraceContext* trace, UINT32 count) {
    if (!trace) return;
    
    Print(L"\n[Recent Decisions (last %u)]\n", count);
    
    UINT32 to_print = count < trace->trace_count ? count : trace->trace_count;
    
    for (UINT32 i = 0; i < to_print && i < 10; i++) {  // Max 10 for readability
        UINT32 idx = (trace->write_index - 1 - i + trace->max_traces) % trace->max_traces;
        DecisionTrace* t = &trace->traces[idx];
        
        const CHAR16* decision_str = L"?";
        switch (t->decision) {
            case DECISION_ACCEPTED: decision_str = L"ACCEPT"; break;
            case DECISION_WARNED: decision_str = L"WARN"; break;
            case DECISION_REFUSED: decision_str = L"REFUSE"; break;
            case DECISION_RESAMPLED: decision_str = L"RESAMPLE"; break;
        }
        
        Print(L"\n  [#%u] Token %u at pos %u - %s\n", 
              trace->total_decisions - i, t->token_id, t->token_pos, decision_str);
        Print(L"    URS score: %.2f, Mode: %u\n", t->urs_score, t->reasoning_mode);
        Print(L"    UIC: %s (%u detections)\n", t->uic_blocked ? L"BLOCKED" : L"OK", t->uic_detections);
        Print(L"    UCR: Risk %u, Safe: %s\n", t->ucr_risk, t->ucr_safe ? L"YES" : L"NO");
        Print(L"    UCO: Survived: %s, Robustness: %.2f\n", 
              t->uco_survived ? L"YES" : L"NO", t->uco_robustness);
        Print(L"    Reason: %a\n", t->decision_reason);
    }
}

/**
 * Export trace to buffer
 */
void trace_export(DRCTraceContext* trace, CHAR8* buffer, UINT32 buffer_size) {
    if (!trace || !buffer || buffer_size == 0) return;
    
    UINT32 pos = 0;
    const CHAR8* header = "DRC_TRACE:";
    for (UINT32 i = 0; header[i] != '\0' && pos < buffer_size - 1; i++) {
        buffer[pos++] = header[i];
    }
    buffer[pos] = '\0';
    
    // Real implementation would format as JSON or CSV
}
