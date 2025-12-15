/*
 * DRC Time Budget Implementation
 */

#include "drc_timebudget.h"

// Budget presets (microseconds per token)
static const UINT64 MODE_BUDGETS[] = {
    50000,      // TIMEBUDGET_FAST: 50ms per token
    150000,     // TIMEBUDGET_NORMAL: 150ms per token
    300000,     // TIMEBUDGET_CAREFUL: 300ms per token
    500000      // TIMEBUDGET_THOROUGH: 500ms per token
};

// Priority weights (percentage of total budget)
static const float PRIORITY_WEIGHTS[] = {
    0.40f,      // CRITICAL: 40%
    0.30f,      // HIGH: 30%
    0.20f,      // MEDIUM: 20%
    0.10f       // LOW: 10%
};

/**
 * Get current time in microseconds
 */
static UINT64 get_time_us(void) {
    // Simple time tracking (would use UEFI timer in real implementation)
    static UINT64 fake_time = 0;
    fake_time += 1000;  // Increment by 1ms for testing
    return fake_time;
}

/**
 * Find budget item by name
 */
static BudgetItem* find_item(TimeBudgetContext* ctx, const CHAR8* name) {
    for (UINT32 i = 0; i < ctx->item_count; i++) {
        UINT32 j = 0;
        while (ctx->items[i].name[j] == name[j] && name[j] != '\0') j++;
        if (ctx->items[i].name[j] == name[j]) return &ctx->items[i];
    }
    return NULL;
}

/**
 * Initialize time budget system
 */
EFI_STATUS timebudget_init(TimeBudgetContext* ctx, ComputationMode mode) {
    if (!ctx) return EFI_INVALID_PARAMETER;
    
    // Clear
    for (UINT32 i = 0; i < sizeof(TimeBudgetContext); i++) {
        ((UINT8*)ctx)[i] = 0;
    }
    
    // Validate mode
    if (mode > TIMEBUDGET_THOROUGH) mode = TIMEBUDGET_NORMAL;
    
    ctx->mode = mode;
    ctx->per_token_budget_us = MODE_BUDGETS[mode];
    ctx->adaptive_enabled = TRUE;
    ctx->strict_enforcement = FALSE;  // Soft limits by default
    
    return EFI_SUCCESS;
}

/**
 * Set computation mode
 */
void timebudget_set_mode(TimeBudgetContext* ctx, ComputationMode mode) {
    if (!ctx || mode > TIMEBUDGET_THOROUGH) return;
    
    ctx->mode = mode;
    ctx->per_token_budget_us = MODE_BUDGETS[mode];
}

/**
 * Allocate budget for an operation
 */
EFI_STATUS timebudget_allocate(TimeBudgetContext* ctx,
                               const CHAR8* name,
                               BudgetPriority priority,
                               UINT64* allocated_us) {
    if (!ctx || !name || ctx->item_count >= MAX_BUDGET_ITEMS) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Calculate allocation based on priority
    UINT64 allocation = (UINT64)(ctx->per_token_budget_us * PRIORITY_WEIGHTS[priority]);
    
    // Create new budget item
    BudgetItem* item = &ctx->items[ctx->item_count++];
    
    // Copy name
    UINT32 i = 0;
    while (i < 63 && name[i] != '\0') {
        item->name[i] = name[i];
        i++;
    }
    item->name[i] = '\0';
    
    item->priority = priority;
    item->allocated_us = allocation;
    item->consumed_us = 0;
    item->is_active = FALSE;
    item->overruns = 0;
    item->efficiency = 0.0f;
    
    if (allocated_us) *allocated_us = allocation;
    
    return EFI_SUCCESS;
}

/**
 * Start timing an operation
 */
void timebudget_start(TimeBudgetContext* ctx, const CHAR8* name) {
    if (!ctx || !name) return;
    
    BudgetItem* item = find_item(ctx, name);
    if (!item) return;
    
    item->start_time = get_time_us();
    item->is_active = TRUE;
}

/**
 * End timing an operation
 */
BudgetStatus timebudget_end(TimeBudgetContext* ctx, const CHAR8* name) {
    if (!ctx || !name) return BUDGET_OK;
    
    BudgetItem* item = find_item(ctx, name);
    if (!item || !item->is_active) return BUDGET_OK;
    
    UINT64 end_time = get_time_us();
    UINT64 elapsed = end_time - item->start_time;
    
    item->consumed_us = elapsed;
    item->is_active = FALSE;
    ctx->current_consumed_us += elapsed;
    ctx->total_consumed_us += elapsed;
    
    // Calculate efficiency
    if (item->allocated_us > 0) {
        item->efficiency = (float)item->consumed_us / (float)item->allocated_us;
    }
    
    // Check status
    BudgetStatus status = BUDGET_OK;
    
    if (item->consumed_us > item->allocated_us) {
        item->overruns++;
        ctx->total_overruns++;
        
        float ratio = (float)item->consumed_us / (float)item->allocated_us;
        if (ratio > 1.5f) {
            status = BUDGET_CRITICAL;
        } else {
            status = BUDGET_EXCEEDED;
        }
    } else if ((float)item->consumed_us / (float)item->allocated_us > 0.75f) {
        status = BUDGET_WARNING;
    }
    
    return status;
}

/**
 * Check if budget allows more computation
 */
