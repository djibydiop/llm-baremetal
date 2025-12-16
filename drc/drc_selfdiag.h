/*
 * DRC Self-Diagnosis System
 * Monitors internal health and detects malfunctions
 */

#ifndef DRC_SELFDIAG_H
#define DRC_SELFDIAG_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// HEALTH STATUS
// ═══════════════════════════════════════════════════════════════

typedef enum {
    HEALTH_HEALTHY = 0,     // All systems nominal
    HEALTH_DEGRADED,        // Some issues but functional
    HEALTH_CRITICAL,        // Major issues detected
    HEALTH_FAILED           // System failure
} HealthStatus;

typedef enum {
    ISSUE_NONE = 0,
    ISSUE_INFINITE_LOOP,    // Detected infinite loop
    ISSUE_MEMORY_LEAK,      // Memory usage growing
    ISSUE_SLOW_RESPONSE,    // Response time too high
    ISSUE_CONTRADICTION,    // Internal contradictions
    ISSUE_STUCK_STATE,      // Stuck in same state
    ISSUE_RESOURCE_EXHAUSTED // Resources depleted
} IssueType;

// ═══════════════════════════════════════════════════════════════
// DIAGNOSTIC DATA
// ═══════════════════════════════════════════════════════════════

typedef struct {
    UINT32 urs_calls;
    UINT32 uic_detections;
    UINT32 ucr_blocks;
    UINT32 uti_events;
    UINT32 uco_attacks;
    UINT32 ums_facts;
    UINT32 uam_blocks;
    UINT32 upe_violations;
    UINT32 uiv_conflicts;
    
    UINT64 last_update_time;
    UINT32 updates_count;
} UnitActivitySnapshot;

typedef struct {
    IssueType type;
    HealthStatus severity;
    CHAR8 description[128];
    UINT64 detected_at;
    BOOLEAN auto_fixed;
    CHAR8 fix_description[64];
} DiagnosticIssue;

typedef struct {
    HealthStatus overall_health;
    
    // Activity monitoring
    UnitActivitySnapshot snapshots[16];  // Circular buffer
    UINT32 snapshot_index;
    
    // Issue tracking
    DiagnosticIssue issues[32];
    UINT32 issue_count;
    
    // Loop detection
    UINT32 same_state_count;
    UINT32 last_token_generated;
    
    // Performance thresholds
    UINT64 max_response_time_us;
    UINT32 max_same_state_iterations;
    
    // Statistics
    UINT32 total_checks;
    UINT32 issues_detected;
    UINT32 auto_fixes_applied;
    
    // Configuration
    BOOLEAN enable_auto_repair;
    BOOLEAN verbose_diagnostics;
} SelfDiagContext;

// ═══════════════════════════════════════════════════════════════
// SELF-DIAGNOSIS FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize self-diagnosis system
 */
EFI_STATUS selfdiag_init(SelfDiagContext* ctx);

/**
 * Take snapshot of current unit activity
 */
void selfdiag_snapshot(SelfDiagContext* ctx,
                       UINT32 urs_calls, UINT32 uic_detections,
                       UINT32 ucr_blocks, UINT32 uti_events,
                       UINT32 uco_attacks, UINT32 ums_facts,
                       UINT32 uam_blocks, UINT32 upe_violations,
                       UINT32 uiv_conflicts);

/**
 * Check for infinite loops
 */
BOOLEAN selfdiag_check_loops(SelfDiagContext* ctx);

/**
 * Check for stuck states
 */
BOOLEAN selfdiag_check_stuck(SelfDiagContext* ctx, UINT32 current_token);

/**
 * Check for contradictions between units
 */
BOOLEAN selfdiag_check_contradictions(SelfDiagContext* ctx,
                                      BOOLEAN uic_blocked,
                                      BOOLEAN ucr_safe,
                                      BOOLEAN uiv_aligned);

/**
 * Run full diagnostic check
 */
HealthStatus selfdiag_check_health(SelfDiagContext* ctx);

/**
 * Attempt auto-repair of detected issues
 */
BOOLEAN selfdiag_auto_repair(SelfDiagContext* ctx);

/**
 * Report issue
 */
void selfdiag_report_issue(SelfDiagContext* ctx,
                           IssueType type,
                           HealthStatus severity,
                           const CHAR8* description);

/**
 * Get current health status
 */
HealthStatus selfdiag_get_health(const SelfDiagContext* ctx);

/**
 * Print diagnostic report
 */
void selfdiag_print_report(const SelfDiagContext* ctx);

/**
 * Get last detected issue
 */
const DiagnosticIssue* selfdiag_get_last_issue(const SelfDiagContext* ctx);

#endif // DRC_SELFDIAG_H
