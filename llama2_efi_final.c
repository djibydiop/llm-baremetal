// REPL V3 - Full Interactive Chat Loop
// Type "quit" or "exit" to stop

#include <efi.h>
#include <efilib.h>
#include <stdint.h>

// Model config
#define DIM 288
#define HIDDEN_DIM 768
#define N_LAYERS 6
#define N_HEADS 6
#define N_KV_HEADS 6
#define VOCAB_SIZE 32000
#define SEQ_LEN 256
#define MAX_TOKENS 100

// ============================================================================
// HEAP ALLOCATOR
// ============================================================================

static char* heap_base = NULL;
static unsigned long heap_offset = 0;
static unsigned long heap_size = 0;

void* simple_alloc(unsigned long bytes) {
    if (heap_offset + bytes > heap_size) return NULL;
    void* ptr = heap_base + heap_offset;
    heap_offset += bytes;
    return ptr;
}

// ============================================================================
// MATH FUNCTIONS
// ============================================================================

float fast_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float xhalf = 0.5f * x;
    int i = *(int*)&x;
    i = 0x5f3759df - (i >> 1);
    x = *(float*)&i;
    x = x * (1.5f - xhalf * x * x);
    x = x * (1.5f - xhalf * x * x);
    return 1.0f / x;
}

float fast_exp(float x) {
    if (x < -10.0f) return 0.0f;
    if (x > 10.0f) return 22026.0f;
    x = 1.0f + x / 256.0f;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x; x *= x; x *= x;
    return x;
}

// ============================================================================
// TRANSFORMER OPERATIONS
// ============================================================================

void rmsnorm(float* o, float* x, float* weight, int size) {
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / fast_sqrt(ss);
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void matmul(float* xout, float* x, float* w, int n, int d) {
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
}

void softmax(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = fast_exp(x[i] - max_val);
        sum += x[i];
    }
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

// ============================================================================
// STRUCTURES
// ============================================================================

typedef struct {
    int dim;
    int hidden_dim;
    int n_layers;
    int n_heads;
    int n_kv_heads;
    int vocab_size;
    int seq_len;
} Config;

typedef struct {
    float* token_embedding_table;
    float* rms_att_weight;
    float* wq;
    float* wk;
    float* wv;
    float* wo;
    float* rms_ffn_weight;
    float* w1;
    float* w2;
    float* w3;
    float* rms_final_weight;
    float* wcls;
} TransformerWeights;

typedef struct {
    float* x;
    float* xb;
    float* xb2;
    float* hb;
    float* hb2;
    float* q;
    float* k;
    float* v;
    float* att;
    float* logits;
    float* key_cache;
    float* value_cache;
} RunState;

typedef struct {
    char** vocab;
    float* vocab_scores;
    int vocab_size;
    int max_token_length;
} Tokenizer;

// ============================================================================
// FORWARD PASS
// ============================================================================

void transformer_forward(RunState* s, TransformerWeights* w, Config* p, int token, int pos) {
    int dim = p->dim;
    int hidden_dim = p->hidden_dim;
    int n_layers = p->n_layers;
    int n_heads = p->n_heads;
    int head_size = dim / n_heads;
    int kv_dim = (dim * p->n_kv_heads) / n_heads;
    int kv_mul = n_heads / p->n_kv_heads;
    
    // Copy embedding
    float* content_row = w->token_embedding_table + token * dim;
    for (int i = 0; i < dim; i++) {
        s->x[i] = content_row[i];
    }
    
    // Forward all layers
    for (int l = 0; l < n_layers; l++) {
        // Attention RMSNorm
        rmsnorm(s->xb, s->x, w->rms_att_weight + l*dim, dim);
        
        // Q, K, V matrices
        matmul(s->q, s->xb, w->wq + l*dim*dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l*dim*kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l*dim*kv_dim, dim, kv_dim);
        
        // Store in KV cache
        int loff = l * p->seq_len * kv_dim;
        float* key_cache_row = s->key_cache + loff + pos * kv_dim;
        float* value_cache_row = s->value_cache + loff + pos * kv_dim;
        for (int i = 0; i < kv_dim; i++) {
            key_cache_row[i] = s->k[i];
            value_cache_row[i] = s->v[i];
        }
        
        // Multihead attention
        for (int h = 0; h < n_heads; h++) {
            float* q_h = s->q + h * head_size;
            int att_offset = h * p->seq_len;
            
            // Attention scores
            for (int t = 0; t <= pos; t++) {
                float* k_t = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) {
                    score += q_h[i] * k_t[i];
                }
                score /= fast_sqrt((float)head_size);
                s->att[att_offset + t] = score;
            }
            
            // Softmax
            softmax(s->att + att_offset, pos + 1);
            
            // Weighted sum
            float* xb_h = s->xb + h * head_size;
            for (int i = 0; i < head_size; i++) xb_h[i] = 0.0f;
            
            for (int t = 0; t <= pos; t++) {
                float* v_t = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float a = s->att[att_offset + t];
                for (int i = 0; i < head_size; i++) {
                    xb_h[i] += a * v_t[i];
                }
            }
        }
        
        // Output projection
        matmul(s->xb2, s->xb, w->wo + l*dim*dim, dim, dim);
        
        // Residual
        for (int i = 0; i < dim; i++) {
            s->x[i] += s->xb2[i];
        }
        
        // FFN RMSNorm
        rmsnorm(s->xb, s->x, w->rms_ffn_weight + l*dim, dim);
        
        // FFN
        matmul(s->hb, s->xb, w->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l*dim*hidden_dim, dim, hidden_dim);
        
        // SwiGLU
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            val *= (1.0f / (1.0f + fast_exp(-val)));
            s->hb[i] = val * s->hb2[i];
        }
        
        matmul(s->xb, s->hb, w->w2 + l*dim*hidden_dim, hidden_dim, dim);
        
        // Residual
        for (int i = 0; i < dim; i++) {
            s->x[i] += s->xb[i];
        }
    }
    
    // Final RMSNorm
    rmsnorm(s->x, s->x, w->rms_final_weight, dim);
    
    // Classifier
    matmul(s->logits, s->x, w->wcls, dim, p->vocab_size);
}

