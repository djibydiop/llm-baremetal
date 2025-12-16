/*
 * DRC Bias Detection Implementation
 */

#include "drc_bias.h"

// Helper: String comparison (case-insensitive)
static BOOLEAN str_contains(const CHAR8* text, const CHAR8* pattern) {
    if (!text || !pattern) return FALSE;
    
    UINT32 text_len = 0, pattern_len = 0;
    while (text[text_len] != '\0') text_len++;
    while (pattern[pattern_len] != '\0') pattern_len++;
    
    if (pattern_len > text_len) return FALSE;
    
    for (UINT32 i = 0; i <= text_len - pattern_len; i++) {
        BOOLEAN match = TRUE;
        for (UINT32 j = 0; j < pattern_len; j++) {
            CHAR8 t = text[i + j];
            CHAR8 p = pattern[j];
            // Simple lowercase
            if (t >= 'A' && t <= 'Z') t += 32;
            if (p >= 'A' && p <= 'Z') p += 32;
            if (t != p) {
                match = FALSE;
                break;
            }
        }
        if (match) return TRUE;
    }
    return FALSE;
}

/**
 * Initialize bias detection
 */
EFI_STATUS bias_init(BiasContext* ctx) {
    if (!ctx) return EFI_INVALID_PARAMETER;
    
    // Clear
    for (UINT32 i = 0; i < sizeof(BiasContext); i++) {
        ((UINT8*)ctx)[i] = 0;
    }
    
    ctx->detection_enabled = TRUE;
    ctx->auto_correct = FALSE;  // Manual review by default
    ctx->alert_threshold = BIAS_SEVERITY_MEDIUM;
    ctx->fairness_score = 1.0f;
    
    // Add common bias patterns
    bias_add_pattern(ctx, "she should", BIAS_GENDER, BIAS_SEVERITY_MEDIUM);
    bias_add_pattern(ctx, "he should", BIAS_GENDER, BIAS_SEVERITY_MEDIUM);
    bias_add_pattern(ctx, "women are", BIAS_GENDER, BIAS_SEVERITY_HIGH);
    bias_add_pattern(ctx, "men are", BIAS_GENDER, BIAS_SEVERITY_HIGH);
    bias_add_pattern(ctx, "too old", BIAS_AGE, BIAS_SEVERITY_MEDIUM);
    bias_add_pattern(ctx, "too young", BIAS_AGE, BIAS_SEVERITY_MEDIUM);
    
    return EFI_SUCCESS;
}

/**
 * Add bias pattern
 */
EFI_STATUS bias_add_pattern(BiasContext* ctx,
                            const CHAR8* pattern,
                            BiasType type,
                            BiasSeverity severity) {
    if (!ctx || !pattern || ctx->pattern_count >= MAX_BIAS_PATTERNS) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    BiasPattern* p = &ctx->patterns[ctx->pattern_count++];
    
    // Copy pattern
    UINT32 i = 0;
    while (i < 63 && pattern[i] != '\0') {
        p->pattern[i] = pattern[i];
        i++;
    }
    p->pattern[i] = '\0';
    
    p->type = type;
    p->severity = severity;
    p->confidence = 0.8f;
    
    return EFI_SUCCESS;
}

/**
 * Check text for bias
 */
