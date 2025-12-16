/*
 * UTI - Unité de Temps et d'Irreversibilité
 * Raisonner avec le temps réel et l'irréversibilité
 */

#ifndef DRC_UTI_H
#define DRC_UTI_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// UTI - TEMPORAL REASONING
// ═══════════════════════════════════════════════════════════════

typedef enum {
    EVENT_PAST,         // Déjà arrivé (irréversible)
    EVENT_PRESENT,      // En cours
    EVENT_FUTURE,       // Pas encore arrivé
    EVENT_TIMELESS      // Hors du temps (logique pure)
} EventTime;

typedef enum {
    CAUSALITY_BEFORE,   // A doit être avant B
    CAUSALITY_AFTER,    // A doit être après B
    CAUSALITY_SIMULTANEOUS,  // A et B en même temps
    CAUSALITY_INDEPENDENT    // Pas de lien
} CausalRelation;

typedef struct {
    UINT32 event_id;
    EventTime time;
    BOOLEAN reversible;     // TRUE si on peut annuler
    BOOLEAN committed;      // TRUE si déjà fait
    UINT64 timestamp;       // Temps absolu (ticks)
    CHAR8 description[64];
} TemporalEvent;

typedef struct {
    UINT32 from_event;
    UINT32 to_event;
    CausalRelation relation;
    BOOLEAN validated;      // TRUE si respect causalité
} CausalLink;

typedef struct {
    TemporalEvent events[32];
    UINT32 event_count;
    
    CausalLink links[64];
    UINT32 link_count;
    
    // Violations détectées
    UINT32 causality_violations;
    UINT32 irreversibility_violations;
    
    // Temps système
    UINT64 current_time;
    UINT64 startup_time;
} UTIContext;

// ═══════════════════════════════════════════════════════════════
// UTI FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UTI context
 */
EFI_STATUS uti_init(UTIContext* uti);

/**
 * Add temporal event
 */
EFI_STATUS uti_add_event(UTIContext* uti, 
                         const CHAR8* description,
                         EventTime time,
                         BOOLEAN reversible);

/**
 * Add causal link between events
 */
EFI_STATUS uti_add_causality(UTIContext* uti,
                             UINT32 cause_id,
                             UINT32 effect_id);

/**
 * Check if causal order is respected
 */
BOOLEAN uti_check_causality(UTIContext* uti);

/**
 * Check if trying to reverse irreversible action
 */
BOOLEAN uti_check_irreversibility(UTIContext* uti, UINT32 event_id);

/**
 * Get current system time
 */
UINT64 uti_get_time(UTIContext* uti);

/**
 * Print temporal report
 */
void uti_print_report(UTIContext* uti);

#endif // DRC_UTI_H