BOOLEAN timebudget_can_continue(const TimeBudgetContext* ctx) {
    if (!ctx) return FALSE;
    
    // Soft limit: allow continuation even if over budget
    if (!ctx->strict_enforcement) return TRUE;
    
    // Strict limit: enforce budget
    return ctx->current_consumed_us < ctx->per_token_budget_us;
}

/**
 * Get remaining budget for current token
 */
UINT64 timebudget_remaining(const TimeBudgetContext* ctx) {
    if (!ctx) return 0;
    
    if (ctx->current_consumed_us >= ctx->per_token_budget_us) {
        return 0;
    }
    
    return ctx->per_token_budget_us - ctx->current_consumed_us;
}

/**
 * Adaptive adjustment based on utilization
 */
void timebudget_adapt(TimeBudgetContext* ctx) {
    if (!ctx || !ctx->adaptive_enabled) return;
    
    // Calculate average utilization
    if (ctx->tokens_processed == 0) return;
    
    float avg_util = (float)ctx->total_consumed_us / 
                     (float)(ctx->per_token_budget_us * ctx->tokens_processed);
    
    ctx->average_utilization = avg_util;
    
    // Adjust mode based on utilization
    if (avg_util < 0.5f && ctx->mode < TIMEBUDGET_THOROUGH) {
        // Underutilized: upgrade to higher quality mode
        ctx->mode = (ComputationMode)(ctx->mode + 1);
        ctx->per_token_budget_us = MODE_BUDGETS[ctx->mode];
        ctx->adaptive_adjustments++;
        
        // Check bounds
        if (ctx->mode > TIMEBUDGET_THOROUGH) ctx->mode = TIMEBUDGET_THOROUGH;
        
    } else if (avg_util > 1.2f && ctx->mode > TIMEBUDGET_FAST) {
        // Over budget: downgrade to faster mode
        ctx->mode = (ComputationMode)(ctx->mode - 1);
        ctx->per_token_budget_us = MODE_BUDGETS[ctx->mode];
        ctx->adaptive_adjustments++;
    }
}

/**
 * Start new token budget
 */
void timebudget_new_token(TimeBudgetContext* ctx) {
    if (!ctx) return;
    
    // Save to history
    if (ctx->history_count < MAX_BUDGET_HISTORY) {
        BudgetHistory* hist = &ctx->history[ctx->history_count++];
        hist->timestamp = get_time_us();
        hist->total_budget_us = ctx->per_token_budget_us;
        hist->total_consumed_us = ctx->current_consumed_us;
        hist->status = ctx->current_status;
        hist->mode = ctx->mode;
    }
    
    // Reset current token budget
    ctx->current_token_start = get_time_us();
    ctx->current_consumed_us = 0;
    ctx->tokens_processed++;
    
    // Adaptive adjustment every 10 tokens
    if (ctx->tokens_processed % 10 == 0) {
        timebudget_adapt(ctx);
    }
}

/**
 * Get current status
 */
BudgetStatus timebudget_get_status(const TimeBudgetContext* ctx) {
    if (!ctx) return BUDGET_OK;
    
    float ratio = (float)ctx->current_consumed_us / (float)ctx->per_token_budget_us;
    
    if (ratio > 1.5f) return BUDGET_CRITICAL;
    if (ratio > 1.0f) return BUDGET_EXCEEDED;
    if (ratio > 0.75f) return BUDGET_WARNING;
    return BUDGET_OK;
}

/**
 * Print budget report
 */
void timebudget_print_report(const TimeBudgetContext* ctx) {
    if (!ctx) return;
    
    const CHAR16* mode_names[] = {L"FAST", L"NORMAL", L"CAREFUL", L"THOROUGH"};
    const CHAR16* status_names[] = {L"OK", L"WARNING", L"EXCEEDED", L"CRITICAL"};
    
    Print(L"\r\n═══════════════════════════════════════════════════════════\r\n");
    Print(L"  TIME BUDGET REPORT\r\n");
    Print(L"═══════════════════════════════════════════════════════════\r\n");
    
    Print(L"  Mode:               %s\r\n", mode_names[ctx->mode]);
    Print(L"  Per-Token Budget:   %d µs\r\n", ctx->per_token_budget_us);
    Print(L"  Tokens Processed:   %d\r\n", ctx->tokens_processed);
    Print(L"  Avg Utilization:    %.1f%%\r\n", ctx->average_utilization * 100.0f);
    Print(L"  Current Status:     %s\r\n", status_names[ctx->current_status]);
    Print(L"\r\n");
    
    Print(L"  Budget Items:\r\n");
    for (UINT32 i = 0; i < ctx->item_count && i < 10; i++) {
        const BudgetItem* item = &ctx->items[i];
        Print(L"    %a: %d/%d µs (%.1f%%) [%d overruns]\r\n",
              item->name,
              item->consumed_us,
              item->allocated_us,
              item->efficiency * 100.0f,
              item->overruns);
    }
    
    Print(L"\r\n");
    Print(L"  Total Overruns:     %d\r\n", ctx->total_overruns);
    Print(L"  Adaptive Adjustments: %d\r\n", ctx->adaptive_adjustments);
    Print(L"═══════════════════════════════════════════════════════════\r\n");
}

/**
 * Get average utilization percentage
 */
float timebudget_get_utilization(const TimeBudgetContext* ctx) {
    if (!ctx || ctx->tokens_processed == 0) return 0.0f;
    return ctx->average_utilization;
}
