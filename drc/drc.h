/*
 * DRC - Djibion Reasoning Core
 * Unified header for all DRC components
 */

#ifndef DRC_H
#define DRC_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// DRC CORE STRUCTURES
// ═══════════════════════════════════════════════════════════════

#ifndef DRC_MAX_HISTORY
#define DRC_MAX_HISTORY 512
#endif
#define DRC_BLACKLIST_SIZE 256
#define DRC_EMBEDDING_DIM 64

// Forward declaration (type defined in llama2_efi.c)
typedef struct DjibionReasonerCore DjibionReasonerCore;

// Main DRC State (v4.0 Multi-Expert System)
#ifndef DJIBION_REASONER_CORE_DEFINED
#define DJIBION_REASONER_CORE_DEFINED
typedef struct DjibionReasonerCore {
    // Token history and monitoring
    int token_history[DRC_MAX_HISTORY];
    int history_length;
    int blacklist[DRC_BLACKLIST_SIZE];
    int blacklist_size;
    
    // Repetition detection
    int repetition_detected;
    int repetition_count;
    int last_token;
    int repeat_chain_length;
    
    // Embedding analysis
    float embedding_variance;
    float embedding_centroid[DRC_EMBEDDING_DIM];
    int embedding_analyzed;
    
    // Loop detection
    int loop_detected;
    int loop_start;
    int loop_length;
    int max_loop_length;
    
    // Distribution monitoring
    float max_prob;
    float entropy;
    int low_entropy_count;
    int stagnation_count;
    
    // Forced diversity mode
    int force_random_token;
    int consecutive_low_entropy;
    
    // Deep monitoring
    int total_zero_probs;
    int total_high_entropy;
    float avg_entropy;
    
    // Domain Detection (10+ specialized domains)
    int detected_domain;
    int domain_confidence;
    int domain_switches;
    
    // Shakespeare Expert Mode
    int shakespeare_mode;
    float shakespeare_vocab_boost;
    float iambic_pentameter_bias;
    float sonnet_structure_boost;
    int theater_dialogue_mode;
    int soliloquy_depth;
    
    // Math Expert Mode
    int math_mode;
    float equation_bias;
    float logic_proof_boost;
    float theorem_awareness;
    int calculus_mode;
    int geometry_mode;
    int algebra_mode;
    
    // Computer Science Expert Mode
    int computer_mode;
    float code_syntax_boost;
    float algorithm_bias;
    int programming_language;
    int data_structures_mode;
    int systems_thinking;
    float debugging_mindset;
    
    // Science Expert Mode
    int science_mode;
    int physics_mode;
    int chemistry_mode;
    int biology_mode;
    int astronomy_mode;
    float scientific_method_boost;
    float formula_awareness;
    
    // Philosophy Expert Mode
    int philosophy_mode;
    int logic_mode;
    int ethics_mode;
    int metaphysics_mode;
    int epistemology_mode;
    float socratic_method_bias;
    float argument_structure_boost;
    
    // History Expert Mode
    int history_mode;
    int ancient_history;
    int medieval_history;
    int modern_history;
    float chronological_awareness;
    float civilization_knowledge;
    
    // Poetry Expert Mode
    int poetry_mode;
    float rhyme_scheme_boost;
    float meter_awareness;
    float metaphor_bias;
    int verse_structure_mode;
    
    // Music Theory Expert Mode
    int music_mode;
    float harmony_awareness;
    float rhythm_pattern_boost;
    int composition_mode;
    
    // Art & Design Expert Mode
    int art_mode;
    int painting_mode;
    int architecture_mode;
    float aesthetic_principles;
    
    // Self-Awareness & Meta-Cognition
    int awareness_mode;
    int meta_cognitive_depth;
    int introspection_level;
    int task_understanding;
    int exposure_awareness;
    int reasoning_transparency;
    
    // Ultra-Advanced Strategy System
    int current_strategy;
    int strategy_switches;
    int hybrid_mode;
    int cross_domain_synthesis;
    
    // Configuration
    int active;
    int verbose_logging;
    int training_mode;
    int network_learning;
    int ultra_aggressive_mode;
    int multi_expert_mode;
    int v4_ultra_advanced;
} DjibionReasonerCore;
#endif // DJIBION_REASONER_CORE_DEFINED

// ═══════════════════════════════════════════════════════════════
// DRC CORE FUNCTIONS
// ═══════════════════════════════════════════════════════════════

// DRC v4.0 functions (implemented in llama2_efi.c)
// NOTE: These are kept in llama2_efi.c for now to avoid breaking existing code
extern void drc_init(DjibionReasonerCore* drc);
extern void drc_observe_token(DjibionReasonerCore* drc, int token);
extern void drc_select_strategy(DjibionReasonerCore* drc);
extern void drc_print_training_stats(DjibionReasonerCore* drc);
extern void drc_sync_with_network(DjibionReasonerCore* drc);

// Include sub-modules
#include "drc_urs.h"
#include "drc_modelbridge.h"
#include "drc_verification.h"
#include "drc_uic.h"
#include "drc_ucr.h"
#include "drc_uti.h"
#include "drc_uco.h"
#include "drc_ums.h"

// Infrastructure modules
#include "drc_perf.h"
#include "drc_config.h"
#include "drc_trace.h"

// Additional cognitive units
#include "drc_uam.h"
#include "drc_upe.h"
#include "drc_uiv.h"

// Phase 3-9: Advanced systems
#include "drc_selfdiag.h"
#include "drc_semcluster.h"
#include "drc_timebudget.h"
#include "drc_bias.h"
#include "drc_emergency.h"
#include "drc_radiocog.h"

#endif // DRC_H
