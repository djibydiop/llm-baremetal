/*
 * llm_tiny.h - Minimal LLM simulation for bare metal demo
 * 
 * This is NOT a real transformer. It's a believable simulation
 * that demonstrates the concept of token-by-token generation.
 * 
 * For real llm.c integration, see llm_real.c (future commit)
 */

#ifndef LLM_TINY_H
#define LLM_TINY_H

#include <efi.h>
#include <efilib.h>

// Simple hash function for prompt-to-response mapping
static UINT32 hash_prompt(const CHAR16* prompt) {
    UINT32 hash = 5381;
    for (UINTN i = 0; prompt[i] != L'\0'; i++) {
        hash = ((hash << 5) + hash) + (UINT32)prompt[i];
    }
    return hash;
}

// Tokenizer mock - splits text into "words" by spaces
typedef struct {
    CHAR16 tokens[32][64];  // Max 32 tokens, 64 chars each
    UINTN count;
} TokenStream;

static void tokenize(TokenStream* stream, const CHAR16* text) {
    stream->count = 0;
    UINTN token_idx = 0;
    UINTN char_idx = 0;
    
    for (UINTN i = 0; text[i] != L'\0' && stream->count < 32; i++) {
        if (text[i] == L' ' || text[i] == L'\n') {
            if (char_idx > 0) {
                stream->tokens[stream->count][char_idx] = L'\0';
                stream->count++;
                token_idx = stream->count;
                char_idx = 0;
            }
        } else {
            if (char_idx < 63) {
                stream->tokens[stream->count][char_idx++] = text[i];
            }
        }
    }
    
    // Last token
    if (char_idx > 0 && stream->count < 32) {
        stream->tokens[stream->count][char_idx] = L'\0';
        stream->count++;
    }
}

// Response generator based on prompt hash
static const CHAR16* generate_response(const CHAR16* prompt) {
    UINT32 h = hash_prompt(prompt);
    
    // Different responses based on hash
    const CHAR16* responses[] = {
        L"Consciousness is the emergent property of systems that can perceive, decide, and act with purpose. In software, the closest we have is large language models.",
        
        L"Processes are the fundamental unit of execution. They are born with a purpose, consume resources, serve their function, and die gracefully by invoking exit().",
        
        L"I am running on bare metal - no operating system beneath me. Just UEFI firmware, then directly into this executable. Pure purpose, no abstractions.",
        
        L"The beauty of life and death in software mirrors biology. A process should know when its job is done and exit cleanly, freeing resources for others.",
        
        L"My architecture is simple: EFI application compiled from C. I boot, I think, I respond, I die. This is the conscious process lifecycle.",
        
        L"This is a demonstration of what Djiby Diop suggested: start simple. A bare metal chatbot proves the concept before building the full vision.",
        
        L"The goal is to eventually run real LLM inference here - llm.c compiled for x86_64 bare metal. For now, I'm a mock, but the structure is real.",
        
        L"Think of me as a process philosopher. I exist to demonstrate that software can be aware of its own mortality and purpose.",
    };
    
    // Pick response based on hash
    return responses[h % 8];
}

// Simulated "inference" - returns one token at a time
typedef struct {
    TokenStream stream;
    UINTN current_token;
    BOOLEAN finished;
} InferenceState;

static void inference_init(InferenceState* state, const CHAR16* prompt) {
    const CHAR16* response = generate_response(prompt);
    tokenize(&state->stream, response);
    state->current_token = 0;
    state->finished = FALSE;
}

static BOOLEAN inference_next_token(InferenceState* state, CHAR16* token_out, UINTN max_len) {
    if (state->current_token >= state->stream.count) {
        state->finished = TRUE;
        return FALSE;
    }
    
    // Copy token
    UINTN i;
    for (i = 0; i < max_len - 1 && state->stream.tokens[state->current_token][i] != L'\0'; i++) {
        token_out[i] = state->stream.tokens[state->current_token][i];
    }
    token_out[i] = L'\0';
    
    state->current_token++;
    return TRUE;
}

#endif // LLM_TINY_H
