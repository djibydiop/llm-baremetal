/*
 * LLM Interface Optimisée pour Tractor-OS
 * 
 * Optimisations:
 * - Zero-copy via shared buffers
 * - Batch processing (8 requêtes/batch)
 * - KV cache pour conversations
 * - Cache-aligned structures
 * - SIMD-ready memory layout
 * 
 * Performance gains attendus:
 * - Batch: 5-8x moins d'overhead FFI
 * - Zero-copy: 50% moins de mémoire
 * - KV cache: 10x plus rapide pour conversations
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef LLM_INTERFACE_OPTIMIZED_H
#define LLM_INTERFACE_OPTIMIZED_H

#include <stdint.h>
#include <stdatomic.h>

// ═══════════════════════════════════════════════════════════
//  CONFIGURATION OPTIMISÉE
// ═══════════════════════════════════════════════════════════

// Cache-aligned sur 64 bytes (taille cache line moderne)
typedef struct __attribute__((aligned(64))) {
    // Hot fields (frequently accessed)
    float temperature;
    int max_tokens;
    uint32_t seed;
    uint32_t _padding1;
    
    // Cold fields
    const char* model_path;
    const char* tokenizer_path;
    uint64_t _padding2[4];  // Reserve pour extensions futures
} LLMConfigOptimized;

// ═══════════════════════════════════════════════════════════
//  BUFFER ZERO-COPY
// ═══════════════════════════════════════════════════════════

typedef struct __attribute__((aligned(64))) {
    // Metadata (atomic pour thread-safety)
    atomic_uint_fast32_t length;
    uint32_t capacity;
    atomic_bool is_ready;
    uint8_t _padding[55];
    
    // Data pointer (sur nouvelle cache line)
    uint8_t* data;
} SharedBuffer;

// ═══════════════════════════════════════════════════════════
//  BATCH PROCESSING
// ═══════════════════════════════════════════════════════════

#define LLM_MAX_BATCH_SIZE 8

typedef struct __attribute__((aligned(64))) {
    uint32_t count;
    uint8_t _padding[60];
    const char* prompts[LLM_MAX_BATCH_SIZE];
    SharedBuffer* outputs[LLM_MAX_BATCH_SIZE];
    int results[LLM_MAX_BATCH_SIZE];
} LLMBatchRequest;

// ═══════════════════════════════════════════════════════════
//  API STANDARD (compatible avec version simple)
// ═══════════════════════════════════════════════════════════

void* llm_init(const LLMConfigOptimized* config);
int llm_generate(void* handle, const char* prompt, SharedBuffer* output);
void llm_cleanup(void* handle);

// ═══════════════════════════════════════════════════════════
//  API OPTIMISÉE
// ═══════════════════════════════════════════════════════════

/**
 * Génération en batch - amortize FFI overhead
 * 
 * Gains: 5-8x moins d'appels FFI, meilleure utilisation cache
 * 
 * @param handle Handle LLM
 * @param batch  Batch de requêtes (max 8)
 * @return 0 si succès, code erreur sinon
 */
int llm_generate_batch(void* handle, LLMBatchRequest* batch);

/**
 * Forward pass direct sur tokens pré-tokenizés
 * 
 * Gains: Skip tokenization, contrôle total sur tokens
 * Utile pour: Beam search, sampling custom, debug
 * 
 * @param handle       Handle LLM
 * @param tokens       Array de token IDs
 * @param token_count  Nombre de tokens
 * @param output_logits Buffer pour logits (vocab_size floats)
 * @param logit_size   Taille du buffer logits
 * @return 0 si succès
 */
int llm_forward_tokens(
    void* handle,
    const int* tokens,
    int token_count,
    float* output_logits,
    int logit_size
);

/**
 * Pre-compute KV cache pour un prompt commun
 * 
 * Gains: 10x plus rapide pour conversations répétées
 * Use case: Chat avec contexte fixe, fine-tuning prompts
 * 
 * @param handle   Handle LLM
 * @param prompt   Prompt de base à mettre en cache
 * @param cache_id ID du cache créé (output)
 * @return 0 si succès
 */