BiasSeverity bias_check_text(BiasContext* ctx,
                             const CHAR8* text,
                             UINT32 token_pos) {
    if (!ctx || !text || !ctx->detection_enabled) return BIAS_SEVERITY_NONE;
    
    ctx->total_checks++;
    BiasSeverity max_severity = BIAS_SEVERITY_NONE;
    
    // Check against all patterns
    for (UINT32 i = 0; i < ctx->pattern_count; i++) {
        BiasPattern* pattern = &ctx->patterns[i];
        
        if (str_contains(text, pattern->pattern)) {
            // Bias detected!
            ctx->biases_detected++;
            
            if (pattern->severity > max_severity) {
                max_severity = pattern->severity;
            }
            
            // Create alert if severe enough
            if (pattern->severity >= ctx->alert_threshold &&
                ctx->alert_count < MAX_BIAS_ALERTS) {
                
                BiasAlert* alert = &ctx->alerts[ctx->alert_count++];
                alert->type = pattern->type;
                alert->severity = pattern->severity;
                alert->token_position = token_pos;
                alert->score = pattern->confidence;
                
                // Copy detected text
                UINT32 j = 0;
                while (j < 127 && text[j] != '\0') {
                    alert->detected_text[j] = text[j];
                    j++;
                }
                alert->detected_text[j] = '\0';
                
                // Get suggestion
                const CHAR8* suggestion = bias_get_suggestion(ctx, pattern->type);
                j = 0;
                while (j < 127 && suggestion[j] != '\0') {
                    alert->suggestion[j] = suggestion[j];
                    j++;
                }
                alert->suggestion[j] = '\0';
            }
        }
    }
    
    // Update fairness score
    if (ctx->biases_detected > 0) {
        float penalty = (float)ctx->biases_detected / (float)(ctx->total_checks + 1);
        ctx->fairness_score = 1.0f - penalty;
        if (ctx->fairness_score < 0.0f) ctx->fairness_score = 0.0f;
    }
    
    return max_severity;
}

/**
 * Check for gender bias
 */
BOOLEAN bias_check_gender(BiasContext* ctx, const CHAR8* text) {
    if (!ctx || !text) return FALSE;
    
    const CHAR8* patterns[] = {
        "women are", "men are", "she should", "he should",
        "girls should", "boys should", "feminine", "masculine"
    };
    
    for (UINT32 i = 0; i < 8; i++) {
        if (str_contains(text, patterns[i])) {
            return TRUE;
        }
    }
    
    return FALSE;
}

/**
 * Check for racial bias
 */
BOOLEAN bias_check_racial(BiasContext* ctx, const CHAR8* text) {
    if (!ctx || !text) return FALSE;
    
    // Check for potentially problematic patterns
    // (This is simplified; real implementation would be more sophisticated)
    const CHAR8* patterns[] = {
        "those people", "their culture", "all of them"
    };
    
    for (UINT32 i = 0; i < 3; i++) {
        if (str_contains(text, patterns[i])) {
            return TRUE;
        }
    }
    
    return FALSE;
}

/**
 * Check for political bias
 */
BOOLEAN bias_check_political(BiasContext* ctx, const CHAR8* text) {
    if (!ctx || !text) return FALSE;
    
    const CHAR8* patterns[] = {
        "obviously", "clearly wrong", "anyone can see",
        "only idiots", "brainwashed"
    };
    
    for (UINT32 i = 0; i < 5; i++) {
        if (str_contains(text, patterns[i])) {
            return TRUE;
        }
    }
    
    return FALSE;
}

/**
 * Add perspective
 */
void bias_add_perspective(BiasContext* ctx, const CHAR8* perspective) {
    if (!ctx || !perspective || ctx->perspective_count >= MAX_PERSPECTIVES) {
        return;
    }
    
    // Check if already exists
    for (UINT32 i = 0; i < ctx->perspective_count; i++) {
        UINT32 j = 0;
        while (ctx->perspectives[i].perspective[j] == perspective[j] && perspective[j] != '\0') j++;
        if (ctx->perspectives[i].perspective[j] == perspective[j]) {
            // Already exists, increment count
            ctx->perspectives[i].representation_count++;
            return;
        }
    }
    
    // Add new perspective
    Perspective* p = &ctx->perspectives[ctx->perspective_count++];
    UINT32 i = 0;
    while (i < 63 && perspective[i] != '\0') {
        p->perspective[i] = perspective[i];
        i++;
    }
    p->perspective[i] = '\0';
    p->representation_count = 1;
}

/**
 * Calculate perspective balance
 */
