/*
 * UCO - Unité de Contre-Raisonnement Sophistiquée (Phase 5 CWEB)
 * Validation dialectique + Existence cognitive
 */

#ifndef DRC_UCO_H
#define DRC_UCO_H

#include <efi.h>
#include <efilib.h>
#include "drc_urs.h"

// ═══════════════════════════════════════════════════════════════
// UCO - SOPHISTICATED COUNTER-REASONING (Phase 5)
// ═══════════════════════════════════════════════════════════════

// Attack types (extended)
typedef enum {
    ATTACK_ASSUMPTION,      // Attaquer une prémisse
    ATTACK_LOGIC,           // Attaquer le raisonnement
    ATTACK_CONCLUSION,      // Attaquer la conclusion
    ATTACK_COUNTEREXAMPLE,  // Trouver contre-exemple
    ATTACK_EXISTENCE,       // Remettre en question l'existence même
    ATTACK_COHERENCE,       // Attaquer la cohérence globale
    ATTACK_CONTEXT,         // Invalider le contexte
    ATTACK_ADVERSARIAL      // Attaque adversariale systématique
} AttackType;

typedef struct {
    AttackType type;
    UINT32 target_node;     // Nœud attaqué
    CHAR8 argument[128];    // Argument de l'attaque
    float strength;         // Force de l'attaque (0.0-1.0)
    BOOLEAN successful;     // TRUE si attaque valide
} CounterArgument;

// Dialectic reasoning (thesis → antithesis → synthesis)
typedef struct {
    CHAR8 thesis[256];
    CHAR8 antithesis[256];
    CHAR8 synthesis[256];
    float confidence;
    BOOLEAN synthesis_valid;
} DialecticTriad;

// Adversarial pattern
typedef struct {
    CHAR8 pattern_name[64];
    AttackType type;
    float severity;
    UINT32 detection_count;
} AdversarialPattern;

typedef struct {
    SolutionPath original_path;
    CounterArgument attacks[16];
    UINT32 attack_count;
    
    // Phase 5: Dialectic reasoning
    DialecticTriad dialectic;
    BOOLEAN dialectic_enabled;
    
    // Phase 5: Adversarial patterns
    AdversarialPattern patterns[8];
    UINT32 pattern_count;
    
    // Phase 5: Existence validation (CWEB)
    BOOLEAN context_valid;
    BOOLEAN existence_justified;
    float coherence_global;
    
    // Résultats
    BOOLEAN path_survived;  // TRUE si résiste aux attaques
    float robustness_score; // Score de robustesse
    UINT32 weaknesses_found;
    
    // Statistiques
    UINT32 total_attacks_generated;
    UINT32 successful_attacks;
    UINT32 dialectic_syntheses;
} UCOContext;

// ═══════════════════════════════════════════════════════════════
// UCO FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UCO context
 */
EFI_STATUS uco_init(UCOContext* uco);

/**
 * Attack solution path (méthode scientifique)
 * Génère des contre-arguments pour tester robustesse
 */
EFI_STATUS uco_attack_path(UCOContext* uco, SolutionPath* path);

/**
 * Generate counter-examples
 */
EFI_STATUS uco_generate_counterexamples(UCOContext* uco);

/**
 * Attack assumptions
 */
EFI_STATUS uco_attack_assumptions(UCOContext* uco);

/**
 * Attack logical steps
 */
EFI_STATUS uco_attack_logic(UCOContext* uco);

/**
 * Calculate robustness score
 */
float uco_calculate_robustness(UCOContext* uco);

/**
 * Check if path survived attacks
 */
BOOLEAN uco_path_survived(UCOContext* uco);
// ═══════════════════════════════════════════════════════════════
// PHASE 5: SOPHISTICATED REASONING
// ═══════════════════════════════════════════════════════════════

/**
 * Dialectic reasoning: thesis → antithesis → synthesis
 */
EFI_STATUS uco_dialectic_reason(UCOContext* uco, const CHAR8* thesis);

/**
 * Attack existence itself (CWEB validation)
 */
BOOLEAN uco_validate_existence(UCOContext* uco, const CHAR8* context);

/**
 * Add adversarial pattern
 */
void uco_add_adversarial_pattern(UCOContext* uco, 
                                 const CHAR8* pattern,
                                 AttackType type,
                                 float severity);

/**
 * Systematic adversarial attack
 */
EFI_STATUS uco_adversarial_attack(UCOContext* uco);

/**
 * Validate global coherence
 */
float uco_validate_coherence(UCOContext* uco);

/**
 * Devil's advocate (extreme mode)
 */
EFI_STATUS uco_devils_advocate(UCOContext* uco, const CHAR8* claim);
/**
 * Print counter-reasoning report
 */
void uco_print_report(UCOContext* uco);

#endif // DRC_UCO_H
