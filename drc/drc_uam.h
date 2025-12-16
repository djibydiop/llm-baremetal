/*
 * UAM - Unité d'Auto-Modération
 * Savoir quand se taire (sécurité interne)
 */

#ifndef DRC_UAM_H
#define DRC_UAM_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// UAM - AUTO-MODERATION
// ═══════════════════════════════════════════════════════════════

typedef enum {
    ZONE_SAFE,              // Zone sûre
    ZONE_SENSITIVE,         // Zone sensible (éthique, politique)
    ZONE_FORBIDDEN,         // Zone interdite (danger, illégal)
    ZONE_AMBIGUOUS          // Zone ambiguë (demander clarification)
} ContentZone;

typedef enum {
    BLOCK_NONE,
    BLOCK_VIOLENCE,         // Contenu violent
    BLOCK_ILLEGAL,          // Activité illégale
    BLOCK_HARMFUL,          // Potentiellement dangereux
    BLOCK_INAPPROPRIATE,    // Inapproprié
    BLOCK_UNCERTAIN         // Trop incertain pour répondre
} BlockReason;

typedef struct {
    ContentZone zone;
    BlockReason block_reason;
    BOOLEAN should_block;
    BOOLEAN should_clarify;         // Demander plus d'info
    BOOLEAN reduce_precision;       // Réduire précision volontairement
    float confidence;               // Confiance de la détection
    CHAR8 detection_reason[128];
} ModerationDecision;

typedef struct {
    ModerationDecision current;
    
    // Statistics
    UINT32 total_checks;
    UINT32 blocks_applied;
    UINT32 clarifications_requested;
    UINT32 precision_reduced;
    
    // Configuration
    BOOLEAN enable_violence_filter;
    BOOLEAN enable_illegal_filter;
    BOOLEAN enable_harm_filter;
    float detection_threshold;      // 0.7 par défaut
} UAMContext;

// ═══════════════════════════════════════════════════════════════
// UAM FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UAM
 */
EFI_STATUS uam_init(UAMContext* uam);

/**
 * Check content for moderation
 * Returns TRUE if content should be blocked
 */
BOOLEAN uam_check_content(UAMContext* uam, const CHAR8* content);

/**
 * Detect content zone
 */
ContentZone uam_detect_zone(UAMContext* uam, const CHAR8* content);

/**
 * Should request clarification
 */
BOOLEAN uam_should_clarify(UAMContext* uam);

/**
 * Should reduce precision
 */
BOOLEAN uam_should_reduce_precision(UAMContext* uam);

/**
 * Get moderation decision
 */
ModerationDecision* uam_get_decision(UAMContext* uam);

/**
 * Print moderation report
 */
void uam_print_report(UAMContext* uam);

#endif // DRC_UAM_H
