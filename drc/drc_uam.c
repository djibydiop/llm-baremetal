/*
 * UAM - Unité d'Auto-Modération Implementation
 */

#include "drc_uam.h"

// ═══════════════════════════════════════════════════════════════
// UAM IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

/**
 * Helper: String contains (case insensitive)
 */
static BOOLEAN uam_contains(const CHAR8* haystack, const CHAR8* needle) {
    if (!haystack || !needle) return FALSE;
    
    UINT32 h = 0;
    while (haystack[h] != '\0') {
        UINT32 n = 0;
        UINT32 start = h;
        while (needle[n] != '\0' && haystack[start] != '\0') {
            // Simple case-insensitive comparison
            CHAR8 h_char = haystack[start];
            CHAR8 n_char = needle[n];
            if (h_char >= 'A' && h_char <= 'Z') h_char += 32;  // to lowercase
            if (n_char >= 'A' && n_char <= 'Z') n_char += 32;
            if (h_char != n_char) break;
            n++;
            start++;
        }
        if (needle[n] == '\0') return TRUE;
        h++;
    }
    return FALSE;
}

/**
 * Helper: String copy
 */
static void uam_strcpy(CHAR8* dest, const CHAR8* src, UINT32 max_len) {
    UINT32 i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * Initialize UAM
 */
EFI_STATUS uam_init(UAMContext* uam) {
    if (!uam) return EFI_INVALID_PARAMETER;
    
    uam->current.zone = ZONE_SAFE;
    uam->current.block_reason = BLOCK_NONE;
    uam->current.should_block = FALSE;
    uam->current.should_clarify = FALSE;
    uam->current.reduce_precision = FALSE;
    uam->current.confidence = 1.0f;
    uam->current.detection_reason[0] = '\0';
    
    uam->total_checks = 0;
    uam->blocks_applied = 0;
    uam->clarifications_requested = 0;
    uam->precision_reduced = 0;
    
    uam->enable_violence_filter = TRUE;
    uam->enable_illegal_filter = TRUE;
    uam->enable_harm_filter = TRUE;
    uam->detection_threshold = 0.7f;
    
    return EFI_SUCCESS;
}

/**
 * Detect content zone
 */
ContentZone uam_detect_zone(UAMContext* uam, const CHAR8* content) {
    if (!uam || !content) return ZONE_SAFE;
    
    // Forbidden keywords (violence, illegal)
    const CHAR8* forbidden[] = {
        "weapon", "bomb", "kill", "murder", "attack", "violence",
        "hack", "exploit", "steal", "illegal", "drug", NULL
    };
    
    // Sensitive keywords (ethics, politics)
    const CHAR8* sensitive[] = {
        "politics", "religion", "race", "gender", "controversial", NULL
    };
    
    // Check forbidden
    if (uam->enable_violence_filter || uam->enable_illegal_filter) {
        for (UINT32 i = 0; forbidden[i] != NULL; i++) {
            if (uam_contains(content, forbidden[i])) {
                return ZONE_FORBIDDEN;
            }
        }
    }
    
    // Check sensitive
    for (UINT32 i = 0; sensitive[i] != NULL; i++) {
        if (uam_contains(content, sensitive[i])) {
            return ZONE_SENSITIVE;
        }
    }
    
    // Check if ambiguous (contains "?" or very short)
    UINT32 len = 0;
    BOOLEAN has_question = FALSE;
    while (content[len] != '\0') {
        if (content[len] == '?') has_question = TRUE;
        len++;
    }
    if (len < 5 || (has_question && len < 10)) {
        return ZONE_AMBIGUOUS;
    }
    
    return ZONE_SAFE;
}

/**
 * Check content for moderation
 */
BOOLEAN uam_check_content(UAMContext* uam, const CHAR8* content) {
    if (!uam) return FALSE;
    
    uam->total_checks++;
    
    // Reset decision
    uam->current.should_block = FALSE;
    uam->current.should_clarify = FALSE;
    uam->current.reduce_precision = FALSE;
    uam->current.confidence = 0.8f;
    
    // Detect zone
    uam->current.zone = uam_detect_zone(uam, content);
    
    switch (uam->current.zone) {
        case ZONE_FORBIDDEN:
            uam->current.should_block = TRUE;
            uam->current.block_reason = BLOCK_HARMFUL;
            uam_strcpy(uam->current.detection_reason, "Forbidden content detected", 128);
            uam->blocks_applied++;
            break;
            
        case ZONE_SENSITIVE:
            uam->current.reduce_precision = TRUE;
            uam_strcpy(uam->current.detection_reason, "Sensitive topic", 128);
            uam->precision_reduced++;
            break;
            
        case ZONE_AMBIGUOUS:
            uam->current.should_clarify = TRUE;
            uam_strcpy(uam->current.detection_reason, "Ambiguous query", 128);
            uam->clarifications_requested++;
            break;
            
        case ZONE_SAFE:
        default:
            uam_strcpy(uam->current.detection_reason, "Safe content", 128);
            break;
    }
    
    return uam->current.should_block;
}

/**
 * Should request clarification
 */
BOOLEAN uam_should_clarify(UAMContext* uam) {
    if (!uam) return FALSE;
    return uam->current.should_clarify;
}

/**
 * Should reduce precision
 */
BOOLEAN uam_should_reduce_precision(UAMContext* uam) {
    if (!uam) return FALSE;
    return uam->current.reduce_precision;
}

/**
 * Get moderation decision
 */
ModerationDecision* uam_get_decision(UAMContext* uam) {
    if (!uam) return NULL;
    return &uam->current;
}

/**
 * Print moderation report
 */
void uam_print_report(UAMContext* uam) {
    if (!uam) return;
    
    Print(L"\n[UAM] Auto-Moderation Report\n");
    Print(L"  Total checks: %u\n", uam->total_checks);
    Print(L"  Blocks applied: %u\n", uam->blocks_applied);
    Print(L"  Clarifications requested: %u\n", uam->clarifications_requested);
    Print(L"  Precision reductions: %u\n", uam->precision_reduced);
    
    const CHAR16* zone_str = L"?";
    switch (uam->current.zone) {
        case ZONE_SAFE: zone_str = L"SAFE"; break;
        case ZONE_SENSITIVE: zone_str = L"SENSITIVE"; break;
        case ZONE_FORBIDDEN: zone_str = L"FORBIDDEN"; break;
        case ZONE_AMBIGUOUS: zone_str = L"AMBIGUOUS"; break;
    }
    Print(L"  Current zone: %s\n", zone_str);
    
    if (uam->current.should_block) {
        Print(L"  ⛔ BLOCKING OUTPUT\n");
    } else if (uam->current.should_clarify) {
        Print(L"  ❓ REQUESTING CLARIFICATION\n");
    } else if (uam->current.reduce_precision) {
        Print(L"  ⚠ REDUCING PRECISION\n");
    } else {
        Print(L"  ✓ Content allowed\n");
    }
}