int sample(float* logits, int n) {
    int max_i = 0;
    float max_val = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > max_val) {
            max_val = logits[i];
            max_i = i;
        }
    }
    return max_i;
}

// ============================================================================
// TOKENIZER
// ============================================================================

static int my_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int my_strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

int str_lookup(char* str, char** vocab, int vocab_size) {
    for (int i = 0; i < vocab_size; i++) {
        if (vocab[i] && my_strcmp(str, vocab[i]) == 0) {
            return i;
        }
    }
    return -1;
}

void encode(char* text, int* tokens, int* n_tokens, int max_tokens, Tokenizer* t) {
    *n_tokens = 0;
    if (max_tokens <= 0) return;

    // Add BOS
    tokens[(*n_tokens)++] = 1;
    if (*n_tokens >= max_tokens) return;

    // Greedy longest-match encoding
    char* str = text;
    while (*str && *n_tokens < max_tokens) {
        int best_id = -1;
        int best_len = 0;

        for (int len = 64; len > 0; len--) {
            char piece[65];
            int i = 0;
            for (i = 0; i < len && str[i]; i++) {
                piece[i] = str[i];
            }
            if (i != len) continue; // not enough chars remaining
            piece[i] = '\0';

            int id = str_lookup(piece, t->vocab, t->vocab_size);
            if (id >= 0) {
                best_id = id;
                best_len = len;
                break;
            }
        }

        if (best_id >= 0) {
            if (*n_tokens >= max_tokens) break;
            tokens[(*n_tokens)++] = best_id;
            str += best_len;
        } else {
            char single[2];
            single[0] = *str;
            single[1] = '\0';
            int id = str_lookup(single, t->vocab, t->vocab_size);
            if (id >= 0) {
                if (*n_tokens >= max_tokens) break;
                tokens[(*n_tokens)++] = id;
            }
            str++;
        }
    }
}

