/**
 * ============================================================================
 * LLM BARE-METAL v5.0 - PUBLIC API INTERFACE
 * ============================================================================
 * 
 * Universal LLM integration API for bare-metal, embedded, and OS projects.
 * Originally designed for Tractor-OS/YamaOS, now generalized for any use case.
 * 
 * License: MIT | Author: djibydiop | Version: 5.0.0
 * SPDX-License-Identifier: MIT
 * ============================================================================
 */

#ifndef LLM_INTERFACE_H
#define LLM_INTERFACE_H

#include <stdint.h>

/* ========================================================================== */
/* VERSION & STATUS CODES                                                     */
/* ========================================================================== */

#define LLM_VERSION_MAJOR 5
#define LLM_VERSION_MINOR 0
#define LLM_VERSION_PATCH 0

#define LLM_SUCCESS              0
#define LLM_ERROR_INIT          -1
#define LLM_ERROR_GENERATE      -2
#define LLM_ERROR_MEMORY        -3
#define LLM_ERROR_MODEL         -4
#define LLM_ERROR_FILE_NOT_FOUND -5
#define LLM_ERROR_INVALID_TOKEN  -6

/* ========================================================================== */
/* CONFIGURATION                                                              */
/* ========================================================================== */

typedef struct {
    const char* model_path;      // Path to .bin model file (e.g., "stories110M.bin")
    const char* tokenizer_path;  // Path to tokenizer.bin
    float temperature;           // 0.0 = greedy, 1.0 = normal, >1.0 = creative
    int max_tokens;              // Maximum tokens to generate
    uint32_t seed;               // RNG seed for reproducibility
    int enable_neuronet;         // Enable NEURO-NET features (0/1)
    int neuronet_node_id;        // Node ID for NEURO-NET (0-15)
} LLMConfig;

/* Default values */
#define LLM_DEFAULT_TEMPERATURE 0.9f
#define LLM_DEFAULT_MAX_TOKENS 256
#define LLM_DEFAULT_SEED 42

/* ========================================================================== */
/* CORE API                                                                   */
/* ========================================================================== */

typedef struct LLMHandle LLMHandle;  // Opaque handle

/**
 * Initialize LLM engine
 * @param config Configuration structure
 * @return Handle on success, NULL on failure
 */
LLMHandle* llm_init(const LLMConfig* config);

/**
 * Generate text from prompt
 * @param handle Initialized LLM handle
 * @param prompt Input text
 * @param output_buffer Pre-allocated output buffer
 * @param buffer_size Buffer size in bytes
 * @return LLM_SUCCESS or error code
 */
int llm_generate(
    LLMHandle* handle,
    const char* prompt,
    char* output_buffer,
    int buffer_size
);

/**
 * Get last error message
 * @param handle LLM handle
 * @return Error string (never NULL)
 */
const char* llm_get_error(LLMHandle* handle);

/**
 * Cleanup and free resources
 * @param handle LLM handle to destroy
 */
void llm_cleanup(LLMHandle* handle);

/**
 * Check if LLM is initialized and ready
 * @param handle LLM handle
 * @return 1 if ready, 0 if not
 */
int llm_is_ready(LLMHandle* handle);

/* ========================================================================== */
/* MODEL INFORMATION                                                          */
/* ========================================================================== */

typedef struct {
    int dim;           // Transformer dimension
    int n_layers;      // Number of layers
    int n_heads;       // Number of attention heads
    int vocab_size;    // Vocabulary size
    int seq_len;       // Max sequence length
} LLMModelInfo;

/**
 * Get model architecture information
 * @param handle LLM handle
 * @param info Pointer to LLMModelInfo structure to fill
 * @return LLM_SUCCESS or error code
 */
int llm_get_model_info(LLMHandle* handle, LLMModelInfo* info);

/**
 * Get version string
 * @return Version string (e.g., "5.0.0")
 */
const char* llm_get_version(void);

/* ========================================================================== */
/* STATISTICS                                                                 */
/* ========================================================================== */

typedef struct {
    uint64_t tokens_generated;
    uint64_t total_time_ms;
    float tokens_per_second;
    uint64_t packets_sent;
    uint64_t packets_received;
    float network_coherence;
    float dream_accuracy;
    uint32_t evolution_generation;
} LLMStats;

/**
 * Get system statistics
 * @param handle LLM handle
 * @param stats Pointer to LLMStats to fill
 * @return LLM_SUCCESS or error code
 */
int llm_get_stats(LLMHandle* handle, LLMStats* stats);

/* ========================================================================== */
/* NEURO-NET API (Optional - Phase 1-4)                                       */
/* ========================================================================== */

#define NEURONET_BROADCAST 0xFF
#define NEURONET_MAX_NODES 16

/**
 * Send data through NEURO-NET
 * @param handle LLM handle (must have enable_neuronet=1)
 * @param data Data buffer
 * @param size Data size
 * @param dest_node Destination node (0-15 or NEURONET_BROADCAST)
 * @return LLM_SUCCESS or error code
 */
int neuronet_send(LLMHandle* handle, const uint8_t* data, uint32_t size, uint8_t dest_node);

/**
 * Receive data from NEURO-NET
 * @param handle LLM handle
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes received or negative error
 */
int neuronet_receive(LLMHandle* handle, uint8_t* buffer, uint32_t buffer_size);

/**
 * Get network coherence metric
 * @param handle LLM handle
 * @return Coherence (0.0-1.0) or negative error
 */
float neuronet_get_coherence(LLMHandle* handle);

/* ========================================================================== */
/* EXAMPLE USAGE                                                              */
/* ========================================================================== */

#if 0  // Example code (not compiled)

// Basic text generation
void example_basic() {
    LLMConfig cfg = {
        .model_path = "stories110M.bin",
        .tokenizer_path = "tokenizer.bin",
        .temperature = 0.9f,
        .max_tokens = 256,
        .seed = 42,
        .enable_neuronet = 0,
        .neuronet_node_id = 0
    };
    
    LLMHandle* llm = llm_init(&cfg);
    if (!llm) return;
    
    char output[1024];
    llm_generate(llm, "Once upon a time", output, sizeof(output));
    printf("%s\n", output);
    
    llm_cleanup(llm);
}

// With NEURO-NET
void example_neuronet() {
    LLMConfig cfg = { /* ... */, .enable_neuronet = 1, .neuronet_node_id = 0 };
    LLMHandle* llm = llm_init(&cfg);
    
    uint8_t data[] = "Hello";
    neuronet_send(llm, data, sizeof(data), 5);  // Send to node 5
    
    float coherence = neuronet_get_coherence(llm);
    printf("Coherence: %.2f\n", coherence);
    
    llm_cleanup(llm);
}

#endif

#endif // LLM_INTERFACE_H
