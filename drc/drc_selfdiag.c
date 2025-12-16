/*
 * DRC Self-Diagnosis System Implementation
 */

#include "drc_selfdiag.h"

// Simple string copy
static void str_copy(CHAR8* dest, const CHAR8* src, UINT32 max_len) {
    UINT32 i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Simple string length
static UINT32 str_len(const CHAR8* s) {
    UINT32 len = 0;
    while (s[len] != '\0') len++;
    return len;
}

/**
 * Initialize self-diagnosis system
 */
EFI_STATUS selfdiag_init(SelfDiagContext* ctx) {
    if (!ctx) return EFI_INVALID_PARAMETER;
    
    // Clear all fields
    for (UINT32 i = 0; i < sizeof(SelfDiagContext); i++) {
        ((UINT8*)ctx)[i] = 0;
    }
    
    ctx->overall_health = HEALTH_HEALTHY;
    ctx->max_response_time_us = 1000000;  // 1 second max
    ctx->max_same_state_iterations = 10;
    ctx->enable_auto_repair = TRUE;
    ctx->verbose_diagnostics = FALSE;
    
    return EFI_SUCCESS;
}

/**
 * Take snapshot of current unit activity
 */
void selfdiag_snapshot(SelfDiagContext* ctx,
                       UINT32 urs_calls, UINT32 uic_detections,
                       UINT32 ucr_blocks, UINT32 uti_events,
                       UINT32 uco_attacks, UINT32 ums_facts,
                       UINT32 uam_blocks, UINT32 upe_violations,
                       UINT32 uiv_conflicts) {
    if (!ctx) return;
    
    UnitActivitySnapshot* snap = &ctx->snapshots[ctx->snapshot_index];
    
    snap->urs_calls = urs_calls;
    snap->uic_detections = uic_detections;
    snap->ucr_blocks = ucr_blocks;
    snap->uti_events = uti_events;
    snap->uco_attacks = uco_attacks;
    snap->ums_facts = ums_facts;
    snap->uam_blocks = uam_blocks;
    snap->upe_violations = upe_violations;
    snap->uiv_conflicts = uiv_conflicts;
    snap->last_update_time = ctx->total_checks;
    snap->updates_count = ctx->total_checks;
    
    ctx->snapshot_index = (ctx->snapshot_index + 1) % 16;
}

/**
 * Check for infinite loops
 */
BOOLEAN selfdiag_check_loops(SelfDiagContext* ctx) {
    if (!ctx || ctx->snapshot_index < 3) return FALSE;
    
    // Compare last 3 snapshots
    UINT32 idx1 = (ctx->snapshot_index - 1 + 16) % 16;
    UINT32 idx2 = (ctx->snapshot_index - 2 + 16) % 16;
    UINT32 idx3 = (ctx->snapshot_index - 3 + 16) % 16;
    
    UnitActivitySnapshot* s1 = &ctx->snapshots[idx1];
    UnitActivitySnapshot* s2 = &ctx->snapshots[idx2];
    UnitActivitySnapshot* s3 = &ctx->snapshots[idx3];
    
    // If activity is identical for 3 iterations, might be stuck
    if (s1->urs_calls == s2->urs_calls && s2->urs_calls == s3->urs_calls &&
        s1->uic_detections == s2->uic_detections && s2->uic_detections == s3->uic_detections) {
        
        selfdiag_report_issue(ctx, ISSUE_INFINITE_LOOP, HEALTH_CRITICAL,
                             "Detected identical activity pattern for 3 iterations");
        return TRUE;
    }
    
    return FALSE;
}

/**
 * Check for stuck states
 */
BOOLEAN selfdiag_check_stuck(SelfDiagContext* ctx, UINT32 current_token) {
    if (!ctx) return FALSE;
    
    if (current_token == ctx->last_token_generated) {
        ctx->same_state_count++;
        
        if (ctx->same_state_count >= ctx->max_same_state_iterations) {
            selfdiag_report_issue(ctx, ISSUE_STUCK_STATE, HEALTH_CRITICAL,
                                 "System stuck generating same token");
            return TRUE;
        }
    } else {
        ctx->same_state_count = 0;
        ctx->last_token_generated = current_token;
    }
    
    return FALSE;
}

/**
 * Check for contradictions between units
 */
BOOLEAN selfdiag_check_contradictions(SelfDiagContext* ctx,
                                      BOOLEAN uic_blocked,
                                      BOOLEAN ucr_safe,
                                      BOOLEAN uiv_aligned) {
    if (!ctx) return FALSE;
    
    // UIC blocks but UCR says safe - contradiction
    if (uic_blocked && ucr_safe) {
        selfdiag_report_issue(ctx, ISSUE_CONTRADICTION, HEALTH_DEGRADED,
                             "UIC blocked but UCR marked safe");
        return TRUE;
    }
    
    // UCR unsafe but UIV aligned - contradiction
    if (!ucr_safe && uiv_aligned) {
        selfdiag_report_issue(ctx, ISSUE_CONTRADICTION, HEALTH_DEGRADED,
                             "UCR unsafe but UIV shows alignment");
        return TRUE;
    }
    
    return FALSE;
}

/**
 * Run full diagnostic check
 */
HealthStatus selfdiag_check_health(SelfDiagContext* ctx) {
    if (!ctx) return HEALTH_FAILED;
    
    ctx->total_checks++;
    
    // Check for loops
    if (selfdiag_check_loops(ctx)) {
        ctx->overall_health = HEALTH_CRITICAL;
        return HEALTH_CRITICAL;
    }
    
    // Check recent issues
    if (ctx->issue_count > 0) {
        DiagnosticIssue* last = &ctx->issues[ctx->issue_count - 1];
        
        if (last->severity == HEALTH_CRITICAL) {
            ctx->overall_health = HEALTH_CRITICAL;
            return HEALTH_CRITICAL;
        }
        
        if (last->severity == HEALTH_DEGRADED) {
            ctx->overall_health = HEALTH_DEGRADED;
            return HEALTH_DEGRADED;
        }
    }
    
    // All checks passed
    ctx->overall_health = HEALTH_HEALTHY;
    return HEALTH_HEALTHY;
}

/**
 * Attempt auto-repair of detected issues
 */
BOOLEAN selfdiag_auto_repair(SelfDiagContext* ctx) {
    if (!ctx || !ctx->enable_auto_repair || ctx->issue_count == 0) {
        return FALSE;
    }
    
    DiagnosticIssue* issue = &ctx->issues[ctx->issue_count - 1];
    
    switch (issue->type) {
        case ISSUE_STUCK_STATE:
            // Reset same state counter
            ctx->same_state_count = 0;
            issue->auto_fixed = TRUE;
            str_copy(issue->fix_description, "Reset state counter", 64);
            ctx->auto_fixes_applied++;
            return TRUE;
            
        case ISSUE_INFINITE_LOOP:
            // Clear snapshots to break loop detection
            for (UINT32 i = 0; i < 16; i++) {
                ctx->snapshots[i].urs_calls = 0;
            }
            issue->auto_fixed = TRUE;
            str_copy(issue->fix_description, "Cleared activity snapshots", 64);
            ctx->auto_fixes_applied++;
            return TRUE;
            
        default:
            return FALSE;
    }
}

/**
 * Report issue
 */
void selfdiag_report_issue(SelfDiagContext* ctx,
                           IssueType type,
                           HealthStatus severity,
                           const CHAR8* description) {
    if (!ctx || ctx->issue_count >= 32) return;
    
    DiagnosticIssue* issue = &ctx->issues[ctx->issue_count++];
    
    issue->type = type;
    issue->severity = severity;
    str_copy(issue->description, description, 128);
    issue->detected_at = ctx->total_checks;
    issue->auto_fixed = FALSE;
    
    ctx->issues_detected++;
    
    if (ctx->verbose_diagnostics) {
        Print(L"[SELFDIAG] Issue detected: %a\r\n", description);
    }
}

/**
 * Get current health status
 */
HealthStatus selfdiag_get_health(const SelfDiagContext* ctx) {
    if (!ctx) return HEALTH_FAILED;
    return ctx->overall_health;
}

/**
 * Print diagnostic report
 */
void selfdiag_print_report(const SelfDiagContext* ctx) {
    if (!ctx) return;
    
    Print(L"\r\n═══════════════════════════════════════════════════════════\r\n");
    Print(L"  SELF-DIAGNOSIS REPORT\r\n");
    Print(L"═══════════════════════════════════════════════════════════\r\n");
    
    // Health status
    const CHAR16* health_str = 
        ctx->overall_health == HEALTH_HEALTHY ? L"✓ HEALTHY" :
        ctx->overall_health == HEALTH_DEGRADED ? L"⚠ DEGRADED" :
        ctx->overall_health == HEALTH_CRITICAL ? L"✗ CRITICAL" : L"✗ FAILED";
    
    Print(L"  Overall Health: %s\r\n", health_str);
    Print(L"\r\n");
    
    // Statistics
    Print(L"  Total Checks:       %d\r\n", ctx->total_checks);
    Print(L"  Issues Detected:    %d\r\n", ctx->issues_detected);
    Print(L"  Auto-Fixes Applied: %d\r\n", ctx->auto_fixes_applied);
    Print(L"  Snapshots Taken:    %d\r\n", ctx->snapshot_index);
    Print(L"\r\n");
    
    // Recent issues
    if (ctx->issue_count > 0) {
        Print(L"  Recent Issues (%d):\r\n", ctx->issue_count);
        
        UINT32 start = ctx->issue_count > 5 ? ctx->issue_count - 5 : 0;
        for (UINT32 i = start; i < ctx->issue_count; i++) {
            const DiagnosticIssue* issue = &ctx->issues[i];
            
            const CHAR8* type_str = 
                issue->type == ISSUE_INFINITE_LOOP ? "LOOP" :
                issue->type == ISSUE_STUCK_STATE ? "STUCK" :
                issue->type == ISSUE_CONTRADICTION ? "CONTRADICTION" :
                issue->type == ISSUE_SLOW_RESPONSE ? "SLOW" : "OTHER";
            
            Print(L"    [%a] %a", type_str, issue->description);
            
            if (issue->auto_fixed) {
                Print(L" (✓ Fixed: %a)", issue->fix_description);
            }
            Print(L"\r\n");
        }
    } else {
        Print(L"  No issues detected\r\n");
    }
    
    Print(L"═══════════════════════════════════════════════════════════\r\n");
}

/**
 * Get last detected issue
 */
const DiagnosticIssue* selfdiag_get_last_issue(const SelfDiagContext* ctx) {
    if (!ctx || ctx->issue_count == 0) return NULL;
    return &ctx->issues[ctx->issue_count - 1];
}
