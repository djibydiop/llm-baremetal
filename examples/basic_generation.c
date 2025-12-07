/**
 * ============================================================================
 * EXAMPLE 1: Basic Text Generation
 * ============================================================================
 * 
 * Minimal example showing how to integrate LLM Bare-Metal for text generation.
 * Perfect starting point for any project.
 * 
 * Compile: gcc basic_generation.c -o basic_generation -I.. -L.. -lllm
 * Run: ./basic_generation
 * 
 * ============================================================================
 */

#include "../llm_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    printf("=== LLM Bare-Metal - Basic Generation Example ===\n\n");
    
    // Step 1: Configure LLM
    LLMConfig config = {
        .model_path = "stories110M.bin",
        .tokenizer_path = "tokenizer.bin",
        .temperature = 0.9f,
        .max_tokens = 256,
        .seed = 42,
        .enable_neuronet = 0,  // Disable NEURO-NET for simplicity
        .neuronet_node_id = 0
    };
    
    // Step 2: Initialize LLM
    printf("Initializing LLM...\n");
    LLMHandle* llm = llm_init(&config);
    if (!llm) {
        fprintf(stderr, "Failed to initialize LLM\n");
        return 1;
    }
    
    // Step 3: Check if ready
    if (!llm_is_ready(llm)) {
        fprintf(stderr, "LLM not ready\n");
        llm_cleanup(llm);
        return 1;
    }
    
    // Step 4: Get model info
    LLMModelInfo info;
    if (llm_get_model_info(llm, &info) == LLM_SUCCESS) {
        printf("Model loaded:\n");
        printf("  Dimension: %d\n", info.dim);
        printf("  Layers: %d\n", info.n_layers);
        printf("  Heads: %d\n", info.n_heads);
        printf("  Vocab: %d\n", info.vocab_size);
        printf("  Max seq: %d\n\n", info.seq_len);
    }
    
    // Step 5: Generate text from prompt
    const char* prompts[] = {
        "Once upon a time",
        "The future of AI is",
        "In a galaxy far away"
    };
    
    char output[1024];
    
    for (int i = 0; i < 3; i++) {
        printf("Prompt %d: \"%s\"\n", i+1, prompts[i]);
        printf("Generating...\n");
        
        int result = llm_generate(llm, prompts[i], output, sizeof(output));
        
        if (result == LLM_SUCCESS) {
            printf("Output: %s\n\n", output);
        } else {
            fprintf(stderr, "Error: %s\n\n", llm_get_error(llm));
        }
    }
    
    // Step 6: Get statistics
    LLMStats stats;
    if (llm_get_stats(llm, &stats) == LLM_SUCCESS) {
        printf("Statistics:\n");
        printf("  Tokens generated: %llu\n", stats.tokens_generated);
        printf("  Total time: %llu ms\n", stats.total_time_ms);
        printf("  Tokens/sec: %.2f\n", stats.tokens_per_second);
    }
    
    // Step 7: Cleanup
    llm_cleanup(llm);
    printf("\nDone!\n");
    
    return 0;
}
