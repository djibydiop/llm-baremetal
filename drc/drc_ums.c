/*
 * UMS - Unité de Mémoire Sémantique Stable
 * Implementation
 */

#include "drc_ums.h"

// ═══════════════════════════════════════════════════════════════
// UMS IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

/**
 * Helper: String comparison
 */
static BOOLEAN ums_strcmp(const CHAR8* s1, const CHAR8* s2) {
    UINT32 i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return FALSE;
        i++;
    }
    return (s1[i] == s2[i]);
}

/**
 * Helper: String contains
 */
static BOOLEAN ums_contains(const CHAR8* haystack, const CHAR8* needle) {
    UINT32 h = 0;
    while (haystack[h] != '\0') {
        UINT32 n = 0;
        UINT32 start = h;
        while (needle[n] != '\0' && haystack[start] == needle[n]) {
            n++;
            start++;
        }
        if (needle[n] == '\0') return TRUE;
        h++;
    }
    return FALSE;
}

/**
 * Initialize UMS context
 */
EFI_STATUS ums_init(UMSContext* ums) {
    if (!ums) return EFI_INVALID_PARAMETER;
    
    ums->fact_count = 0;
    ums->total_facts_added = 0;
    ums->facts_validated = 0;
    ums->facts_rejected = 0;
    ums->hallucination_prevented = 0;
    
    ums->validation_threshold = 0.9f;
    ums->strict_mode = TRUE;
    
    return EFI_SUCCESS;
}

/**
 * Add fact to memory
 */
EFI_STATUS ums_add_fact(UMSContext* ums,
                        const CHAR8* content,
                        float confidence,
                        UINT32 source_id) {
    if (!ums || !content) return EFI_INVALID_PARAMETER;
    if (ums->fact_count >= 128) return EFI_OUT_OF_RESOURCES;
    
    // Check if confidence meets threshold
    if (ums->strict_mode && confidence < ums->validation_threshold) {
        ums->hallucination_prevented++;
        return EFI_ACCESS_DENIED;  // Rejected: too uncertain
    }
    
    // Check for contradictions
    if (ums_check_contradiction(ums, content)) {
        ums->hallucination_prevented++;
        return EFI_ACCESS_DENIED;  // Rejected: contradicts existing fact
    }
    
    SemanticFact* fact = &ums->facts[ums->fact_count];
    
    // Copy content
    UINT32 i = 0;
    while (i < 255 && content[i] != '\0') {
        fact->content[i] = content[i];
        i++;
    }
    fact->content[i] = '\0';
    
    // Set metadata
    fact->status = (confidence >= ums->validation_threshold) ? 
                   FACT_VALIDATED : FACT_HYPOTHESIS;
    fact->confidence = confidence;
    fact->timestamp = ums->total_facts_added;
    fact->validation_count = 0;
    fact->source_id = source_id;
    fact->immutable = FALSE;
    
    ums->fact_count++;
    ums->total_facts_added++;
    
    if (fact->status == FACT_VALIDATED) {
        ums->facts_validated++;
    }
    
    return EFI_SUCCESS;
}

/**
 * Validate fact
 */
EFI_STATUS ums_validate_fact(UMSContext* ums, UINT32 fact_id) {
    if (!ums || fact_id >= ums->fact_count) return EFI_INVALID_PARAMETER;
    
    SemanticFact* fact = &ums->facts[fact_id];
    
    if (fact->status == FACT_REJECTED) {
        return EFI_ACCESS_DENIED;  // Cannot validate rejected fact
    }
    
    fact->status = FACT_VALIDATED;
    fact->validation_count++;
    
    // After 3 validations, make immutable
    if (fact->validation_count >= 3) {
        fact->immutable = TRUE;
        fact->confidence = 1.0f;
    }
    
    ums->facts_validated++;
    
    return EFI_SUCCESS;
}

/**
 * Reject fact
 */
