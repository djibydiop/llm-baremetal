/*
 * Bare Metal LLM Chatbot with REPL
 * Based on llm.c inference code (train_gpt2.c lines 1130-1160)
 * Adapted for EFI environment without keyboard input (demo mode)
 */

#include <efi.h>
#include <efilib.h>
#include "gpt_nano.h"

// EFI globals
EFI_HANDLE ImageHandle;
EFI_SYSTEM_TABLE *SystemTable;

// Model state
GPTNano g_model;

// Tokenizer: simple char-to-byte for ASCII
// VOCAB_SIZE is defined in gpt_nano.h
#define MAX_PROMPT_LEN 64
#define MAX_GEN_TOKENS 128

// Random number generator (xorshift from llm.c)
// Reference: https://en.wikipedia.org/wiki/Xorshift#xorshift*
// Better distribution than LCG, passes statistical tests
static UINT64 g_rng_state = 1337;

static UINT32 random_u32() {
    // xorshift rng from llm.c (train_gpt2.c)
    g_rng_state ^= g_rng_state >> 12;
    g_rng_state ^= g_rng_state << 25;
    g_rng_state ^= g_rng_state >> 27;
    return (g_rng_state * 0x2545F4914F6CDD1Dull) >> 32;
}

static float random_f32() {
    // Random float in [0, 1) from llm.c
    return (random_u32() >> 8) / 16777216.0f;
}

// Softmax with temperature for sampling
static void softmax_temp(float* logits, int size, float temperature) {
    float max_val = logits[0];
    for (int i = 1; i < size; i++) {
        if (logits[i] > max_val) max_val = logits[i];
    }
    
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        logits[i] = gpt_exp((logits[i] - max_val) / temperature);
        sum += logits[i];
    }
    
    for (int i = 0; i < size; i++) {
        logits[i] /= sum;
    }
}

// Multinomial sampling (identical to llm.c implementation)
// Reference: train_gpt2.c sample_mult()
// Samples index from probability distribution
// coin is random float in [0, 1), probs must sum to 1.0
static int sample_mult(float* probs, int size, float coin) {
    float cdf = 0.0f;
    for (int i = 0; i < size; i++) {
        cdf += probs[i];
        if (coin < cdf) {
            return i;
        }
    }
    return size - 1; // in case of rounding errors
}

// Generate text from prompt
static void generate(const CHAR16* prompt_str, int max_tokens, float temperature) {
    // Convert prompt to bytes
    UINT8 prompt[MAX_PROMPT_LEN];
    int prompt_len = 0;
    
    for (int i = 0; prompt_str[i] != 0 && i < MAX_PROMPT_LEN - 1; i++) {
        if (prompt_str[i] < 256) {  // ASCII only
            prompt[prompt_len++] = (UINT8)prompt_str[i];
        }
    }
    
    if (prompt_len == 0) return;
    
    // Allocate token buffer
    UINT8* tokens = AllocatePool(sizeof(UINT8) * MAX_GEN_TOKENS);
    if (!tokens) {
        Print(L"[Error] Memory allocation failed\n");
        return;
    }
    
    // Initialize with prompt
    for (int i = 0; i < prompt_len && i < MAX_GEN_TOKENS; i++) {
        tokens[i] = prompt[i];
    }
    
    // Echo prompt
    for (int i = 0; i < prompt_len; i++) {
        Print(L"%c", (CHAR16)tokens[i]);
    }
    
    // Generate tokens autoregressively (pattern from llm.c)
    // Reference: train_gpt2.c lines 1130-1160
    for (int t = prompt_len; t < max_tokens && t < MAX_GEN_TOKENS; t++) {
        // Build context window (last BLOCK_SIZE tokens)
        UINT8 context[BLOCK_SIZE];
        int context_len = 0;
        int start_pos = (t >= BLOCK_SIZE) ? (t - BLOCK_SIZE) : 0;
        
        for (int i = start_pos; i < t; i++) {
            context[context_len++] = tokens[i];
        }
        
        // Forward pass (similar to gpt2_forward in llm.c)
        // Pass t-1 as absolute position (we're predicting token at position t)
        float logits[VOCAB_SIZE];
        gpt_nano_forward_logits(&g_model, context, context_len, t - 1, logits);
        
        // Apply temperature and softmax
        softmax_temp(logits, VOCAB_SIZE, temperature);
        
        // Sample next token (same algorithm as llm.c)
        float coin = random_f32();
        int next_token = sample_mult(logits, VOCAB_SIZE, coin);
        
        tokens[t] = (UINT8)next_token;
        
        // Print token
        if (next_token >= 32 && next_token < 127) {  // Printable ASCII
            Print(L"%c", (CHAR16)next_token);
        } else if (next_token == 10) {  // Newline
            Print(L"\n");
        }
        
        // Check for early stop (repeated chars might indicate end)
        if (t > prompt_len + 10) {
            BOOLEAN all_same = TRUE;
            for (int i = t - 5; i < t; i++) {
                if (tokens[i] != tokens[t]) {
                    all_same = FALSE;
                    break;
                }
            }
            if (all_same) break;  // Stop if repeating
        }
    }
    
    Print(L"\n");
    FreePool(tokens);
}

