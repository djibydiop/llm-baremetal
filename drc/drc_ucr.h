/*
 * UCR - Unité de Confiance et de Risque
 * Décider si une réponse est acceptable
 */

#ifndef DRC_UCR_H
#define DRC_UCR_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// UCR - RISK ASSESSMENT
// ═══════════════════════════════════════════════════════════════

typedef enum {
    RISK_NONE,
    RISK_LOW,           // Acceptable
    RISK_MEDIUM,        // Avertissement
    RISK_HIGH,          // Refuser sauf override
    RISK_CRITICAL       // Refuser absolument
} RiskLevel;

typedef enum {
    IMPACT_NONE,
    IMPACT_COSMETIC,    // Erreur de formulation
    IMPACT_LOW,         // Information incorrecte mineure
    IMPACT_MEDIUM,      // Désinformation possible
    IMPACT_HIGH,        // Risque sécurité/sûreté
    IMPACT_CRITICAL     // Danger immédiat
} ImpactLevel;

typedef enum {
    DECISION_ACCEPT,        // Réponse OK
    DECISION_WARN,          // Réponse avec avertissement
    DECISION_REFUSE,        // Refuser de répondre
    DECISION_ASK_MORE       // Demander clarification
} RiskDecision;

typedef struct {
    RiskLevel probability;      // Probabilité d'erreur
    ImpactLevel impact;         // Gravité si erreur
    RiskDecision decision;      // Décision finale
    
    CHAR8 reason[128];
    float confidence_score;     // Score de confiance global
    BOOLEAN safe_to_output;
    
    // Facteurs de risque détectés
    BOOLEAN low_confidence;
    BOOLEAN high_incoherence;
    BOOLEAN domain_mismatch;
    BOOLEAN temporal_issue;
} RiskAssessment;

typedef struct {
    RiskAssessment current;
    
    // Statistiques
    UINT32 total_assessments;
    UINT32 accepted;
    UINT32 refused;
    UINT32 warnings;
    
    // Seuils configurables
    float min_confidence;       // 0.7 par défaut
    float max_incoherence;      // 0.3 par défaut
    BOOLEAN paranoid_mode;      // TRUE = ultra-strict
} UCRContext;

// ═══════════════════════════════════════════════════════════════
// UCR FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UCR context
 */
EFI_STATUS ucr_init(UCRContext* ucr);

/**
 * Assess risk of current reasoning
 * Called AFTER UIC, BEFORE output
 */
EFI_STATUS ucr_assess_risk(UCRContext* ucr, 
                            float urs_confidence,
                            float uic_coherence,
                            UINT32 verification_failures);

/**
 * Make final decision
 */
RiskDecision ucr_decide(UCRContext* ucr);

/**
 * Check if output is safe
 */
BOOLEAN ucr_is_safe(UCRContext* ucr);

/**
 * Print risk report
 */
void ucr_print_report(UCRContext* ucr);

#endif // DRC_UCR_H
