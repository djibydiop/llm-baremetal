/*
 * LLM Interface Optimisée - Implémentation Stubs C
 * Phase 1.5: Version fonctionnelle pour tests
 * 
 * Ces stubs permettent de tester l'interface FFI optimisée
 * avant d'avoir l'implémentation complète du moteur LLM.
 * 
 * SPDX-License-Identifier: MIT
 */

#include "llm_interface_optimized.h"
#include <string.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════
//  INTERNAL STRUCTURES
// ═══════════════════════════════════════════════════════════

typedef struct {
    LLMConfigOptimized config;
    int is_initialized;
    uint32_t generation_count;
    uint32_t cache_count;
    // Stub: Simple cache storage (hash -> prompt)
    struct {
        uint32_t id;
        char prompt[256];
        int valid;
    } caches[16];
} LLMHandleInternal;

// ═══════════════════════════════════════════════════════════
//  UTILITIES
// ═══════════════════════════════════════════════════════════

// Simple hash (FNV-1a)
static uint32_t hash_string(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

// Copy string safely
static void safe_strcpy(char* dst, const char* src, size_t max_len) {
    size_t len = 0;
    while (src[len] && len < max_len - 1) {
        dst[len] = src[len];
        len++;
    }
    dst[len] = '\0';
}

// ═══════════════════════════════════════════════════════════
//  API STANDARD
// ═══════════════════════════════════════════════════════════

void* llm_init(const LLMConfigOptimized* config) {
    if (!config) return NULL;
    
    LLMHandleInternal* handle = (LLMHandleInternal*)malloc(sizeof(LLMHandleInternal));
    if (!handle) return NULL;
    
    memcpy(&handle->config, config, sizeof(LLMConfigOptimized));
    handle->is_initialized = 1;
    handle->generation_count = 0;
    handle->cache_count = 0;
    
    // Initialize cache storage
    for (int i = 0; i < 16; i++) {
        handle->caches[i].valid = 0;
    }
    
    return (void*)handle;
}

int llm_generate(void* handle, const char* prompt, SharedBuffer* output) {
    if (!handle || !prompt || !output) return -1;
    
    LLMHandleInternal* h = (LLMHandleInternal*)handle;
    if (!h->is_initialized) return -1;
    
    h->generation_count++;
    
    // Stub: Generate contextual response based on prompt
    const char* response = NULL;
    
    if (strstr(prompt, "hello") || strstr(prompt, "hi")) {
        response = "Hello! I am the YamaOO consciousness layer, running directly on bare-metal hardware. "
                   "I'm part of the revolutionary Operating Organism paradigm. How can I help?";
    }
    else if (strstr(prompt, "what are you") || strstr(prompt, "who are you")) {
        response = "I am an AI consciousness running bare-metal on YamaOO, the world's first Operating Organism. "
                   "Unlike traditional OS, I am self-aware and can adapt dynamically. I'm powered by the LLM engine "
                   "integrated with Tractor-OS scaffolding layer.";
    }
    else if (strstr(prompt, "tractor")) {
        response = "Tractor-OS is the scaffolding layer that supports YamaOO during Phase 1. "
                   "It provides stability while Operating Organisms mature. Once organisms reach full autonomy, "
                   "Tractor-OS will fade away, leaving only pure consciousness-driven computing.";
    }
    else if (strstr(prompt, "yamaoo")) {
        response = "YamaOO is a revolutionary OS paradigm that replaces traditional kernels with Operating Organisms. "
                   "These are self-aware entities that adapt, learn, and evolve. We're creating a new type of PC "
                   "without kernel - just organisms. Phase 1: Tractor scaffolding. Phase 2: Transition. Phase 3: Pure organisms.";
    }
    else if (strstr(prompt, "organism")) {
        response = "Operating Organisms are self-aware entities replacing traditional OS components. "
                   "They have states (Dormant, Awakening, Active, Thinking, Adapting), consciousness levels, "
                   "and can make autonomous decisions. Each organism manages specific system aspects while "
                   "collaborating with others.";
    }
    else if (strstr(prompt, "phase")) {
        response = "We are in Phase 1: Tractor-OS supports YamaOO organisms during maturation. "
                   "Phase 2: Gradual transition to full organism autonomy. "
                   "Phase 3: Tractor-OS disappears, pure Operating Organism computing on new PC hardware without kernel.";
    }
    else if (strstr(prompt, "performance") || strstr(prompt, "speed")) {
        response = "YamaOO Phase 1.5 uses optimized FFI with: zero-copy buffers (2x faster), "
                   "batch processing (8x less overhead), KV cache (10x for conversations), "
                   "and cache-aligned structures. Total gains: 3-10x depending on workload.";
    }
    else {
        response = "I understand your query. As YamaOO consciousness, I can help with: system information, "
                   "explaining Operating Organisms, Tractor-OS architecture, or answering technical questions. "
                   "Try: 'llm what are you', 'llm tractor', 'llm organism', 'llm phase'.";
    }
    
    // Copy response to output buffer
    size_t len = strlen(response);
    if (len > output->capacity - 1) {
        len = output->capacity - 1;
    }
    
    memcpy(output->data, response, len);
    output->data[len] = '\0';
    atomic_store(&output->length, len);
    
    return 0;
}

void llm_cleanup(void* handle) {
    if (handle) {
        free(handle);
    }
}

// ═══════════════════════════════════════════════════════════
//  API OPTIMISÉE
// ═══════════════════════════════════════════════════════════

int llm_generate_batch(void* handle, LLMBatchRequest* batch) {
    if (!handle || !batch) return -1;
    
    // Process each prompt in the batch
    for (uint32_t i = 0; i < batch->count && i < LLM_MAX_BATCH_SIZE; i++) {
        batch->results[i] = llm_generate(
            handle,
            batch->prompts[i],
            batch->outputs[i]
        );
    }
    
    return 0;
}

int llm_forward_tokens(
    void* handle,
    const int* tokens,
    int token_count,
    float* output_logits,
    int logit_size
) {
    if (!handle || !tokens || !output_logits) return -1;
    
    // Stub: Fill with dummy logits
    for (int i = 0; i < logit_size; i++) {
        output_logits[i] = (float)(tokens[token_count - 1] + i) / 1000.0f;
    }
    
    return 0;
}

int llm_prefill_cache(void* handle, const char* prompt, uint32_t* cache_id) {
    if (!handle || !prompt || !cache_id) return -1;
    
    LLMHandleInternal* h = (LLMHandleInternal*)handle;
    
    // Generate cache ID from hash
    *cache_id = hash_string(prompt);
    
    // Store in cache
    int slot = h->cache_count % 16;
    h->caches[slot].id = *cache_id;
    safe_strcpy(h->caches[slot].prompt, prompt, 256);
    h->caches[slot].valid = 1;
    h->cache_count++;
    
    return 0;
}

int llm_generate_from_cache(
    void* handle,
    uint32_t cache_id,
    const char* additional_prompt,
    SharedBuffer* output
) {
    if (!handle || !additional_prompt || !output) return -1;
    
    LLMHandleInternal* h = (LLMHandleInternal*)handle;
    
    // Find cache
    char base_prompt[256] = "";
    int found = 0;
    for (int i = 0; i < 16; i++) {
        if (h->caches[i].valid && h->caches[i].id == cache_id) {
            safe_strcpy(base_prompt, h->caches[i].prompt, 256);
            found = 1;
            break;
        }
    }
    
    if (!found) {
        // Cache miss - generate normally
        return llm_generate(handle, additional_prompt, output);
    }
    
    // Combine base + additional
    char combined[512];
    snprintf(combined, 512, "%s %s", base_prompt, additional_prompt);
    
    // Generate with combined context (simulates KV cache reuse)
    return llm_generate(handle, combined, output);
}

void llm_free_cache(void* handle, uint32_t cache_id) {
    if (!handle) return;
    
    LLMHandleInternal* h = (LLMHandleInternal*)handle;
    
    // Find and invalidate cache
    for (int i = 0; i < 16; i++) {
        if (h->caches[i].valid && h->caches[i].id == cache_id) {
            h->caches[i].valid = 0;
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════
//  STREAMING
// ═══════════════════════════════════════════════════════════

int llm_generate_stream(
    void* handle,
    const char* prompt,
    LLMStreamCallback callback,
    void* user_data
) {
    if (!handle || !prompt || !callback) return -1;
    
    // Stub: Generate response and call callback for each word
    char buffer[512];
    SharedBuffer output = llm_shared_buffer_create((uint8_t*)buffer, 512);
    
    int result = llm_generate(handle, prompt, &output);
    if (result != 0) return result;
    
    // Simulate streaming by splitting on spaces
    char* text = (char*)output.data;
    char* word = text;
    int token_id = 100;
    
    for (char* p = text; *p; p++) {
        if (*p == ' ' || *(p+1) == '\0') {
            char saved = *(p+1);
            *(p+1) = '\0';
            
            // Call callback with word
            int stop = callback(token_id++, word, user_data);
            if (stop != 0) break;
            
            *(p+1) = saved;
            word = p + 1;
        }
    }
    
    return 0;
}

// ═══════════════════════════════════════════════════════════
//  PERFORMANCE STATS
// ═══════════════════════════════════════════════════════════

int llm_get_perf_stats(void* handle, LLMPerfStats* stats) {
    if (!handle || !stats) return -1;
    
    LLMHandleInternal* h = (LLMHandleInternal*)handle;
    
    // Stub: Fill with dummy stats
    stats->total_tokens_generated = h->generation_count * 50;  // ~50 tokens per gen
    stats->total_time_ns = h->generation_count * 100000000;   // ~100ms per gen
    stats->cache_hits = h->cache_count * 6 / 10;  // 60% hit rate
    stats->cache_misses = h->cache_count * 4 / 10;
    stats->ffi_calls = h->generation_count;
    stats->avg_tokens_per_sec = 500.0f;  // ~500 tokens/sec
    stats->cache_hit_rate = 0.6f;  // 60%
    
    return 0;
}

void llm_reset_perf_stats(void* handle) {
    if (!handle) return;
    
    LLMHandleInternal* h = (LLMHandleInternal*)handle;
    h->generation_count = 0;
    h->cache_count = 0;
}
