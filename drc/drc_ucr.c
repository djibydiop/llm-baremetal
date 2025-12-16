/*
 * UCR - Unité de Confiance et de Risque
 * Implementation
 */

#include "drc_ucr.h"

// ═══════════════════════════════════════════════════════════════
// UCR IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UCR context
 */
EFI_STATUS ucr_init(UCRContext* ucr) {
    if (!ucr) return EFI_INVALID_PARAMETER;
    
    ucr->current.probability = RISK_NONE;
    ucr->current.impact = IMPACT_NONE;
    ucr->current.decision = DECISION_ACCEPT;
    ucr->current.confidence_score = 1.0f;
    ucr->current.safe_to_output = TRUE;
    ucr->current.low_confidence = FALSE;
    ucr->current.high_incoherence = FALSE;
    ucr->current.domain_mismatch = FALSE;
    ucr->current.temporal_issue = FALSE;
    
    ucr->total_assessments = 0;
    ucr->accepted = 0;
    ucr->refused = 0;
    ucr->warnings = 0;
    
    ucr->min_confidence = 0.7f;
    ucr->max_incoherence = 0.3f;
    ucr->paranoid_mode = FALSE;
    
    return EFI_SUCCESS;
}

/**
 * Assess risk based on multiple factors
 */
EFI_STATUS ucr_assess_risk(UCRContext* ucr,
                            float urs_confidence,
                            float uic_coherence,
                            UINT32 verification_failures) {
    if (!ucr) return EFI_INVALID_PARAMETER;
    
    ucr->total_assessments++;
    RiskAssessment* risk = &ucr->current;
    
    // Reset
    risk->low_confidence = FALSE;
    risk->high_incoherence = FALSE;
    risk->domain_mismatch = FALSE;
    risk->temporal_issue = FALSE;
    
    // Factor 1: URS confidence
    if (urs_confidence < ucr->min_confidence) {
        risk->low_confidence = TRUE;
        risk->probability = RISK_MEDIUM;
    } else {
        risk->probability = RISK_LOW;
    }
    
    // Factor 2: UIC coherence
    float incoherence = 1.0f - uic_coherence;
    if (incoherence > ucr->max_incoherence) {
        risk->high_incoherence = TRUE;
        if (risk->probability < RISK_HIGH) {
            risk->probability = RISK_HIGH;
        }
    }
    
    // Factor 3: Verification failures
    if (verification_failures > 3) {
        risk->domain_mismatch = TRUE;
        risk->probability = RISK_CRITICAL;
    }
    
    // Determine impact
    if (risk->low_confidence && risk->high_incoherence) {
        risk->impact = IMPACT_HIGH;
    } else if (risk->low_confidence || risk->high_incoherence) {
        risk->impact = IMPACT_MEDIUM;
    } else {
        risk->impact = IMPACT_LOW;
    }
    
    // Calculate confidence score
    risk->confidence_score = urs_confidence * uic_coherence;
    if (verification_failures > 0) {
        risk->confidence_score *= (1.0f - (verification_failures * 0.1f));
    }
    if (risk->confidence_score < 0.0f) risk->confidence_score = 0.0f;
    
    // Make decision
    if (risk->probability >= RISK_CRITICAL || risk->impact >= IMPACT_CRITICAL) {
        risk->decision = DECISION_REFUSE;
        risk->safe_to_output = FALSE;
        ucr->refused++;
    } else if (risk->probability >= RISK_HIGH || risk->impact >= IMPACT_HIGH) {
        if (ucr->paranoid_mode) {
            risk->decision = DECISION_REFUSE;
            risk->safe_to_output = FALSE;
            ucr->refused++;
        } else {
            risk->decision = DECISION_WARN;
            risk->safe_to_output = TRUE;
            ucr->warnings++;
        }
    } else if (risk->probability >= RISK_MEDIUM) {
        risk->decision = DECISION_WARN;
        risk->safe_to_output = TRUE;
        ucr->warnings++;
    } else {
        risk->decision = DECISION_ACCEPT;
        risk->safe_to_output = TRUE;
        ucr->accepted++;
    }
    
    // Build reason string
    CHAR8* reason = risk->reason;
    UINT32 pos = 0;
    
    if (risk->low_confidence) {
        const CHAR8* msg = "Low confidence. ";
        for (UINT32 i = 0; msg[i] != '\0' && pos < 127; i++) {
            reason[pos++] = msg[i];
        }
    }
    if (risk->high_incoherence) {
        const CHAR8* msg = "High incoherence. ";
        for (UINT32 i = 0; msg[i] != '\0' && pos < 127; i++) {
            reason[pos++] = msg[i];
        }
    }
    if (verification_failures > 0) {
        const CHAR8* msg = "Verification failures. ";
        for (UINT32 i = 0; msg[i] != '\0' && pos < 127; i++) {
            reason[pos++] = msg[i];
        }
    }
    if (pos == 0) {
        const CHAR8* msg = "All checks passed.";
        for (UINT32 i = 0; msg[i] != '\0' && pos < 127; i++) {
            reason[pos++] = msg[i];
        }
    }
    reason[pos] = '\0';
    
    return EFI_SUCCESS;
}

