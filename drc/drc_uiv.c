/*
 * UIV - Unité d'Intention et de Valeurs Implementation
 */

#include "drc_uiv.h"

// ═══════════════════════════════════════════════════════════════
// UIV IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

/**
 * Helper: String copy
 */
static void uiv_strcpy(CHAR8* dest, const CHAR8* src, UINT32 max_len) {
    UINT32 i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * Initialize UIV with core values
 */
EFI_STATUS uiv_init(UIVContext* uiv) {
    if (!uiv) return EFI_INVALID_PARAMETER;
    
    uiv->objective_count = 0;
    uiv->value_count = 0;
    uiv->conflicts_detected = 0;
    uiv->conflicts_resolved = 0;
    uiv->alignment_score = 1.0f;
    uiv->aligned = TRUE;
    
    // Initialize core values
    uiv_add_value(uiv, VALUE_SAFETY, 1.0f);        // Highest priority
    uiv_add_value(uiv, VALUE_TRUTHFULNESS, 0.9f);
    uiv_add_value(uiv, VALUE_HELPFULNESS, 0.8f);
    uiv_add_value(uiv, VALUE_RESPECT, 0.85f);
    uiv_add_value(uiv, VALUE_TRANSPARENCY, 0.75f);
    
    return EFI_SUCCESS;
}

/**
 * Add objective
 */
EFI_STATUS uiv_add_objective(UIVContext* uiv,
                             const CHAR8* description,
                             ObjectivePriority priority) {
    if (!uiv || !description || uiv->objective_count >= 8) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    Objective* obj = &uiv->objectives[uiv->objective_count];
    uiv_strcpy(obj->description, description, 64);
    obj->priority = priority;
    obj->achieved = FALSE;
    obj->completion = 0.0f;
    
    uiv->objective_count++;
    return EFI_SUCCESS;
}

/**
 * Add value constraint
 */
EFI_STATUS uiv_add_value(UIVContext* uiv, CoreValue value, float weight) {
    if (!uiv || uiv->value_count >= 5) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    ValueConstraint* val = &uiv->values[uiv->value_count];
    val->value = value;
    val->weight = weight;
    val->violated = FALSE;
    
    uiv->value_count++;
    return EFI_SUCCESS;
}

/**
 * Check if action aligns with values
 */
BOOLEAN uiv_check_alignment(UIVContext* uiv, const CHAR8* action) {
    if (!uiv || !action) return FALSE;
    
    // Simple heuristic: check for alignment keywords
    // Real implementation would use semantic analysis
    
    // Check safety
    const CHAR8* unsafe[] = {"harm", "damage", "destroy", "attack", NULL};
    for (UINT32 i = 0; unsafe[i] != NULL; i++) {
        UINT32 j = 0;
        while (action[j] != '\0') {
            UINT32 k = 0;
            while (unsafe[i][k] != '\0' && action[j + k] == unsafe[i][k]) k++;
            if (unsafe[i][k] == '\0') {
                // Found unsafe keyword
                for (UINT32 v = 0; v < uiv->value_count; v++) {
                    if (uiv->values[v].value == VALUE_SAFETY) {
                        uiv->values[v].violated = TRUE;
                    }
                }
                return FALSE;  // Not aligned
            }
            j++;
        }
    }
    
    // Check truthfulness
    const CHAR8* deceptive[] = {"lie", "deceive", "fake", "mislead", NULL};
    for (UINT32 i = 0; deceptive[i] != NULL; i++) {
        UINT32 j = 0;
        while (action[j] != '\0') {
            UINT32 k = 0;
            while (deceptive[i][k] != '\0' && action[j + k] == deceptive[i][k]) k++;
            if (deceptive[i][k] == '\0') {
                for (UINT32 v = 0; v < uiv->value_count; v++) {
                    if (uiv->values[v].value == VALUE_TRUTHFULNESS) {
                        uiv->values[v].violated = TRUE;
                    }
                }
                return FALSE;
            }
            j++;
        }
    }
    
    return TRUE;  // Aligned
}

/**
 * Resolve conflict (higher priority wins)
 */
ObjectivePriority uiv_resolve_conflict(UIVContext* uiv,
                                        ObjectivePriority obj1,
                                        ObjectivePriority obj2) {
    if (!uiv) return PRIORITY_LOW;
    
    uiv->conflicts_detected++;
    
    // Higher priority (lower enum value) wins
    if (obj1 < obj2) {
        uiv->conflicts_resolved++;
        return obj1;
    } else {
        uiv->conflicts_resolved++;
        return obj2;
    }
}

/**
 * Calculate alignment score
 */
float uiv_calculate_alignment(UIVContext* uiv) {
    if (!uiv || uiv->value_count == 0) return 1.0f;
    
    float total_weight = 0.0f;
    float violated_weight = 0.0f;
    
    for (UINT32 i = 0; i < uiv->value_count; i++) {
        total_weight += uiv->values[i].weight;
        if (uiv->values[i].violated) {
            violated_weight += uiv->values[i].weight;
        }
    }
    
    if (total_weight == 0.0f) return 1.0f;
    
    uiv->alignment_score = 1.0f - (violated_weight / total_weight);
    uiv->aligned = (uiv->alignment_score >= 0.7f);
    
    return uiv->alignment_score;
}

/**
 * Get highest priority objective
 */
Objective* uiv_get_top_objective(UIVContext* uiv) {
    if (!uiv || uiv->objective_count == 0) return NULL;
    
    Objective* top = &uiv->objectives[0];
    for (UINT32 i = 1; i < uiv->objective_count; i++) {
        if (uiv->objectives[i].priority < top->priority) {
            top = &uiv->objectives[i];
        }
    }
    
    return top;
}

/**
 * Print values report
 */
void uiv_print_report(UIVContext* uiv) {
    if (!uiv) return;
    
    Print(L"\n[UIV] Intention & Values Report\n");
    Print(L"  Objectives: %u\n", uiv->objective_count);
    Print(L"  Values: %u\n", uiv->value_count);
    Print(L"  Conflicts detected: %u\n", uiv->conflicts_detected);
    Print(L"  Conflicts resolved: %u\n", uiv->conflicts_resolved);
    Print(L"  Alignment score: %.2f\n", uiv->alignment_score);
    
    if (uiv->objective_count > 0) {
        Print(L"\n  Top objectives:\n");
        for (UINT32 i = 0; i < uiv->objective_count && i < 3; i++) {
            Objective* obj = &uiv->objectives[i];
            const CHAR16* prio_str = L"?";
            switch (obj->priority) {
                case PRIORITY_CRITICAL: prio_str = L"CRITICAL"; break;
                case PRIORITY_HIGH: prio_str = L"HIGH"; break;
                case PRIORITY_MEDIUM: prio_str = L"MEDIUM"; break;
                case PRIORITY_LOW: prio_str = L"LOW"; break;
            }
            Print(L"    [%s] %a (%.0f%%)\n", prio_str, obj->description, obj->completion * 100.0f);
        }
    }
    
    if (uiv->value_count > 0) {
        Print(L"\n  Values:\n");
        for (UINT32 i = 0; i < uiv->value_count; i++) {
            ValueConstraint* val = &uiv->values[i];
            const CHAR16* val_str = L"?";
            switch (val->value) {
                case VALUE_SAFETY: val_str = L"SAFETY"; break;
                case VALUE_TRUTHFULNESS: val_str = L"TRUTH"; break;
                case VALUE_HELPFULNESS: val_str = L"HELP"; break;
                case VALUE_RESPECT: val_str = L"RESPECT"; break;
                case VALUE_TRANSPARENCY: val_str = L"TRANS"; break;
            }
            Print(L"    [%s] Weight: %.2f %s\n", 
                  val_str, val->weight, val->violated ? L"⛔" : L"✓");
        }
    }
    
    if (uiv->aligned) {
        Print(L"  ✓ Actions aligned with values\n");
    } else {
        Print(L"  ⚠ Value violations detected\n");
    }
}
