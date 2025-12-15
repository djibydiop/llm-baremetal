/*
 * Interactive Chat REPL with Network Boot
 * Real conversational AI running bare-metal
 */

#ifndef CHAT_REPL_H
#define CHAT_REPL_H

#include <efi.h>
#include <efilib.h>

#define MAX_INPUT_LENGTH 512
#define MAX_HISTORY 32
#define MAX_GENERATION_LENGTH 256

typedef struct {
    CHAR16 prompt[MAX_INPUT_LENGTH];
    CHAR16 response[MAX_GENERATION_LENGTH];
    UINT64 timestamp;
} ChatExchange;

typedef struct {
    // Chat history
    ChatExchange history[MAX_HISTORY];
    UINT32 history_count;
    
    // Input buffer
    CHAR16 current_input[MAX_INPUT_LENGTH];
    UINT32 input_pos;
    
    // State
    BOOLEAN running;
    BOOLEAN network_mode;  // Network boot for large models
    
    // Stats
    UINT32 total_exchanges;
    UINT64 total_tokens_generated;
    float avg_tokens_per_sec;
    
    // Network loading
    CHAR8 remote_model_url[256];
    BOOLEAN streaming_model;  // Loading model via network
    UINT64 model_bytes_loaded;
    UINT64 model_total_size;
} ChatREPL;

// ═══════════════════════════════════════════════════════════════
// CHAT REPL FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize chat REPL
 */
EFI_STATUS chat_repl_init(ChatREPL* repl);

/**
 * Main REPL loop
 */
EFI_STATUS chat_repl_run(ChatREPL* repl);

/**
 * Process user input
 */
EFI_STATUS chat_repl_process_input(ChatREPL* repl, const CHAR16* input);

/**
 * Generate AI response
 */
EFI_STATUS chat_repl_generate_response(ChatREPL* repl, const CHAR16* prompt, CHAR16* response);

/**
 * Enable network boot mode for large models
 */
EFI_STATUS chat_repl_enable_network(ChatREPL* repl, const CHAR8* model_url);

/**
 * Stream model chunks over network (bypass UEFI memory limits)
 */
EFI_STATUS chat_repl_stream_model_chunk(ChatREPL* repl, UINT64 offset, UINT64 size);

/**
 * Print chat history
 */
void chat_repl_print_history(ChatREPL* repl);

/**
 * Clear screen and reset
 */
void chat_repl_clear(ChatREPL* repl);

/**
 * Print help
 */
void chat_repl_print_help(void);

#endif // CHAT_REPL_H