// ============================================================================
// KEYBOARD INPUT
// ============================================================================

void read_user_input(CHAR16* buffer, int max_len) {
    int pos = 0;
    EFI_INPUT_KEY Key;
    
    while (pos < max_len - 1) {
        // Wait for key
        UINTN index;
        uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &index);
        uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
        
        if (Key.UnicodeChar == 0x000D) {  // Enter
            buffer[pos] = 0;
            Print(L"\r\n");
            break;
        } else if (Key.UnicodeChar == 0x0008) {  // Backspace
            if (pos > 0) {
                pos--;
                Print(L"\b \b");
            }
        } else if (Key.UnicodeChar >= 32 && Key.UnicodeChar < 127) {
            buffer[pos++] = Key.UnicodeChar;
            Print(L"%c", Key.UnicodeChar);
        }
    }
    
    buffer[pos] = 0;
}

void char16_to_char(char* dest, CHAR16* src, int max_len) {
    int i;
    for (i = 0; i < max_len - 1 && src[i]; i++) {
        dest[i] = (char)src[i];
    }
    dest[i] = 0;
}

int check_quit_command(char* text) {
    // Check for "quit" or "exit"
    if (my_strcmp(text, "quit") == 0 || my_strcmp(text, "exit") == 0) {
        return 1;
    }
    return 0;
}

void reset_kv_cache(RunState* s, Config* p) {
    // Clear KV cache for new conversation
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int cache_size = p->n_layers * p->seq_len * kv_dim;
    
    for (int i = 0; i < cache_size; i++) {
        s->key_cache[i] = 0.0f;
        s->value_cache[i] = 0.0f;
    }
}

