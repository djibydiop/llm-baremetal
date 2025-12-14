"""
Add REPL chatbot functionality to llama2_efi.c
Based on Karpathy's chat code: https://github.com/karpathy/llama2.c/blob/350e04fe35433e6d2941dce5a1f53308f87058eb/run.c#L802
"""

REPL_TEMPLATE = '''
// ============================================================================
// REPL CHATBOT INTERFACE (Karpathy-style)
// ============================================================================

#define MAX_PROMPT_LEN 512
#define CHAT_BOS_TOKEN 1
#define CHAT_EOS_TOKEN 2
#define USER_TOKEN 1234      // Special token for "User:"
#define ASSISTANT_TOKEN 5678 // Special token for "Assistant:"

typedef struct {
    CHAR16 prompt[MAX_PROMPT_LEN];
    int prompt_tokens[MAX_PROMPT_LEN];
    int n_prompt_tokens;
    int conversation_history[2048];
    int history_len;
    int turn;
} ChatState;

void chat_init(ChatState* chat) {
    chat->n_prompt_tokens = 0;
    chat->history_len = 0;
    chat->turn = 0;
    for (int i = 0; i < MAX_PROMPT_LEN; i++) {
        chat->prompt[i] = 0;
        chat->prompt_tokens[i] = 0;
    }
}

int chat_read_prompt(ChatState* chat, EFI_SYSTEM_TABLE *ST) {
    """Read user input from keyboard"""
    Print(L"\\n>>> ");
    
    EFI_INPUT_KEY Key;
    EFI_STATUS Status;
    UINTN Index;
    int cursor = 0;
    
    while (TRUE) {
        ST->BootServices->WaitForEvent(1, &ST->ConIn->WaitForKey, &Index);
        Status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
        
        if (EFI_ERROR(Status)) continue;
        
        if (Key.UnicodeChar == 0x0D) { // Enter
            chat->prompt[cursor] = 0;
            Print(L"\\r\\n");
            return cursor;
        } else if (Key.UnicodeChar == 0x08) { // Backspace
            if (cursor > 0) {
                cursor--;
                chat->prompt[cursor] = 0;
                Print(L"\\b \\b");
            }
        } else if (Key.UnicodeChar >= 0x20 && cursor < MAX_PROMPT_LEN - 1) {
            chat->prompt[cursor] = Key.UnicodeChar;
            Print(L"%c", Key.UnicodeChar);
            cursor++;
        }
        
        // CTRL+C to interrupt (optional)
        if (Key.UnicodeChar == 0x03) {
            Print(L"^C\\r\\n");
            return -1;
        }
    }
}

void chat_encode_prompt(ChatState* chat, Tokenizer* tokenizer) {
    """Convert Unicode prompt to tokens"""
    // Simple ASCII conversion for now
    char ascii_prompt[MAX_PROMPT_LEN];
    for (int i = 0; i < MAX_PROMPT_LEN && chat->prompt[i]; i++) {
        ascii_prompt[i] = (char)chat->prompt[i];
    }
    ascii_prompt[MAX_PROMPT_LEN-1] = 0;
    
    // Tokenize using SentencePiece tokenizer
    encode(tokenizer, ascii_prompt, 1, 0, chat->prompt_tokens, &chat->n_prompt_tokens);
}

int chat_generate_response(
    ChatState* chat,
    Transformer* transformer,
    Tokenizer* tokenizer,
    float temperature,
    EFI_SYSTEM_TABLE *ST
) {
    """Generate LLM response token by token"""
    
    // Build conversation context
    int context[2048];
    int context_len = 0;
    
    // Add BOS token
    context[context_len++] = CHAT_BOS_TOKEN;
    
    // Add conversation history
    for (int i = 0; i < chat->history_len && context_len < 2000; i++) {
        context[context_len++] = chat->conversation_history[i];
    }
    
    // Add user prompt
    for (int i = 0; i < chat->n_prompt_tokens && context_len < 2000; i++) {
        context[context_len++] = chat->prompt_tokens[i];
    }
    
    Print(L"\\nAssistant: ");
    
    // Generate tokens until EOS or max length
    int max_tokens = 256;
    int generated = 0;
    int prev_token = context[context_len - 1];
    
    for (int step = 0; step < max_tokens; step++) {
        // Forward pass
        float* logits = forward(transformer, prev_token, context_len + step);
        
        // Sample next token
        int next;
        if (temperature == 0.0f) {
            next = sample_argmax(logits, transformer->config.vocab_size);
        } else {
            // Apply temperature
            for (int i = 0; i < transformer->config.vocab_size; i++) {
                logits[i] /= temperature;
            }
            softmax(logits, transformer->config.vocab_size);
            float coin = (float)rand_efi() / (float)RAND_MAX;
            next = sample_mult(logits, transformer->config.vocab_size, coin);
        }
        
        // Check for EOS
        if (next == CHAT_EOS_TOKEN) break;
        
        // Decode and print token
        char* piece = decode_token(tokenizer, prev_token, next);
        if (piece && piece[0]) {
            // Convert to Unicode and print
            for (int i = 0; piece[i]; i++) {
                Print(L"%c", (CHAR16)piece[i]);
            }
        }
        
        // Add to history
        if (chat->history_len < 2000) {
            chat->conversation_history[chat->history_len++] = next;
        }
        
        prev_token = next;
        generated++;
        
        // Check for keyboard interrupt (CTRL+C)
        EFI_INPUT_KEY Key;
        if (!EFI_ERROR(ST->ConIn->ReadKeyStroke(ST->ConIn, &Key))) {
            if (Key.UnicodeChar == 0x03) {
                Print(L" [interrupted]\\r\\n");
                break;
            }
        }
    }
    
    Print(L"\\r\\n");
    return generated;
}

void chat_repl(
    Transformer* transformer,
    Tokenizer* tokenizer,
    float temperature,
    EFI_SYSTEM_TABLE *ST
) {
    """Main REPL loop for interactive chat"""
    
    ChatState chat;
    chat_init(&chat);
    
    Print(L"\\r\\n╔════════════════════════════════════════════════════════════════╗\\r\\n");
    Print(L"║           🎭 SHAKESPEARE CHATBOT - Bare Metal REPL            ║\\r\\n");
    Print(L"╠════════════════════════════════════════════════════════════════╣\\r\\n");
    Print(L"║  Type your message and press Enter                            ║\\r\\n");
    Print(L"║  Press CTRL+C to interrupt generation                         ║\\r\\n");
    Print(L"║  Type 'exit' or 'quit' to end conversation                    ║\\r\\n");
    Print(L"╚════════════════════════════════════════════════════════════════╝\\r\\n");
    
    while (TRUE) {
        // Read user input
        int prompt_len = chat_read_prompt(&chat, ST);
        
        if (prompt_len < 0) break; // CTRL+C
        if (prompt_len == 0) continue; // Empty input
        
        // Check for exit commands
        if (chat.prompt[0] == L'e' && chat.prompt[1] == L'x' && 
            chat.prompt[2] == L'i' && chat.prompt[3] == L't') {
            Print(L"Goodbye!\\r\\n");
            break;
        }
        if (chat.prompt[0] == L'q' && chat.prompt[1] == L'u' && 
            chat.prompt[2] == L'i' && chat.prompt[3] == L't') {
            Print(L"Farewell!\\r\\n");
            break;
        }
        
        // Encode prompt
        chat_encode_prompt(&chat, tokenizer);
        
        // Generate response
        int tokens_generated = chat_generate_response(
            &chat, transformer, tokenizer, temperature, ST
        );
        
        chat.turn++;
    }
}
'''

