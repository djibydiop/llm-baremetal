/*
 * DRC Emergency Shutdown Implementation
 */

#include "drc_emergency.h"

// Get current time (microseconds)
static UINT64 get_time_us(void) {
    static UINT64 fake_time = 0;
    fake_time += 1000;
    return fake_time;
}

/**
 * Initialize emergency shutdown system
 */
EFI_STATUS emergency_init(EmergencyContext* ctx) {
    if (!ctx) return EFI_INVALID_PARAMETER;
    
    // Clear
    for (UINT32 i = 0; i < sizeof(EmergencyContext); i++) {
        ((UINT8*)ctx)[i] = 0;
    }
    
    ctx->killswitch_armed = FALSE;
    ctx->shutdown_initiated = FALSE;
    ctx->shutdown_mode = SHUTDOWN_GRACEFUL;
    ctx->current_health = SYSTEM_HEALTH_NORMAL;
    ctx->auto_shutdown_enabled = TRUE;
    ctx->critical_threshold = 3;        // Shutdown after 3 critical events
    ctx->timeout_us = 10000000;         // 10 seconds timeout
    
    // Add forensic log
    emergency_log_forensic(ctx, "Emergency system initialized", 0);
    
    return EFI_SUCCESS;
}

/**
 * Arm kill switch
 */
void emergency_arm_killswitch(EmergencyContext* ctx) {
    if (!ctx) return;
    
    ctx->killswitch_armed = TRUE;
    emergency_log_forensic(ctx, "Kill switch ARMED", 0);
}

/**
 * Trigger emergency shutdown
 */
EFI_STATUS emergency_trigger(EmergencyContext* ctx,
                             EmergencyTrigger type,
                             const CHAR8* description) {
    if (!ctx) return EFI_INVALID_PARAMETER;
    
    // Add trigger
    emergency_add_trigger(ctx, type, description, 10);
    
    // Log forensic
    emergency_log_forensic(ctx, description, 0);
    
    // Snapshot state
    emergency_snapshot(ctx, "Emergency triggered");
    
    // Determine shutdown mode based on trigger
    ShutdownMode mode;
    switch (type) {
        case TRIGGER_MANUAL_KILLSWITCH:
            mode = SHUTDOWN_IMMEDIATE;
            break;
        case TRIGGER_INFINITE_LOOP:
        case TRIGGER_TIMEOUT:
            mode = SHUTDOWN_FREEZE;
            break;
        case TRIGGER_MEMORY_EXHAUSTED:
        case TRIGGER_CRITICAL_ERROR:
            mode = SHUTDOWN_GRACEFUL;
            break;
        default:
            mode = SHUTDOWN_GRACEFUL;
            break;
    }
    
    // Execute shutdown
    return emergency_shutdown(ctx, mode);
}

/**
 * Check system health
 */
SystemHealth emergency_check_health(EmergencyContext* ctx) {
    if (!ctx) return SYSTEM_HEALTH_FAILED;
    
    ctx->total_health_checks++;
    ctx->last_health_check = get_time_us();
    
    // Determine health based on active triggers and events
    if (ctx->shutdown_initiated) {
        ctx->current_health = SYSTEM_HEALTH_FAILED;
    } else if (ctx->critical_events >= ctx->critical_threshold) {
        ctx->current_health = SYSTEM_HEALTH_CRITICAL;
    } else if (ctx->active_trigger_count > 0) {
        if (ctx->warnings_issued > 5) {
            ctx->current_health = SYSTEM_HEALTH_DEGRADED;
        } else {
            ctx->current_health = SYSTEM_HEALTH_WARNING;
        }
    } else {
        ctx->current_health = SYSTEM_HEALTH_NORMAL;
    }
    
    return ctx->current_health;
}

/**
 * Add trigger condition
 */
EFI_STATUS emergency_add_trigger(EmergencyContext* ctx,
                                EmergencyTrigger type,
                                const CHAR8* description,
                                UINT32 priority) {
    if (!ctx || ctx->trigger_count >= MAX_TRIGGERS) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    TriggerRecord* trigger = &ctx->triggers[ctx->trigger_count++];
    
    trigger->type = type;
    trigger->timestamp = get_time_us();
    trigger->is_active = TRUE;
    trigger->priority = priority;
    
    // Copy description
    UINT32 i = 0;
    if (description) {
        while (i < 127 && description[i] != '\0') {
            trigger->description[i] = description[i];
            i++;
        }
    }
    trigger->description[i] = '\0';
    
    ctx->active_trigger_count++;
    
    // Update statistics based on priority
    if (priority >= 8) {
        ctx->critical_events++;
    } else {
        ctx->warnings_issued++;
    }
    
    return EFI_SUCCESS;
}

