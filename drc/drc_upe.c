/*
 * UPE - Unité de Plausibilité Expérientielle Implementation
 */

#include "drc_upe.h"

// ═══════════════════════════════════════════════════════════════
// UPE IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

/**
 * Helper: String contains
 */
static BOOLEAN upe_contains(const CHAR8* haystack, const CHAR8* needle) {
    if (!haystack || !needle) return FALSE;
    UINT32 h = 0;
    while (haystack[h] != '\0') {
        UINT32 n = 0, start = h;
        while (needle[n] != '\0' && haystack[start] == needle[n]) { n++; start++; }
        if (needle[n] == '\0') return TRUE;
        h++;
    }
    return FALSE;
}

/**
 * Helper: String copy
 */
static void upe_strcpy(CHAR8* dest, const CHAR8* src, UINT32 max_len) {
    UINT32 i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * Initialize UPE
 */
EFI_STATUS upe_init(UPEContext* upe) {
    if (!upe) return EFI_INVALID_PARAMETER;
    
    upe->current.level = PLAUSIBLE;
    upe->current.violation = VIOLATION_NONE;
    upe->current.feasible = TRUE;
    upe->current.plausibility_score = 1.0f;
    upe->current.reason[0] = '\0';
    
    upe->total_checks = 0;
    upe->impossible_detected = 0;
    upe->implausible_detected = 0;
    upe->physics_violations = 0;
    
    upe->strict_physics = TRUE;
    upe->plausibility_threshold = 0.5f;
    
    return EFI_SUCCESS;
}

/**
 * Check for physics violations
 */
BOOLEAN upe_check_physics(UPEContext* upe, const CHAR8* statement) {
    if (!upe || !statement) return FALSE;
    
    // Keywords indicating physics violations
    const CHAR8* impossible_keywords[] = {
        "perpetual motion", "faster than light", "infinite energy",
        "time travel", "teleport", "antigravity", NULL
    };
    
    for (UINT32 i = 0; impossible_keywords[i] != NULL; i++) {
        if (upe_contains(statement, impossible_keywords[i])) {
            upe->current.violation = VIOLATION_PHYSICS;
            upe->physics_violations++;
            return TRUE;  // Violation detected
        }
    }
    
    return FALSE;
}

/**
 * Check resource feasibility
 */
BOOLEAN upe_check_resources(UPEContext* upe, const CHAR8* statement) {
    if (!upe || !statement) return FALSE;
    
    // Keywords indicating resource issues
    const CHAR8* resource_keywords[] = {
        "unlimited", "infinite resource", "costless", "free energy", NULL
    };
    
    for (UINT32 i = 0; resource_keywords[i] != NULL; i++) {
        if (upe_contains(statement, resource_keywords[i])) {
            upe->current.violation = VIOLATION_RESOURCE;
            return TRUE;  // Violation detected
        }
    }
    
    return FALSE;
}

/**
 * Check scale feasibility
 */
BOOLEAN upe_check_scale(UPEContext* upe, const CHAR8* statement) {
    if (!upe || !statement) return FALSE;
    
    // Keywords indicating scale issues
    const CHAR8* scale_keywords[] = {
        "microscopic universe", "infinitely large", "zero volume",
        "instant construction", NULL
    };
    
    for (UINT32 i = 0; scale_keywords[i] != NULL; i++) {
        if (upe_contains(statement, scale_keywords[i])) {
            upe->current.violation = VIOLATION_SCALE;
            return TRUE;  // Violation detected
        }
    }
    
    return FALSE;
}

/**
 * Check plausibility of statement
 */
PlausibilityLevel upe_check_plausibility(UPEContext* upe, const CHAR8* statement) {
    if (!upe || !statement) return UNKNOWN;
    
    upe->total_checks++;
    
    // Reset
    upe->current.level = PLAUSIBLE;
    upe->current.violation = VIOLATION_NONE;
    upe->current.feasible = TRUE;
    upe->current.plausibility_score = 1.0f;
    
    // Check physics
    if (upe_check_physics(upe, statement)) {
        upe->current.level = IMPOSSIBLE;
        upe->current.feasible = FALSE;
        upe->current.plausibility_score = 0.0f;
        upe_strcpy(upe->current.reason, "Physics violation detected", 128);
        upe->impossible_detected++;
        return IMPOSSIBLE;
    }
    
    // Check resources
    if (upe_check_resources(upe, statement)) {
        upe->current.level = IMPLAUSIBLE;
        upe->current.plausibility_score = 0.2f;
        upe_strcpy(upe->current.reason, "Resource constraints violated", 128);
        upe->implausible_detected++;
        return IMPLAUSIBLE;
    }
    
    // Check scale
    if (upe_check_scale(upe, statement)) {
        upe->current.level = IMPLAUSIBLE;
        upe->current.plausibility_score = 0.3f;
        upe_strcpy(upe->current.reason, "Scale issues detected", 128);
        upe->implausible_detected++;
        return IMPLAUSIBLE;
    }
    
    upe_strcpy(upe->current.reason, "Plausible in real world", 128);
    return PLAUSIBLE;
}

/**
 * Get plausibility score
 */
float upe_get_score(UPEContext* upe) {
    if (!upe) return 0.0f;
    return upe->current.plausibility_score;
}

/**
 * Print plausibility report
 */
void upe_print_report(UPEContext* upe) {
    if (!upe) return;
    
    Print(L"\n[UPE] Experiential Plausibility Report\n");
    Print(L"  Total checks: %u\n", upe->total_checks);
    Print(L"  Impossible detected: %u\n", upe->impossible_detected);
    Print(L"  Implausible detected: %u\n", upe->implausible_detected);
    Print(L"  Physics violations: %u\n", upe->physics_violations);
    
    const CHAR16* level_str = L"?";
    switch (upe->current.level) {
        case PLAUSIBLE: level_str = L"PLAUSIBLE"; break;
        case IMPLAUSIBLE: level_str = L"IMPLAUSIBLE"; break;
        case IMPOSSIBLE: level_str = L"IMPOSSIBLE"; break;
        case UNKNOWN: level_str = L"UNKNOWN"; break;
    }
    Print(L"  Current level: %s\n", level_str);
    Print(L"  Plausibility score: %.2f\n", upe->current.plausibility_score);
    Print(L"  Reason: %a\n", upe->current.reason);
    
    if (!upe->current.feasible) {
        Print(L"  ⛔ NOT FEASIBLE\n");
    } else {
        Print(L"  ✓ Feasible\n");
    }
}