# Instructions for integrating into llama2_efi.c
INTEGRATION_GUIDE = """
╔════════════════════════════════════════════════════════════════╗
║            REPL CHATBOT INTEGRATION GUIDE                      ║
╚════════════════════════════════════════════════════════════════╝

1. ADD REPL CODE TO llama2_efi.c:
   - Insert the REPL_TEMPLATE code above around line 4000
   - Place it after the tokenizer functions, before main()

2. MODIFY efi_main() TO USE REPL:
   
   Replace the current single-prompt generation with:
   
   // After model loading and initialization...
   Print(L"Model loaded! Starting REPL...\\r\\n");
   
   // Run interactive chatbot
   chat_repl(&transformer, &tokenizer, temperature, ST);
   
   Print(L"\\r\\nThank you for using llama2_efi!\\r\\n");
   return EFI_SUCCESS;

3. RECOMPILE:
   wsl make clean
   wsl make
   
4. DEPLOY:
   wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mcopy -i llama2_efi.img -o llama2.efi ::EFI/BOOT/BOOTX64.EFI"

5. TEST IN QEMU:
   qemu-system-x86_64 -bios OVMF.fd -drive file=llama2_efi.img,format=raw -m 2G -smp 2

6. EXPECTED BEHAVIOR:
   - Shows banner with instructions
   - >>> prompt appears
   - Type anything, press Enter
   - LLM generates response token-by-token
   - Can interrupt with CTRL+C
   - Type 'exit' or 'quit' to end

7. FOR VIRAL VIDEO (Justine's suggestion):
   - Film from Dakar (outdoor, show location)
   - Boot from USB stick on real hardware
   - Show REPL generating Shakespeare quotes
   - Post on Hacker News with title:
     "Running Llama2 LLM as bare-metal UEFI app (no OS) from Senegal"
   - Tag: @karpathy on Twitter
   - Include link to GitHub repo

This will get attention from:
- Karpathy (1.4M followers)
- Meta AI team (proud of LLaMA usage)
- HN community (loves unusual tech)
- Senegal tech community (representation matters)
"""

if __name__ == "__main__":
    print(REPL_TEMPLATE)
    print("\n" + "="*80)
    print(INTEGRATION_GUIDE)
    
    # Save template to file
    with open("repl_chatbot_template.c", "w") as f:
        f.write(REPL_TEMPLATE)
    
    print("\n✅ Saved: repl_chatbot_template.c")
    print("   Copy this into llama2_efi.c around line 4000")
