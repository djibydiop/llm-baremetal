/*
 * DRC Time Budget System
 * Adaptive computation time management
 */

#ifndef DRC_TIMEBUDGET_H
#define DRC_TIMEBUDGET_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// TIME BUDGET CONFIGURATION
// ═══════════════════════════════════════════════════════════════

#define MAX_BUDGET_ITEMS        32
#define MAX_BUDGET_HISTORY      16

// Computation modes
typedef enum {
    TIMEBUDGET_FAST,        // Quick response, lower quality
    TIMEBUDGET_NORMAL,      // Balanced
    TIMEBUDGET_CAREFUL,     // Slower, higher quality
    TIMEBUDGET_THOROUGH     // Maximum quality, slowest
} ComputationMode;

// Budget allocation priority
typedef enum {
    BUDGET_PRIORITY_CRITICAL,  // Safety-critical (URS, Verification)
    BUDGET_PRIORITY_HIGH,      // Important (UCR, UIV, UAM)
    BUDGET_PRIORITY_MEDIUM,    // Standard (UIC, UTI, UCO)
    BUDGET_PRIORITY_LOW        // Optional (UMS, UPE)
} BudgetPriority;

// Budget status
typedef enum {
    BUDGET_OK,              // Within budget
    BUDGET_WARNING,         // Approaching limit (>75%)
    BUDGET_EXCEEDED,        // Over budget
    BUDGET_CRITICAL         // Severely over budget (>150%)
} BudgetStatus;

// ═══════════════════════════════════════════════════════════════
// STRUCTURES
// ═══════════════════════════════════════════════════════════════

// Budget allocation for a single unit/operation
typedef struct {
    CHAR8 name[64];
    BudgetPriority priority;
    UINT64 allocated_us;       // Allocated time in microseconds
    UINT64 consumed_us;        // Actually consumed
    UINT64 start_time;         // Start timestamp
    BOOLEAN is_active;
    UINT32 overruns;           // Number of times exceeded
    float efficiency;          // consumed / allocated ratio
} BudgetItem;

// Historical budget data
typedef struct {
    UINT64 timestamp;
    UINT64 total_budget_us;
    UINT64 total_consumed_us;
    BudgetStatus status;
    ComputationMode mode;
} BudgetHistory;

// Main time budget context
typedef struct {
    // Global settings
    ComputationMode mode;
    UINT64 per_token_budget_us;   // Total budget per token
    UINT64 total_budget_us;        // Total for entire inference
    BOOLEAN adaptive_enabled;
    BOOLEAN strict_enforcement;
    
    // Current state
    UINT64 current_token_start;
    UINT64 current_consumed_us;
    UINT64 total_consumed_us;
    UINT32 tokens_processed;
    BudgetStatus current_status;
    
    // Budget items
    BudgetItem items[MAX_BUDGET_ITEMS];
    UINT32 item_count;
    
    // History
    BudgetHistory history[MAX_BUDGET_HISTORY];
    UINT32 history_count;
    
    // Statistics
    UINT32 total_overruns;
    UINT32 adaptive_adjustments;
    float average_utilization;
    
} TimeBudgetContext;

// ═══════════════════════════════════════════════════════════════
// FUNCTION PROTOTYPES
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize time budget system
 */
EFI_STATUS timebudget_init(TimeBudgetContext* ctx, ComputationMode mode);

/**
 * Set computation mode
 */
void timebudget_set_mode(TimeBudgetContext* ctx, ComputationMode mode);

/**
 * Allocate budget for an operation
 */
EFI_STATUS timebudget_allocate(TimeBudgetContext* ctx,
                               const CHAR8* name,
                               BudgetPriority priority,
                               UINT64* allocated_us);

/**
 * Start timing an operation
 */
void timebudget_start(TimeBudgetContext* ctx, const CHAR8* name);

/**
 * End timing an operation
 */
BudgetStatus timebudget_end(TimeBudgetContext* ctx, const CHAR8* name);

/**
 * Check if budget allows more computation
 */
BOOLEAN timebudget_can_continue(const TimeBudgetContext* ctx);

/**
 * Get remaining budget for current token
 */
UINT64 timebudget_remaining(const TimeBudgetContext* ctx);

/**
 * Adaptive adjustment based on utilization
 */
void timebudget_adapt(TimeBudgetContext* ctx);

/**
 * Start new token budget
 */
void timebudget_new_token(TimeBudgetContext* ctx);

/**
 * Get current status
 */
BudgetStatus timebudget_get_status(const TimeBudgetContext* ctx);

/**
 * Print budget report
 */
void timebudget_print_report(const TimeBudgetContext* ctx);

/**
 * Get average utilization percentage
 */
float timebudget_get_utilization(const TimeBudgetContext* ctx);

#endif // DRC_TIMEBUDGET_H
