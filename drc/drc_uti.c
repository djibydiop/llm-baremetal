/*
 * UTI - Unité de Temps et d'Irreversibilité
 * Implementation
 */

#include "drc_uti.h"

// EFI Time service extern
extern EFI_RUNTIME_SERVICES *RT;

// ═══════════════════════════════════════════════════════════════
// UTI IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize UTI context
 */
EFI_STATUS uti_init(UTIContext* uti) {
    if (!uti) return EFI_INVALID_PARAMETER;
    
    uti->event_count = 0;
    uti->link_count = 0;
    uti->causality_violations = 0;
    uti->irreversibility_violations = 0;
    
    // Get current time from EFI Runtime Services
    if (RT && RT->GetTime) {
        EFI_TIME time;
        if (RT->GetTime(&time, NULL) == EFI_SUCCESS) {
            // Simple timestamp: Year*10000 + Month*100 + Day
            uti->current_time = time.Year * 10000 + time.Month * 100 + time.Day;
        } else {
            uti->current_time = 0;
        }
    } else {
        // Fallback: Use simple counter
        uti->current_time = 0;
    }
    
    uti->startup_time = uti->current_time;
    
    return EFI_SUCCESS;
}

/**
 * Add temporal event
 */
EFI_STATUS uti_add_event(UTIContext* uti,
                         const CHAR8* description,
                         EventTime time,
                         BOOLEAN reversible) {
    if (!uti || !description) return EFI_INVALID_PARAMETER;
    if (uti->event_count >= 32) return EFI_OUT_OF_RESOURCES;
    
    TemporalEvent* event = &uti->events[uti->event_count];
    event->event_id = uti->event_count;
    event->time = time;
    event->reversible = reversible;
    event->committed = (time == EVENT_PAST);
    event->timestamp = uti->current_time;
    
    // Copy description
    UINT32 i = 0;
    while (i < 63 && description[i] != '\0') {
        event->description[i] = description[i];
        i++;
    }
    event->description[i] = '\0';
    
    uti->event_count++;
    return EFI_SUCCESS;
}

/**
 * Add causal link
 */
EFI_STATUS uti_add_causality(UTIContext* uti,
                             UINT32 cause_id,
                             UINT32 effect_id) {
    if (!uti) return EFI_INVALID_PARAMETER;
    if (cause_id >= uti->event_count || effect_id >= uti->event_count) {
        return EFI_INVALID_PARAMETER;
    }
    if (uti->link_count >= 64) return EFI_OUT_OF_RESOURCES;
    
    CausalLink* link = &uti->links[uti->link_count];
    link->from_event = cause_id;
    link->to_event = effect_id;
    
    // Determine relation based on timestamps
    TemporalEvent* cause = &uti->events[cause_id];
    TemporalEvent* effect = &uti->events[effect_id];
    
    if (cause->timestamp < effect->timestamp) {
        link->relation = CAUSALITY_BEFORE;
        link->validated = TRUE;
    } else if (cause->timestamp > effect->timestamp) {
        link->relation = CAUSALITY_AFTER;
        link->validated = FALSE;  // Violation!
        uti->causality_violations++;
    } else {
        link->relation = CAUSALITY_SIMULTANEOUS;
        link->validated = TRUE;
    }
    
    uti->link_count++;
    return EFI_SUCCESS;
}

/**
 * Check if causal order is respected
 */
BOOLEAN uti_check_causality(UTIContext* uti) {
    if (!uti) return FALSE;
    
    return (uti->causality_violations == 0);
}

/**
 * Check if trying to reverse irreversible action
 */
BOOLEAN uti_check_irreversibility(UTIContext* uti, UINT32 event_id) {
    if (!uti || event_id >= uti->event_count) return FALSE;
    
    TemporalEvent* event = &uti->events[event_id];
    
    if (event->committed && !event->reversible) {
        uti->irreversibility_violations++;
        return FALSE;  // Cannot reverse
    }
    
    return TRUE;  // Reversible or not committed yet
}

/**
 * Get current system time
 */
UINT64 uti_get_time(UTIContext* uti) {
    if (!uti) return 0;
    return uti->current_time;
}

/**
 * Print temporal report
 */
void uti_print_report(UTIContext* uti) {
    if (!uti) return;
    
    Print(L"\n[UTI] Temporal Reasoning Report\n");
    Print(L"  Events tracked: %u\n", uti->event_count);
    Print(L"  Causal links: %u\n", uti->link_count);
    Print(L"  Causality violations: %u\n", uti->causality_violations);
    Print(L"  Irreversibility violations: %u\n", uti->irreversibility_violations);
    Print(L"  Current time: %llu\n", uti->current_time);
    Print(L"  Uptime: %llu\n", uti->current_time - uti->startup_time);
    
    if (uti->event_count > 0 && uti->event_count <= 8) {
        Print(L"  Events:\n");
        for (UINT32 i = 0; i < uti->event_count; i++) {
            TemporalEvent* ev = &uti->events[i];
            const CHAR16* time_str = L"?";
            switch (ev->time) {
                case EVENT_PAST: time_str = L"PAST"; break;
                case EVENT_PRESENT: time_str = L"NOW"; break;
                case EVENT_FUTURE: time_str = L"FUTURE"; break;
                case EVENT_TIMELESS: time_str = L"TIMELESS"; break;
            }
            Print(L"    #%u [%s] %a %s\n", i, time_str, ev->description,
                  ev->reversible ? L"(reversible)" : L"(irreversible)");
        }
    }
    
    if (uti_check_causality(uti)) {
        Print(L"  ✓ Causal order respected\n");
    } else {
        Print(L"  ⚠ Causal violations detected\n");
    }
}
