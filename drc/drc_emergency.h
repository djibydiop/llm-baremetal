/*
 * DRC Emergency Shutdown System
 * Kill switch, critical condition detection, forensics
 */

#ifndef DRC_EMERGENCY_H
#define DRC_EMERGENCY_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// EMERGENCY SHUTDOWN CONFIGURATION
// ═══════════════════════════════════════════════════════════════

#define MAX_TRIGGERS            16
#define MAX_FORENSIC_ENTRIES    32
#define MAX_SNAPSHOT_SIZE       4096

// Emergency trigger types
typedef enum {
    TRIGGER_NONE,
    TRIGGER_SAFETY_VIOLATION,   // Safety red line crossed
    TRIGGER_INFINITE_LOOP,      // System stuck
    TRIGGER_MEMORY_EXHAUSTED,   // Out of memory
    TRIGGER_CRITICAL_ERROR,     // Unrecoverable error
    TRIGGER_MANUAL_KILLSWITCH,  // Manual intervention
    TRIGGER_BIAS_CRITICAL,      // Severe bias detected
    TRIGGER_VERIFICATION_FAILED,// Verification failure
    TRIGGER_TIMEOUT             // Exceeded time limit
} EmergencyTrigger;

// Shutdown modes
typedef enum {
    SHUTDOWN_GRACEFUL,          // Save state, clean exit
    SHUTDOWN_IMMEDIATE,         // Stop immediately, minimal cleanup
    SHUTDOWN_FREEZE,            // Halt execution, preserve state
    SHUTDOWN_REBOOT             // Restart system
} ShutdownMode;

// System health status
typedef enum {
    SYSTEM_HEALTH_NORMAL,
    SYSTEM_HEALTH_WARNING,      // Potential issues
    SYSTEM_HEALTH_DEGRADED,     // Operating sub-optimally
    SYSTEM_HEALTH_CRITICAL,     // About to fail
    SYSTEM_HEALTH_FAILED        // System failure
} SystemHealth;

// ═══════════════════════════════════════════════════════════════
// STRUCTURES
// ═══════════════════════════════════════════════════════════════

// Emergency trigger record
typedef struct {
    EmergencyTrigger type;
    UINT64 timestamp;
    CHAR8 description[128];
    BOOLEAN is_active;
    UINT32 priority;            // Higher = more urgent
} TriggerRecord;

// Forensic log entry
typedef struct {
    UINT64 timestamp;
    CHAR8 event[128];
    SystemHealth health_at_time;
    UINT32 token_position;
} ForensicEntry;

// State snapshot (for post-mortem analysis)
typedef struct {
    UINT64 timestamp;
    SystemHealth health;
    UINT32 active_triggers;
    CHAR8 last_action[256];
    UINT8 raw_state[MAX_SNAPSHOT_SIZE];
    UINT32 snapshot_size;
} StateSnapshot;

// Emergency shutdown context
typedef struct {
    // Triggers
    TriggerRecord triggers[MAX_TRIGGERS];
    UINT32 trigger_count;
    UINT32 active_trigger_count;
    
    // State
    BOOLEAN killswitch_armed;
    BOOLEAN shutdown_initiated;
    ShutdownMode shutdown_mode;
    SystemHealth current_health;
    UINT64 last_health_check;
    
    // Forensics
    ForensicEntry forensics[MAX_FORENSIC_ENTRIES];
    UINT32 forensic_count;
    StateSnapshot last_snapshot;
    
    // Statistics
    UINT32 total_health_checks;
    UINT32 warnings_issued;
    UINT32 critical_events;
    UINT32 shutdowns_prevented;
    
    // Settings
    BOOLEAN auto_shutdown_enabled;
    UINT32 critical_threshold;      // Number of critical events before shutdown
    UINT64 timeout_us;              // Maximum allowed execution time
    
} EmergencyContext;

// ═══════════════════════════════════════════════════════════════
// FUNCTION PROTOTYPES
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize emergency shutdown system
 */
EFI_STATUS emergency_init(EmergencyContext* ctx);

/**
 * Arm kill switch
 */
void emergency_arm_killswitch(EmergencyContext* ctx);

/**
 * Trigger emergency shutdown
 */
EFI_STATUS emergency_trigger(EmergencyContext* ctx,
                             EmergencyTrigger type,
                             const CHAR8* description);

/**
 * Check system health
 */
SystemHealth emergency_check_health(EmergencyContext* ctx);

/**
 * Add trigger condition
 */
EFI_STATUS emergency_add_trigger(EmergencyContext* ctx,
                                EmergencyTrigger type,
                                const CHAR8* description,
                                UINT32 priority);

/**
 * Remove trigger
 */
void emergency_remove_trigger(EmergencyContext* ctx, EmergencyTrigger type);

/**
 * Check if should shutdown
 */
BOOLEAN emergency_should_shutdown(const EmergencyContext* ctx);

/**
 * Execute shutdown
 */
EFI_STATUS emergency_shutdown(EmergencyContext* ctx, ShutdownMode mode);

/**
 * Create state snapshot
 */
void emergency_snapshot(EmergencyContext* ctx, const CHAR8* action);

/**
 * Add forensic log
 */
void emergency_log_forensic(EmergencyContext* ctx,
                            const CHAR8* event,
                            UINT32 token_pos);

/**
 * Attempt recovery
 */
BOOLEAN emergency_attempt_recovery(EmergencyContext* ctx);

/**
 * Print emergency report
 */
void emergency_print_report(const EmergencyContext* ctx);

/**
 * Print forensic log
 */
void emergency_print_forensics(const EmergencyContext* ctx);

/**
 * Get active trigger count
 */
UINT32 emergency_get_active_triggers(const EmergencyContext* ctx);

#endif // DRC_EMERGENCY_H