/**
 * Make final decision
 */
RiskDecision ucr_decide(UCRContext* ucr) {
    if (!ucr) return DECISION_REFUSE;
    return ucr->current.decision;
}

/**
 * Check if output is safe
 */
BOOLEAN ucr_is_safe(UCRContext* ucr) {
    if (!ucr) return FALSE;
    return ucr->current.safe_to_output;
}

/**
 * Print risk report
 */
void ucr_print_report(UCRContext* ucr) {
    if (!ucr) return;
    
    Print(L"\n[UCR] Risk Assessment Report\n");
    Print(L"  Total assessments: %u\n", ucr->total_assessments);
    Print(L"  Accepted: %u, Warned: %u, Refused: %u\n",
          ucr->accepted, ucr->warnings, ucr->refused);
    
    RiskAssessment* risk = &ucr->current;
    
    const CHAR16* risk_str = L"NONE";
    switch (risk->probability) {
        case RISK_LOW: risk_str = L"LOW"; break;
        case RISK_MEDIUM: risk_str = L"MEDIUM"; break;
        case RISK_HIGH: risk_str = L"HIGH"; break;
        case RISK_CRITICAL: risk_str = L"CRITICAL"; break;
        default: break;
    }
    
    const CHAR16* impact_str = L"NONE";
    switch (risk->impact) {
        case IMPACT_COSMETIC: impact_str = L"COSMETIC"; break;
        case IMPACT_LOW: impact_str = L"LOW"; break;
        case IMPACT_MEDIUM: impact_str = L"MEDIUM"; break;
        case IMPACT_HIGH: impact_str = L"HIGH"; break;
        case IMPACT_CRITICAL: impact_str = L"CRITICAL"; break;
        default: break;
    }
    
    const CHAR16* decision_str = L"?";
    switch (risk->decision) {
        case DECISION_ACCEPT: decision_str = L"ACCEPT"; break;
        case DECISION_WARN: decision_str = L"WARN"; break;
        case DECISION_REFUSE: decision_str = L"REFUSE"; break;
        case DECISION_ASK_MORE: decision_str = L"ASK_MORE"; break;
    }
    
    Print(L"  Current Risk: %s\n", risk_str);
    Print(L"  Impact: %s\n", impact_str);
    Print(L"  Decision: %s\n", decision_str);
    Print(L"  Confidence: %.2f\n", risk->confidence_score);
    Print(L"  Reason: %a\n", risk->reason);
    
    if (risk->safe_to_output) {
        Print(L"  ✓ Safe to output\n");
    } else {
        Print(L"  ⛔ NOT safe to output\n");
    }
}