EFI_STATUS ums_reject_fact(UMSContext* ums, UINT32 fact_id) {
    if (!ums || fact_id >= ums->fact_count) return EFI_INVALID_PARAMETER;
    
    SemanticFact* fact = &ums->facts[fact_id];
    
    if (fact->immutable) {
        return EFI_ACCESS_DENIED;  // Cannot reject immutable fact
    }
    
    fact->status = FACT_REJECTED;
    fact->confidence = 0.0f;
    ums->facts_rejected++;
    
    return EFI_SUCCESS;
}

/**
 * Query fact by content
 */
SemanticFact* ums_query(UMSContext* ums, const CHAR8* query) {
    if (!ums || !query) return NULL;
    
    // Simple search: find fact containing query string
    for (UINT32 i = 0; i < ums->fact_count; i++) {
        SemanticFact* fact = &ums->facts[i];
        
        if (fact->status == FACT_VALIDATED || fact->status == FACT_HYPOTHESIS) {
            if (ums_contains(fact->content, query)) {
                return fact;
            }
        }
    }
    
    return NULL;
}

/**
 * Check if fact contradicts existing memory
 */
BOOLEAN ums_check_contradiction(UMSContext* ums, const CHAR8* new_fact) {
    if (!ums || !new_fact) return FALSE;
    
    // Simple heuristic: check for negation words in validated facts
    const CHAR8* negations[] = {"not", "no", "never", "false", NULL};
    
    for (UINT32 i = 0; i < ums->fact_count; i++) {
        SemanticFact* fact = &ums->facts[i];
        
        if (fact->status != FACT_VALIDATED) continue;
        
        // Check if new_fact contains negation of existing fact
        // Very simple check: if existing fact doesn't have "not" but new fact does
        BOOLEAN existing_has_not = ums_contains(fact->content, "not");
        BOOLEAN new_has_not = ums_contains(new_fact, "not");
        
        if (existing_has_not != new_has_not) {
            // Check if they're about the same topic (simple word overlap)
            // This is a stub - real implementation would use semantic similarity
            return TRUE;  // Potential contradiction
        }
    }
    
    return FALSE;
}

/**
 * Get validated facts count
 */
UINT32 ums_get_validated_count(UMSContext* ums) {
    if (!ums) return 0;
    
    UINT32 count = 0;
    for (UINT32 i = 0; i < ums->fact_count; i++) {
        if (ums->facts[i].status == FACT_VALIDATED) {
            count++;
        }
    }
    return count;
}

/**
 * Print memory report
 */
void ums_print_report(UMSContext* ums) {
    if (!ums) return;
    
    Print(L"\n[UMS] Semantic Memory Report\n");
    Print(L"  Total facts: %u\n", ums->fact_count);
    Print(L"  Validated: %u\n", ums_get_validated_count(ums));
    Print(L"  Rejected: %u\n", ums->facts_rejected);
    Print(L"  Hallucinations prevented: %u\n", ums->hallucination_prevented);
    Print(L"  Validation threshold: %.2f\n", ums->validation_threshold);
    
    if (ums->fact_count > 0 && ums->fact_count <= 8) {
        Print(L"  Recent facts:\n");
        for (UINT32 i = 0; i < ums->fact_count; i++) {
            SemanticFact* fact = &ums->facts[i];
            
            const CHAR16* status_str = L"?";
            switch (fact->status) {
                case FACT_VALIDATED: status_str = L"✓"; break;
                case FACT_HYPOTHESIS: status_str = L"?"; break;
                case FACT_REJECTED: status_str = L"✗"; break;
                case FACT_UNCERTAIN: status_str = L"~"; break;
            }
            
            Print(L"    [%s] %a (conf=%.2f)%s\n",
                  status_str, fact->content, fact->confidence,
                  fact->immutable ? L" [LOCKED]" : L"");
        }
    }
    
    if (ums->strict_mode) {
        Print(L"  Mode: STRICT (high confidence required)\n");
    } else {
        Print(L"  Mode: PERMISSIVE\n");
    }
}