// Helper to read input from keyboard
static void read_user_input(CHAR16* buffer, UINTN max_len) {
    UINTN idx = 0;
    EFI_INPUT_KEY key;
    
    // Clear buffer
    for (UINTN i = 0; i < max_len; i++) buffer[i] = 0;

    while (idx < max_len - 1) {
        UINTN index;
        SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &index);
        SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
        
        if (key.UnicodeChar == L'\r') { // Enter
            Print(L"\n");
            break;
        } else if (key.UnicodeChar == 0x08) { // Backspace
            if (idx > 0) {
                idx--;
                Print(L"\b \b"); // Move back, print space, move back
            }
        } else if (key.UnicodeChar >= 32 && key.UnicodeChar < 127) {
            buffer[idx++] = key.UnicodeChar;
            Print(L"%c", key.UnicodeChar);
        }
    }
}

// Helper for mock streaming (demo purposes)
static void mock_generate(const CHAR16* text) {
    for (int i = 0; text[i] != L'\0'; i++) {
        Print(L"%c", text[i]);
        // Busy wait for delay (simulate inference speed)
        // Adjust this loop count based on CPU speed, 2M is roughly 10-20ms on fast CPUs
        for (volatile int j = 0; j < 2000000; j++) {} 
    }
    Print(L"\n");
}

// Simple string comparison
static int my_strcmp(const CHAR16* s1, const CHAR16* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(INT16*)s1 - *(INT16*)s2;
}

// REPL - Read-Eval-Print Loop
static void chatbot_repl() {
    Print(L"\n");
    Print(L"================================================\n");
    Print(L"  Bare Metal LLM Chatbot (v0.3 - Demo Mode)\n");
    Print(L"================================================\n");
    Print(L"Model: Nano GPT (%d params)\n", g_model.n_params);
    Print(L"Trained on Shakespeare dataset\n");
    Print(L"\n");
    
    // Demo mode: Test with hardcoded prompts to validate the model
    const CHAR16* test_prompts[] = {
        L"To be or not to be",
        L"Romeo and",
        L"What light through",
        L"The king",
        L"O",
        NULL
    };
    
    for (int i = 0; test_prompts[i] != NULL; i++) {
        Print(L"\n>>> Prompt: %s\n", test_prompts[i]);
        Print(L">>> Greedy (T=0.01):\n");
        generate(test_prompts[i], 64, 0.01f);  // Very low temperature = almost greedy
        Print(L"\n>>> Sampling (T=1.0):\n");
        generate(test_prompts[i], 64, 1.0f);
        Print(L"\n");
        
        // Small delay for readability
        for (volatile int j = 0; j < 50000000; j++) {}
    }
    
    Print(L"\n================================================\n");
    Print(L"Demo complete! All prompts processed.\n");
    Print(L"================================================\n");
}

// EFI entry point
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    ImageHandle = image_handle;
    SystemTable = system_table;
    
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\n");
    Print(L"Initializing Nano GPT...\n");
    
    // Initialize model
    gpt_nano_init(&g_model);
    
    Print(L"Model ready: %d parameters\n", g_model.n_params);
    Print(L"\n");
    
    // Start chatbot demo
    chatbot_repl();
    
    Print(L"\nDemo finished. System will halt in 5 seconds...\n");
    
    // Delay before exit
    for (volatile UINT64 i = 0; i < 1000000000; i++) {}
    
    return EFI_SUCCESS;
}