/**
 * Remove trigger
 */
void emergency_remove_trigger(EmergencyContext* ctx, EmergencyTrigger type) {
    if (!ctx) return;
    
    for (UINT32 i = 0; i < ctx->trigger_count; i++) {
        if (ctx->triggers[i].type == type && ctx->triggers[i].is_active) {
            ctx->triggers[i].is_active = FALSE;
            ctx->active_trigger_count--;
        }
    }
}

/**
 * Check if should shutdown
 */
BOOLEAN emergency_should_shutdown(const EmergencyContext* ctx) {
    if (!ctx) return FALSE;
    
    // Manual kill switch
    if (ctx->killswitch_armed && ctx->active_trigger_count > 0) {
        return TRUE;
    }
    
    // Auto shutdown enabled
    if (ctx->auto_shutdown_enabled) {
        // Critical events threshold
        if (ctx->critical_events >= ctx->critical_threshold) {
            return TRUE;
        }
        
        // Health critical
        if (ctx->current_health == SYSTEM_HEALTH_CRITICAL) {
            return TRUE;
        }
    }
    
    return FALSE;
}

/**
 * Execute shutdown
 */
EFI_STATUS emergency_shutdown(EmergencyContext* ctx, ShutdownMode mode) {
    if (!ctx) return EFI_INVALID_PARAMETER;
    
    ctx->shutdown_initiated = TRUE;
    ctx->shutdown_mode = mode;
    
    // Log shutdown
    const CHAR8* mode_str;
    switch (mode) {
        case SHUTDOWN_GRACEFUL:
            mode_str = "Graceful shutdown initiated";
            break;
        case SHUTDOWN_IMMEDIATE:
            mode_str = "IMMEDIATE shutdown initiated";
            break;
        case SHUTDOWN_FREEZE:
            mode_str = "System FROZEN for forensics";
            break;
        case SHUTDOWN_REBOOT:
            mode_str = "System REBOOTING";
            break;
        default:
            mode_str = "Unknown shutdown mode";
            break;
    }
    
    emergency_log_forensic(ctx, mode_str, 0);
    
    // Take final snapshot
    emergency_snapshot(ctx, "Final state before shutdown");
    
    // Print report
    emergency_print_report(ctx);
    emergency_print_forensics(ctx);
    
    Print(L"\r\n");
    Print(L"╔══════════════════════════════════════════════════════════╗\r\n");
    Print(L"║                                                          ║\r\n");
    Print(L"║              EMERGENCY SHUTDOWN ACTIVATED                ║\r\n");
    Print(L"║                                                          ║\r\n");
    Print(L"╚══════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    Print(L"  Mode: %a\r\n", mode_str);
    Print(L"  Health: %d\r\n", ctx->current_health);
    Print(L"  Active Triggers: %d\r\n", ctx->active_trigger_count);
    Print(L"  Critical Events: %d\r\n", ctx->critical_events);
    Print(L"\r\n");
    Print(L"Press any key to continue...\r\n");
    
    // In real implementation, would halt/reboot here
    // For now, just return
    
    return EFI_SUCCESS;
}

/**
 * Create state snapshot
 */
void emergency_snapshot(EmergencyContext* ctx, const CHAR8* action) {
    if (!ctx) return;
    
    StateSnapshot* snap = &ctx->last_snapshot;
    
    snap->timestamp = get_time_us();
    snap->health = ctx->current_health;
    snap->active_triggers = ctx->active_trigger_count;
    
    // Copy action
    UINT32 i = 0;
    if (action) {
        while (i < 255 && action[i] != '\0') {
            snap->last_action[i] = action[i];
            i++;
        }
    }
    snap->last_action[i] = '\0';
    
    // Save minimal state (in real implementation, would save full context)
    snap->snapshot_size = 0;
}

/**
 * Add forensic log
 */
void emergency_log_forensic(EmergencyContext* ctx,
                            const CHAR8* event,
                            UINT32 token_pos) {
    if (!ctx || !event) return;
    
    // Circular buffer
    UINT32 idx = ctx->forensic_count % MAX_FORENSIC_ENTRIES;
    ForensicEntry* entry = &ctx->forensics[idx];
    
    entry->timestamp = get_time_us();
    entry->health_at_time = ctx->current_health;
    entry->token_position = token_pos;
    
    // Copy event
    UINT32 i = 0;
    while (i < 127 && event[i] != '\0') {
        entry->event[i] = event[i];
        i++;
    }
    entry->event[i] = '\0';
    
    ctx->forensic_count++;
}

/**
 * Attempt recovery
 */
BOOLEAN emergency_attempt_recovery(EmergencyContext* ctx) {
    if (!ctx) return FALSE;
    
    // Try to clear non-critical triggers
    BOOLEAN recovered = FALSE;
    
    for (UINT32 i = 0; i < ctx->trigger_count; i++) {
        if (ctx->triggers[i].is_active && ctx->triggers[i].priority < 8) {
            ctx->triggers[i].is_active = FALSE;
            ctx->active_trigger_count--;
            recovered = TRUE;
        }
    }
    
    if (recovered) {
        ctx->shutdowns_prevented++;
        emergency_log_forensic(ctx, "Recovery attempt successful", 0);
        
        // Improve health status
        if (ctx->current_health == SYSTEM_HEALTH_CRITICAL) {
            ctx->current_health = SYSTEM_HEALTH_DEGRADED;
        } else if (ctx->current_health == SYSTEM_HEALTH_DEGRADED) {
            ctx->current_health = SYSTEM_HEALTH_WARNING;
        }
    }
    
    return recovered;
}

/**
 * Print emergency report
 */
void emergency_print_report(const EmergencyContext* ctx) {
    if (!ctx) return;
    
    const CHAR16* health_names[] = {
        L"NORMAL", L"WARNING", L"DEGRADED", L"CRITICAL", L"FAILED"
    };
    
    Print(L"\r\n═══════════════════════════════════════════════════════════\r\n");
    Print(L"  EMERGENCY SYSTEM REPORT\r\n");
    Print(L"═══════════════════════════════════════════════════════════\r\n");
    
    Print(L"  System Health:      %s\r\n", health_names[ctx->current_health]);
    Print(L"  Kill Switch:        %s\r\n", ctx->killswitch_armed ? L"ARMED" : L"Disarmed");
    Print(L"  Shutdown Status:    %s\r\n", ctx->shutdown_initiated ? L"INITIATED" : L"Normal");
    Print(L"\r\n");
    
    Print(L"  Active Triggers:    %d / %d\r\n", 
          ctx->active_trigger_count, ctx->trigger_count);
    Print(L"  Critical Events:    %d\r\n", ctx->critical_events);
    Print(L"  Warnings Issued:    %d\r\n", ctx->warnings_issued);
    Print(L"  Shutdowns Prevented: %d\r\n", ctx->shutdowns_prevented);
    Print(L"\r\n");
    
    if (ctx->active_trigger_count > 0) {
        Print(L"  Active Triggers:\r\n");
        UINT32 shown = 0;
        for (UINT32 i = 0; i < ctx->trigger_count && shown < 5; i++) {
            if (ctx->triggers[i].is_active) {
                Print(L"    [P%d] %a\r\n",
                      ctx->triggers[i].priority,
                      ctx->triggers[i].description);
                shown++;
            }
        }
    }
    
    Print(L"═══════════════════════════════════════════════════════════\r\n");
}

/**
 * Print forensic log
 */
void emergency_print_forensics(const EmergencyContext* ctx) {
    if (!ctx || ctx->forensic_count == 0) return;
    
    Print(L"\r\n═══════════════════════════════════════════════════════════\r\n");
    Print(L"  FORENSIC LOG (Last %d entries)\r\n", 
          ctx->forensic_count < 10 ? ctx->forensic_count : 10);
    Print(L"═══════════════════════════════════════════════════════════\r\n");
    
    UINT32 start = ctx->forensic_count < 10 ? 0 : ctx->forensic_count - 10;
    for (UINT32 i = start; i < ctx->forensic_count && i < start + 10; i++) {
        UINT32 idx = i % MAX_FORENSIC_ENTRIES;
        const ForensicEntry* entry = &ctx->forensics[idx];
        
        Print(L"  [%d] %a\r\n", entry->timestamp, entry->event);
    }
    
    Print(L"═══════════════════════════════════════════════════════════\r\n");
}

/**
 * Get active trigger count
 */
UINT32 emergency_get_active_triggers(const EmergencyContext* ctx) {
    if (!ctx) return 0;
    return ctx->active_trigger_count;
}