int llm_prefill_cache(
    void* handle,
    const char* prompt,
    uint32_t* cache_id
);

/**
 * Génération incrémentale depuis cache KV
 * 
 * Permet d'ajouter du contexte à un prompt cached
 * 
 * @param handle           Handle LLM
 * @param cache_id         ID du cache (de prefill_cache)
 * @param additional_prompt Texte additionnel
 * @param output           Buffer de sortie
 * @return 0 si succès
 */
int llm_generate_from_cache(
    void* handle,
    uint32_t cache_id,
    const char* additional_prompt,
    SharedBuffer* output
);

/**
 * Libère un cache KV spécifique
 * 
 * @param handle   Handle LLM
 * @param cache_id ID du cache à libérer
 */
void llm_free_cache(void* handle, uint32_t cache_id);

// ═══════════════════════════════════════════════════════════
//  ADVANCED: STREAMING & CALLBACKS
// ═══════════════════════════════════════════════════════════

/**
 * Callback pour génération streaming (token par token)
 * 
 * @param token     Token ID généré
 * @param text      Texte décodé du token (peut être NULL)
 * @param user_data Données utilisateur
 * @return 0 pour continuer, non-zero pour arrêter
 */
typedef int (*LLMStreamCallback)(int token, const char* text, void* user_data);

/**
 * Génération avec streaming callback
 * 
 * Gains: Latence perçue réduite, UI responsive
 * 
 * @param handle    Handle LLM
 * @param prompt    Prompt
 * @param callback  Fonction appelée pour chaque token
 * @param user_data Données passées au callback
 * @return 0 si succès
 */
int llm_generate_stream(
    void* handle,
    const char* prompt,
    LLMStreamCallback callback,
    void* user_data
);

// ═══════════════════════════════════════════════════════════
//  PERFORMANCE PROFILING
// ═══════════════════════════════════════════════════════════

typedef struct {
    uint64_t total_tokens_generated;
    uint64_t total_time_ns;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint32_t ffi_calls;
    float avg_tokens_per_sec;
    float cache_hit_rate;
} LLMPerfStats;

/**
 * Récupère les statistiques de performance
 * 
 * @param handle Handle LLM
 * @param stats  Structure à remplir
 * @return 0 si succès
 */
int llm_get_perf_stats(void* handle, LLMPerfStats* stats);

/**
 * Reset les compteurs de performance
 */
void llm_reset_perf_stats(void* handle);

// ═══════════════════════════════════════════════════════════
//  MEMORY MANAGEMENT HELPERS
// ═══════════════════════════════════════════════════════════

/**
 * Crée un SharedBuffer depuis un buffer Rust
 * 
 * @param data     Pointeur vers données (doit rester valide)
 * @param capacity Taille du buffer
 * @return SharedBuffer initialisé
 */
static inline SharedBuffer llm_shared_buffer_create(uint8_t* data, uint32_t capacity) {
    SharedBuffer buf = {
        .length = ATOMIC_VAR_INIT(0),
        .capacity = capacity,
        .is_ready = ATOMIC_VAR_INIT(1),
        .data = data
    };
    return buf;
}

/**
 * Récupère la longueur actuelle du buffer (atomic)
 */
static inline uint32_t llm_shared_buffer_length(const SharedBuffer* buf) {
    return atomic_load(&buf->length);
}

/**
 * Set la longueur du buffer (atomic)
 */
static inline void llm_shared_buffer_set_length(SharedBuffer* buf, uint32_t len) {
    atomic_store(&buf->length, len);
}

// ═══════════════════════════════════════════════════════════
//  COMPILE-TIME CHECKS
// ═══════════════════════════════════════════════════════════

// Vérifie que les structures sont bien alignées
_Static_assert(sizeof(LLMConfigOptimized) == 64, "LLMConfigOptimized must be 64 bytes");
_Static_assert(sizeof(SharedBuffer) == 128, "SharedBuffer must be 128 bytes (2 cache lines)");
_Static_assert(sizeof(LLMBatchRequest) % 64 == 0, "LLMBatchRequest must be cache-aligned");

#endif // LLM_INTERFACE_OPTIMIZED_H

