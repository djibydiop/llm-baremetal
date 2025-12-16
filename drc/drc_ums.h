/*
 * UMS - Unité de Mémoire Sémantique Stable
 * Se souvenir sans halluciner
 */

#ifndef DRC_UMS_H
#define DRC_UMS_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// UMS - STABLE SEMANTIC MEMORY
// ═══════════════════════════════════════════════════════════════

typedef enum {
    FACT_VALIDATED,     // Fait vérifié et approuvé
    FACT_HYPOTHESIS,    // Hypothèse non validée
    FACT_REJECTED,      // Invalidé
    FACT_UNCERTAIN      // En attente de validation
} FactStatus;

typedef struct {
    CHAR8 content[256];
    FactStatus status;
    float confidence;       // 1.0 = certitude absolue
    UINT64 timestamp;       // Quand ajouté
    UINT32 validation_count; // Combien de fois validé
    UINT32 source_id;       // Origine (URS path, user input, etc.)
    BOOLEAN immutable;      // TRUE = ne peut plus changer
} SemanticFact;

typedef struct {
    SemanticFact facts[128];
    UINT32 fact_count;
    
    // Statistiques
    UINT32 total_facts_added;
    UINT32 facts_validated;
    UINT32 facts_rejected;
    UINT32 hallucination_prevented; // Tentatives d'ajout invalides bloquées
    
    // Configuration
    float validation_threshold;  // 0.9 = strict
    BOOLEAN strict_mode;         // TRUE = rejeter incertain
} UMSContext;

// ═══════════════════════════════════════════════════════════════
// UMS FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UMS context
 */
EFI_STATUS ums_init(UMSContext* ums);

/**
 * Add fact (must be validated by URS+UIC+UCR first)
 */
EFI_STATUS ums_add_fact(UMSContext* ums,
                        const CHAR8* content,
                        float confidence,
                        UINT32 source_id);

/**
 * Validate fact (promote HYPOTHESIS → VALIDATED)
 */
EFI_STATUS ums_validate_fact(UMSContext* ums, UINT32 fact_id);

/**
 * Reject fact (mark as REJECTED)
 */
EFI_STATUS ums_reject_fact(UMSContext* ums, UINT32 fact_id);

/**
 * Query fact by content
 */
SemanticFact* ums_query(UMSContext* ums, const CHAR8* query);

/**
 * Check if fact contradicts existing memory
 */
BOOLEAN ums_check_contradiction(UMSContext* ums, const CHAR8* new_fact);

/**
 * Get validated facts count
 */
UINT32 ums_get_validated_count(UMSContext* ums);

/**
 * Print memory report
 */
void ums_print_report(UMSContext* ums);

#endif // DRC_UMS_H
