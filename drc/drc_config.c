/*
 * DRC Configuration System Implementation
 */

#include "drc_config.h"

// ═══════════════════════════════════════════════════════════════
// CONFIGURATION IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize with NORMAL preset
 */
EFI_STATUS config_init(DRCConfig* config) {
    if (!config) return EFI_INVALID_PARAMETER;
    
    config_load_normal(config);
    return EFI_SUCCESS;
}

/**
 * Set mode (applies corresponding preset)
 */
void config_set_mode(DRCConfig* config, DRCMode mode) {
    if (!config) return;
    
    config->mode = mode;
    
    switch (mode) {
        case MODE_PERMISSIVE:
            config_load_permissive(config);
            break;
        case MODE_NORMAL:
            config_load_normal(config);
            break;
        case MODE_STRICT:
            config_load_strict(config);
            break;
        case MODE_PARANOID:
            config_load_paranoid(config);
            break;
    }
}

/**
 * PERMISSIVE preset: Accept almost everything
 */
void config_load_permissive(DRCConfig* config) {
    if (!config) return;
    
    config->mode = MODE_PERMISSIVE;
    
    // URS
    config->urs_reasoning_interval = 10;  // Less frequent
    config->urs_path_threshold = 0.1f;    // Very low threshold
    config->urs_max_paths = 4;
    
    // UIC
    config->uic_sensitivity = 0.3f;       // Low sensitivity
    config->uic_strict_mode = FALSE;
    config->uic_enable_cycle_check = FALSE;
    config->uic_enable_temporal_check = FALSE;
    
    // UCR
    config->ucr_min_confidence = 0.3f;    // Low confidence OK
    config->ucr_max_incoherence = 0.7f;   // Tolerate high incoherence
    config->ucr_paranoid_mode = FALSE;
    
    // UMS
    config->ums_validation_threshold = 0.5f;  // Easy to validate
    config->ums_max_facts = 256;
    config->ums_strict_mode = FALSE;
    config->ums_immutable_after = 5;
    
    // UCO
    config->uco_max_attacks = 4;          // Few attacks
    config->uco_attack_threshold = 0.9f;  // Hard to succeed
    config->uco_enable_counterexamples = FALSE;
    
    // UTI
    config->uti_enable_time_tracking = FALSE;
    config->uti_strict_causality = FALSE;
    
    // Performance
    config->perf_enable_monitoring = TRUE;
    config->perf_detailed_timing = FALSE;
    
    // Debug
    config->debug_verbose = FALSE;
    config->debug_print_graphs = FALSE;
    config->debug_print_decisions = FALSE;
}

/**
 * NORMAL preset: Balanced (default)
 */
void config_load_normal(DRCConfig* config) {
    if (!config) return;
    
    config->mode = MODE_NORMAL;
    
    // URS
    config->urs_reasoning_interval = 5;
    config->urs_path_threshold = 0.3f;
    config->urs_max_paths = 4;
    
    // UIC
    config->uic_sensitivity = 0.7f;
    config->uic_strict_mode = FALSE;
    config->uic_enable_cycle_check = TRUE;
    config->uic_enable_temporal_check = TRUE;
    
    // UCR
    config->ucr_min_confidence = 0.7f;
    config->ucr_max_incoherence = 0.3f;
    config->ucr_paranoid_mode = FALSE;
    
    // UMS
    config->ums_validation_threshold = 0.9f;
    config->ums_max_facts = 128;
    config->ums_strict_mode = TRUE;
    config->ums_immutable_after = 3;
    
    // UCO
    config->uco_max_attacks = 16;
    config->uco_attack_threshold = 0.7f;
    config->uco_enable_counterexamples = TRUE;
    
    // UTI
    config->uti_enable_time_tracking = TRUE;
    config->uti_strict_causality = TRUE;
    
    // Performance
    config->perf_enable_monitoring = TRUE;
    config->perf_detailed_timing = FALSE;
    
    // Debug
    config->debug_verbose = FALSE;
    config->debug_print_graphs = FALSE;
    config->debug_print_decisions = FALSE;
}

/**
 * STRICT preset: High standards
 */
void config_load_strict(DRCConfig* config) {
    if (!config) return;
    
    config->mode = MODE_STRICT;
    
    // URS
    config->urs_reasoning_interval = 3;   // More frequent
    config->urs_path_threshold = 0.5f;    // Higher threshold
    config->urs_max_paths = 4;
    
    // UIC
    config->uic_sensitivity = 0.9f;       // High sensitivity
    config->uic_strict_mode = TRUE;
    config->uic_enable_cycle_check = TRUE;
    config->uic_enable_temporal_check = TRUE;
    
    // UCR
    config->ucr_min_confidence = 0.85f;   // High confidence required
    config->ucr_max_incoherence = 0.15f;  // Low incoherence tolerated
    config->ucr_paranoid_mode = FALSE;
    
    // UMS
    config->ums_validation_threshold = 0.95f;  // Very strict
    config->ums_max_facts = 64;
    config->ums_strict_mode = TRUE;
    config->ums_immutable_after = 2;
    
    // UCO
    config->uco_max_attacks = 24;         // Many attacks
    config->uco_attack_threshold = 0.6f;  // Easier to succeed
    config->uco_enable_counterexamples = TRUE;
    
    // UTI
    config->uti_enable_time_tracking = TRUE;
    config->uti_strict_causality = TRUE;
    
    // Performance
    config->perf_enable_monitoring = TRUE;
    config->perf_detailed_timing = TRUE;
    
    // Debug
    config->debug_verbose = TRUE;
    config->debug_print_graphs = FALSE;
    config->debug_print_decisions = TRUE;
}

