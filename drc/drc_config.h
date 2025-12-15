/*
 * DRC Configuration System
 * Runtime tuning without recompilation
 */

#ifndef DRC_CONFIG_H
#define DRC_CONFIG_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// CONFIGURATION SYSTEM
// ═══════════════════════════════════════════════════════════════

typedef enum {
    MODE_PERMISSIVE,    // Accept almost everything
    MODE_NORMAL,        // Balanced (default)
    MODE_STRICT,        // High standards
    MODE_PARANOID       // Maximum safety
} DRCMode;

typedef struct {
    // Global mode
    DRCMode mode;
    
    // URS configuration
    UINT32 urs_reasoning_interval;      // Run every N tokens (default: 5)
    float urs_path_threshold;           // Min score to accept path (default: 0.3)
    UINT32 urs_max_paths;               // Max parallel paths (default: 4)
    
    // UIC configuration
    float uic_sensitivity;              // Detection sensitivity (default: 0.7)
    BOOLEAN uic_strict_mode;            // Block on MEDIUM severity (default: FALSE)
    BOOLEAN uic_enable_cycle_check;     // Enable cycle detection (default: TRUE)
    BOOLEAN uic_enable_temporal_check;  // Enable temporal checks (default: TRUE)
    
    // UCR configuration
    float ucr_min_confidence;           // Min confidence to accept (default: 0.7)
    float ucr_max_incoherence;          // Max incoherence tolerated (default: 0.3)
    BOOLEAN ucr_paranoid_mode;          // Ultra-strict mode (default: FALSE)
    
    // UMS configuration
    float ums_validation_threshold;     // Threshold for fact validation (default: 0.9)
    UINT32 ums_max_facts;               // Max facts to store (default: 128)
    BOOLEAN ums_strict_mode;            // Reject uncertain facts (default: TRUE)
    UINT32 ums_immutable_after;         // Make immutable after N validations (default: 3)
    
    // UCO configuration
    UINT32 uco_max_attacks;             // Max attacks per path (default: 16)
    float uco_attack_threshold;         // Attack success threshold (default: 0.7)
    BOOLEAN uco_enable_counterexamples; // Generate counterexamples (default: TRUE)
    
    // UTI configuration
    BOOLEAN uti_enable_time_tracking;   // Enable temporal tracking (default: TRUE)
    BOOLEAN uti_strict_causality;       // Enforce strict causal order (default: TRUE)
    
    // Performance configuration
    BOOLEAN perf_enable_monitoring;     // Enable performance tracking (default: TRUE)
    BOOLEAN perf_detailed_timing;       // Detailed per-call timing (default: FALSE)
    
    // Debug configuration
    BOOLEAN debug_verbose;              // Verbose output (default: FALSE)
    BOOLEAN debug_print_graphs;         // Print reasoning graphs (default: FALSE)
    BOOLEAN debug_print_decisions;      // Print all decisions (default: FALSE)
} DRCConfig;

// ═══════════════════════════════════════════════════════════════
// CONFIGURATION FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize configuration with defaults
 */
EFI_STATUS config_init(DRCConfig* config);

/**
 * Set mode (applies preset)
 */
void config_set_mode(DRCConfig* config, DRCMode mode);

/**
 * Load configuration from preset
 */
void config_load_permissive(DRCConfig* config);
void config_load_normal(DRCConfig* config);
void config_load_strict(DRCConfig* config);
void config_load_paranoid(DRCConfig* config);

/**
 * Validate configuration (check for invalid values)
 */
BOOLEAN config_validate(DRCConfig* config);

/**
 * Print current configuration
 */
void config_print(DRCConfig* config);

/**
 * Export configuration to string (for logging)
 */
void config_export(DRCConfig* config, CHAR8* buffer, UINT32 buffer_size);

#endif // DRC_CONFIG_H