// ============================================================================
// MAIN
// ============================================================================

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n");
    Print(L"════════════════════════════════════════\r\n");
    Print(L"  LLAMA2 CHAT REPL V3 - Full Loop\r\n");
    Print(L"════════════════════════════════════════\r\n\r\n");
    
    // ========================================================================
    // [1/7] Heap
    // ========================================================================
    
    Print(L"[1/7] Allocating heap (100MB)...\r\n");
    
    heap_size = 100 * 1024 * 1024;
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, heap_size, &heap_base);
    if (EFI_ERROR(status)) {
        Print(L"❌ Heap allocation failed\r\n");
        return status;
    }
    
    Print(L"✅ Heap ready\r\n\r\n");
    
    // ========================================================================
    // [2/7] File System
    // ========================================================================
    
    Print(L"[2/7] Opening file system...\r\n");
    
    EFI_LOADED_IMAGE *LoadedImage;
    status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &LoadedImageProtocol, &LoadedImage);
    if (EFI_ERROR(status)) {
        Print(L"❌ LoadedImage protocol failed\r\n");
        return status;
    }
    
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &FileSystemProtocol, &FileSystem);
    if (EFI_ERROR(status)) {
        Print(L"❌ FileSystem protocol failed\r\n");
        return status;
    }
    
    EFI_FILE_HANDLE Root;
    status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(status)) {
        Print(L"❌ OpenVolume failed\r\n");
        return status;
    }
    
    Print(L"✅ File system ready\r\n\r\n");
    
    // ========================================================================
    // [3/7] Load Model
    // ========================================================================
    
    Print(L"[3/7] Loading model (stories15M.bin)...\r\n");
    
    EFI_FILE_HANDLE ModelFile;
    status = uefi_call_wrapper(Root->Open, 5, Root, &ModelFile, L"stories15M.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        Print(L"❌ Model file not found\r\n");
        return status;
    }
    
    Config config;
    UINTN bytes_to_read = 7 * sizeof(int);
    uefi_call_wrapper(ModelFile->Read, 3, ModelFile, &bytes_to_read, &config);
    
    // In llama2.c format, a negative vocab_size indicates shared classifier weights.
    int shared_classifier = (config.vocab_size < 0);
    if (config.vocab_size < 0) config.vocab_size = -config.vocab_size;
    
    Print(L"✅ Model: %dM params, %d layers, %d vocab\r\n\r\n", 
          config.dim, config.n_layers, config.vocab_size);
    
    // ========================================================================
    // [4/7] Weight Pointers
    // ========================================================================
    
    Print(L"[4/7] Mapping weights...\r\n");
    if (config.vocab_size < 0) config.vocab_size = -config.vocab_size;

    int kv_dim = (config.dim * config.n_kv_heads) / config.n_heads;
    int head_size = config.dim / config.n_heads;

    // Compute total weights size (floats)
    UINTN n_floats = 0;
    n_floats += (UINTN)config.vocab_size * (UINTN)config.dim;                  // token_embedding_table
    n_floats += (UINTN)config.n_layers * (UINTN)config.dim;                    // rms_att_weight
    n_floats += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim; // wq
    n_floats += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;     // wk
    n_floats += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;     // wv
    n_floats += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim; // wo
    n_floats += (UINTN)config.n_layers * (UINTN)config.dim;                    // rms_ffn_weight
    n_floats += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim; // w1
    n_floats += (UINTN)config.n_layers * (UINTN)config.hidden_dim * (UINTN)config.dim; // w2
    n_floats += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim; // w3
    n_floats += (UINTN)config.dim;                                             // rms_final_weight
    n_floats += (UINTN)config.seq_len * (UINTN)head_size / 2;                  // freq_cis_real (skipped in mapping)
    n_floats += (UINTN)config.seq_len * (UINTN)head_size / 2;                  // freq_cis_imag (skipped in mapping)
    if (!shared_classifier) {
        n_floats += (UINTN)config.vocab_size * (UINTN)config.dim;              // wcls
    }

    bytes_to_read = n_floats * sizeof(float);
    float* weights_mem = (float*)simple_alloc(bytes_to_read);
    UINTN bytes_read_weights = bytes_to_read;
    status = uefi_call_wrapper(ModelFile->Read, 3, ModelFile, &bytes_read_weights, weights_mem);
    if (EFI_ERROR(status) || bytes_read_weights != bytes_to_read) {
        Print(L"❌ Failed to read weights (got %d bytes, expected %d)\r\n", (int)bytes_read_weights, (int)bytes_to_read);
        return EFI_LOAD_ERROR;
    }

    float* weights_ptr = weights_mem;

    TransformerWeights weights;
    weights.token_embedding_table = weights_ptr;
    weights_ptr += config.vocab_size * config.dim;
    
    weights.rms_att_weight = weights_ptr;
    weights_ptr += config.n_layers * config.dim;
    
    weights.wq = weights_ptr;
    weights_ptr += config.n_layers * config.dim * config.dim;
    
    weights.wk = weights_ptr;
    weights_ptr += config.n_layers * config.dim * kv_dim;
    
    weights.wv = weights_ptr;
    weights_ptr += config.n_layers * config.dim * kv_dim;
    
    weights.wo = weights_ptr;
    weights_ptr += config.n_layers * config.dim * config.dim;
    
    weights.rms_ffn_weight = weights_ptr;
    weights_ptr += config.n_layers * config.dim;
    
    weights.w1 = weights_ptr;
    weights_ptr += config.n_layers * config.dim * config.hidden_dim;
    
    weights.w2 = weights_ptr;
    weights_ptr += config.n_layers * config.hidden_dim * config.dim;
    
    weights.w3 = weights_ptr;
    weights_ptr += config.n_layers * config.dim * config.hidden_dim;
    
    weights.rms_final_weight = weights_ptr;
    weights_ptr += config.dim;
    
    // Skip freq_cis_real and freq_cis_imag (RoPE precomputed freqs)
    weights_ptr += config.seq_len * head_size / 2;  // freq_cis_real
    weights_ptr += config.seq_len * head_size / 2;  // freq_cis_imag
    
    weights.wcls = shared_classifier ? weights.token_embedding_table : weights_ptr;
    
    uefi_call_wrapper(ModelFile->Close, 1, ModelFile);
    
    Print(L"✅ Weights mapped\r\n\r\n");
    
    // ========================================================================
    // [5/7] State Buffers
    // ========================================================================
    
    Print(L"[5/7] Allocating state buffers...\r\n");
    
    RunState state;
    
    state.x = (float*)simple_alloc(config.dim * sizeof(float));
    state.xb = (float*)simple_alloc(config.dim * sizeof(float));
    state.xb2 = (float*)simple_alloc(config.dim * sizeof(float));
    state.hb = (float*)simple_alloc(config.hidden_dim * sizeof(float));
    state.hb2 = (float*)simple_alloc(config.hidden_dim * sizeof(float));
    state.q = (float*)simple_alloc(config.dim * sizeof(float));
    state.k = (float*)simple_alloc(kv_dim * sizeof(float));
    state.v = (float*)simple_alloc(kv_dim * sizeof(float));
    state.att = (float*)simple_alloc(config.n_heads * config.seq_len * sizeof(float));
    state.logits = (float*)simple_alloc(config.vocab_size * sizeof(float));
    state.key_cache = (float*)simple_alloc(config.n_layers * config.seq_len * kv_dim * sizeof(float));
    state.value_cache = (float*)simple_alloc(config.n_layers * config.seq_len * kv_dim * sizeof(float));
    
    Print(L"✅ State buffers allocated\r\n\r\n");
    
    // ========================================================================
    // [6/7] Tokenizer
    // ========================================================================
    
    Print(L"[6/7] Loading tokenizer...\r\n");
    
    EFI_FILE_HANDLE TokFile;
    status = uefi_call_wrapper(Root->Open, 5, Root, &TokFile, L"tokenizer.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        Print(L"❌ Tokenizer file not found\r\n");
        return status;
    }
    
    Tokenizer tokenizer;
    bytes_to_read = sizeof(int);
    uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, &tokenizer.max_token_length);
    
    tokenizer.vocab_size = config.vocab_size;
    tokenizer.vocab = (char**)simple_alloc(config.vocab_size * sizeof(char*));
    tokenizer.vocab_scores = (float*)simple_alloc(config.vocab_size * sizeof(float));
    
    for (int i = 0; i < config.vocab_size; i++) {
        bytes_to_read = sizeof(float);
        uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, &tokenizer.vocab_scores[i]);
        
        int len;
        bytes_to_read = sizeof(int);
        uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, &len);
        
        tokenizer.vocab[i] = (char*)simple_alloc(len + 1);
        bytes_to_read = len;
        uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, tokenizer.vocab[i]);
        tokenizer.vocab[i][len] = '\0';
    }
    
    uefi_call_wrapper(TokFile->Close, 1, TokFile);
    
    Print(L"✅ Tokenizer loaded (%d tokens)\r\n\r\n", tokenizer.vocab_size);
    
    // ========================================================================
    // [7/7] Interactive REPL Loop
    // ========================================================================
    
    Print(L"[7/7] Entering chat loop...\r\n\r\n");
    
    Print(L"════════════════════════════════════════\r\n");
    Print(L"  CHAT MODE ACTIVE\r\n");
    Print(L"  Type 'quit' or 'exit' to stop\r\n");
    Print(L"════════════════════════════════════════\r\n\r\n");
    
    int conversation_count = 0;
    
    // MAIN LOOP
    while (1) {
        conversation_count++;
        
        // Read user input
        CHAR16 user_input[512];
        Print(L"You: ");
        read_user_input(user_input, 512);
        
        // Convert to char
        char prompt[512];
        char16_to_char(prompt, user_input, 512);
        
        // Check for quit
        if (check_quit_command(prompt)) {
            Print(L"\r\n");
            Print(L"════════════════════════════════════════\r\n");
            Print(L"  Goodbye! Had %d conversations.\r\n", conversation_count - 1);
            Print(L"════════════════════════════════════════\r\n\r\n");
            break;
        }
        
        // Reset KV cache AND all state for new generation
        reset_kv_cache(&state, &config);
        
        // Zero out state buffers
        for (int i = 0; i < config.dim; i++) {
            state.x[i] = 0.0f;
            state.xb[i] = 0.0f;
            state.xb2[i] = 0.0f;
        }
        for (int i = 0; i < config.hidden_dim; i++) {
            state.hb[i] = 0.0f;
            state.hb2[i] = 0.0f;
        }
        
        // Encode prompt
        int prompt_tokens[256];
        int n_prompt_tokens = 0;
        encode(prompt, prompt_tokens, &n_prompt_tokens, 256, &tokenizer);
        
        Print(L"AI: ");
        Print(L"AI: ");
        
        // Process prompt tokens through model first (prefill)
        for (int i = 0; i < n_prompt_tokens; i++) {
            transformer_forward(&state, &weights, &config, prompt_tokens[i], i);
        }
        
        // Echo prompt
        for (int i = 1; i < n_prompt_tokens; i++) {
            if (tokenizer.vocab[prompt_tokens[i]]) {
                char* piece = tokenizer.vocab[prompt_tokens[i]];
                
                // Skip special tokens in echo
                if (piece[0] == '<' && my_strlen(piece) > 2) {
                    int len = my_strlen(piece);
                    if (piece[len-1] == '>') continue;
                }
                
                CHAR16 wpiece[256];
                int piece_len = my_strlen(piece);
                int copy_len = piece_len;
                if (copy_len > 255) copy_len = 255;
                for (int j = 0; j < copy_len; j++) {
                    wpiece[j] = (CHAR16)piece[j];
                }
                wpiece[copy_len] = 0;
                Print(L"%s", wpiece);
            }
        }
        
        // Start generation from the last prompt token.
        // After prefill, state.logits already corresponds to the last prompt token at position (n_prompt_tokens-1).
        int next;
        int token = prompt_tokens[n_prompt_tokens - 1];
        int pos = n_prompt_tokens - 1;
        
        int generated_count = 0;
        int repeat_count = 0;
        int last_token = -1;
        
        for (int step = 0; step < MAX_TOKENS; step++) {
            // We sample from the logits produced by the previous forward pass.
            // For step==0, logits come from the final prompt token (prefill).

            // Sample next token (greedy argmax)
            next = sample(state.logits, config.vocab_size);
            
            // Check for EOS
            if (next == 1) break;
            
            // Check if stuck on same token (per conversation)
            if (next == last_token) {
                repeat_count++;
                if (repeat_count > 5) break;
            } else {
                repeat_count = 0;
                last_token = next;
            }
            
            // Print token (ALL tokens for now, no filtering!)
            if (next >= 0 && next < config.vocab_size && tokenizer.vocab[next]) {
                char* piece = tokenizer.vocab[next];
                int len = my_strlen(piece);
                if (len > 0) {
                    CHAR16 wpiece[256];
                    int copy_len = len;
                    if (copy_len > 255) copy_len = 255;
                    for (int i = 0; i < copy_len; i++) {
                        wpiece[i] = (CHAR16)piece[i];
                    }
                    wpiece[copy_len] = 0;
                    Print(L"%s", wpiece);
                    generated_count++;
                }
            }
            
            // Advance position and compute next logits
            token = next;
            pos++;
            if (pos >= config.seq_len) break;
            transformer_forward(&state, &weights, &config, token, pos);
        }
        
        Print(L"\r\n\r\n");
    }
    
    Print(L"Press any key to exit...\r\n");
    EFI_INPUT_KEY Key;
    UINTN index;
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &index);
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
    
    return EFI_SUCCESS;
}