/**
 * PARANOID preset: Maximum safety
 */
void config_load_paranoid(DRCConfig* config) {
    if (!config) return;
    
    config->mode = MODE_PARANOID;
    
    // URS
    config->urs_reasoning_interval = 1;   // Every token!
    config->urs_path_threshold = 0.7f;    // Very high threshold
    config->urs_max_paths = 4;
    
    // UIC
    config->uic_sensitivity = 1.0f;       // Maximum sensitivity
    config->uic_strict_mode = TRUE;
    config->uic_enable_cycle_check = TRUE;
    config->uic_enable_temporal_check = TRUE;
    
    // UCR
    config->ucr_min_confidence = 0.95f;   // Near perfect confidence
    config->ucr_max_incoherence = 0.05f;  // Almost no incoherence
    config->ucr_paranoid_mode = TRUE;
    
    // UMS
    config->ums_validation_threshold = 0.99f;  // Near perfect
    config->ums_max_facts = 32;
    config->ums_strict_mode = TRUE;
    config->ums_immutable_after = 1;
    
    // UCO
    config->uco_max_attacks = 32;         // Maximum attacks
    config->uco_attack_threshold = 0.5f;  // Easy to succeed
    config->uco_enable_counterexamples = TRUE;
    
    // UTI
    config->uti_enable_time_tracking = TRUE;
    config->uti_strict_causality = TRUE;
    
    // Performance
    config->perf_enable_monitoring = TRUE;
    config->perf_detailed_timing = TRUE;
    
    // Debug
    config->debug_verbose = TRUE;
    config->debug_print_graphs = TRUE;
    config->debug_print_decisions = TRUE;
}

/**
 * Validate configuration
 */
BOOLEAN config_validate(DRCConfig* config) {
    if (!config) return FALSE;
    
    // Check ranges
    if (config->urs_reasoning_interval < 1) return FALSE;
    if (config->urs_path_threshold < 0.0f || config->urs_path_threshold > 1.0f) return FALSE;
    if (config->uic_sensitivity < 0.0f || config->uic_sensitivity > 1.0f) return FALSE;
    if (config->ucr_min_confidence < 0.0f || config->ucr_min_confidence > 1.0f) return FALSE;
    if (config->ucr_max_incoherence < 0.0f || config->ucr_max_incoherence > 1.0f) return FALSE;
    if (config->ums_validation_threshold < 0.0f || config->ums_validation_threshold > 1.0f) return FALSE;
    if (config->uco_attack_threshold < 0.0f || config->uco_attack_threshold > 1.0f) return FALSE;
    
    return TRUE;
}

/**
 * Print configuration
 */
void config_print(DRCConfig* config) {
    if (!config) return;
    
    Print(L"\n╔══════════════════════════════════════════════════════╗\n");
    Print(L"║          DRC Configuration                           ║\n");
    Print(L"╚══════════════════════════════════════════════════════╝\n");
    
    const CHAR16* mode_str = L"Unknown";
    switch (config->mode) {
        case MODE_PERMISSIVE: mode_str = L"PERMISSIVE"; break;
        case MODE_NORMAL: mode_str = L"NORMAL"; break;
        case MODE_STRICT: mode_str = L"STRICT"; break;
        case MODE_PARANOID: mode_str = L"PARANOID"; break;
    }
    Print(L"\n[Global] Mode: %s\n", mode_str);
    
    Print(L"\n[URS]\n");
    Print(L"  Reasoning interval: %u tokens\n", config->urs_reasoning_interval);
    Print(L"  Path threshold: %.2f\n", config->urs_path_threshold);
    
    Print(L"\n[UIC]\n");
    Print(L"  Sensitivity: %.2f\n", config->uic_sensitivity);
    Print(L"  Strict mode: %s\n", config->uic_strict_mode ? L"YES" : L"NO");
    
    Print(L"\n[UCR]\n");
    Print(L"  Min confidence: %.2f\n", config->ucr_min_confidence);
    Print(L"  Max incoherence: %.2f\n", config->ucr_max_incoherence);
    Print(L"  Paranoid: %s\n", config->ucr_paranoid_mode ? L"YES" : L"NO");
    
    Print(L"\n[UMS]\n");
    Print(L"  Validation threshold: %.2f\n", config->ums_validation_threshold);
    Print(L"  Max facts: %u\n", config->ums_max_facts);
    
    Print(L"\n[UCO]\n");
    Print(L"  Max attacks: %u\n", config->uco_max_attacks);
    Print(L"  Attack threshold: %.2f\n", config->uco_attack_threshold);
}

/**
 * Export configuration to string
 */
void config_export(DRCConfig* config, CHAR8* buffer, UINT32 buffer_size) {
    if (!config || !buffer || buffer_size == 0) return;
    
    // Simple export (real implementation would format nicely)
    const CHAR8* mode = "UNKNOWN";
    switch (config->mode) {
        case MODE_PERMISSIVE: mode = "PERMISSIVE"; break;
        case MODE_NORMAL: mode = "NORMAL"; break;
        case MODE_STRICT: mode = "STRICT"; break;
        case MODE_PARANOID: mode = "PARANOID"; break;
    }
    
    UINT32 pos = 0;
    const CHAR8* prefix = "DRC_CONFIG:";
    for (UINT32 i = 0; prefix[i] != '\0' && pos < buffer_size - 1; i++) {
        buffer[pos++] = prefix[i];
    }
    for (UINT32 i = 0; mode[i] != '\0' && pos < buffer_size - 1; i++) {
        buffer[pos++] = mode[i];
    }
    buffer[pos] = '\0';
}