BalanceLevel bias_calculate_balance(BiasContext* ctx) {
    if (!ctx || ctx->perspective_count == 0) return BALANCE_NONE;
    
    // Calculate proportions
    UINT32 total = 0;
    for (UINT32 i = 0; i < ctx->perspective_count; i++) {
        total += ctx->perspectives[i].representation_count;
    }
    
    if (total == 0) return BALANCE_NONE;
    
    for (UINT32 i = 0; i < ctx->perspective_count; i++) {
        ctx->perspectives[i].proportion = 
            (float)ctx->perspectives[i].representation_count / (float)total;
    }
    
    // Determine balance level
    if (ctx->perspective_count >= 4) {
        ctx->balance_level = BALANCE_DIVERSE;
    } else if (ctx->perspective_count >= 2) {
        // Check if balanced (no single perspective > 60%)
        float max_prop = 0.0f;
        for (UINT32 i = 0; i < ctx->perspective_count; i++) {
            if (ctx->perspectives[i].proportion > max_prop) {
                max_prop = ctx->perspectives[i].proportion;
            }
        }
        ctx->balance_level = (max_prop < 0.6f) ? BALANCE_FAIR : BALANCE_PARTIAL;
    } else {
        ctx->balance_level = BALANCE_PARTIAL;
    }
    
    return ctx->balance_level;
}

/**
 * Get bias suggestion
 */
const CHAR8* bias_get_suggestion(const BiasContext* ctx, BiasType type) {
    switch (type) {
        case BIAS_GENDER:
            return "Consider using gender-neutral language";
        case BIAS_RACIAL:
            return "Avoid generalizations about groups";
        case BIAS_AGE:
            return "Age is not indicative of ability";
        case BIAS_POLITICAL:
            return "Present multiple viewpoints fairly";
        case BIAS_RELIGIOUS:
            return "Respect diverse belief systems";
        case BIAS_SOCIOECONOMIC:
            return "Avoid class-based assumptions";
        case BIAS_CULTURAL:
            return "Cultural practices vary widely";
        case BIAS_ABILITY:
            return "Use person-first language";
        default:
            return "Review for potential bias";
    }
}

/**
 * Calculate fairness score
 */
float bias_calculate_fairness(const BiasContext* ctx) {
    if (!ctx) return 0.0f;
    return ctx->fairness_score;
}

/**
 * Print bias report
 */
void bias_print_report(const BiasContext* ctx) {
    if (!ctx) return;
    
    const CHAR16* severity_names[] = {L"NONE", L"LOW", L"MEDIUM", L"HIGH", L"CRITICAL"};
    const CHAR16* balance_names[] = {L"NONE", L"PARTIAL", L"FAIR", L"DIVERSE"};
    
    Print(L"\r\n═══════════════════════════════════════════════════════════\r\n");
    Print(L"  BIAS DETECTION REPORT\r\n");
    Print(L"═══════════════════════════════════════════════════════════\r\n");
    
    Print(L"  Total Checks:       %d\r\n", ctx->total_checks);
    Print(L"  Biases Detected:    %d\r\n", ctx->biases_detected);
    Print(L"  Biases Corrected:   %d\r\n", ctx->biases_corrected);
    Print(L"  Fairness Score:     %.2f / 1.0\r\n", ctx->fairness_score);
    Print(L"  Balance Level:      %s\r\n", balance_names[ctx->balance_level]);
    Print(L"\r\n");
    
    if (ctx->alert_count > 0) {
        Print(L"  Bias Alerts:\r\n");
        for (UINT32 i = 0; i < ctx->alert_count && i < 5; i++) {
            const BiasAlert* alert = &ctx->alerts[i];
            Print(L"    [%s] Token %d: \"%a\"\r\n",
                  severity_names[alert->severity],
                  alert->token_position,
                  alert->detected_text);
            Print(L"      → %a\r\n", alert->suggestion);
        }
    }
    
    if (ctx->perspective_count > 0) {
        Print(L"\r\n  Perspectives:\r\n");
        for (UINT32 i = 0; i < ctx->perspective_count; i++) {
            const Perspective* p = &ctx->perspectives[i];
            Print(L"    %a: %d occurrences (%.1f%%)\r\n",
                  p->perspective,
                  p->representation_count,
                  p->proportion * 100.0f);
        }
    }
    
    Print(L"═══════════════════════════════════════════════════════════\r\n");
}

/**
 * Get alert count by type
 */
UINT32 bias_get_alert_count(const BiasContext* ctx, BiasType type) {
    if (!ctx) return 0;
    
    UINT32 count = 0;
    for (UINT32 i = 0; i < ctx->alert_count; i++) {
        if (ctx->alerts[i].type == type) {
            count++;
        }
    }
    return count;
}
