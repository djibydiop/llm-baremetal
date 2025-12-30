// REPL V3 - Full Interactive Chat Loop
// Type "quit" or "exit" to stop

#include <efi.h>
#include <efilib.h>
#include <stdint.h>

// djiblas optimized matmul
#define DJIBLAS_DISABLE_CPUID 1
#include "djiblas.h"

// Model config
#define DIM 288
#define HIDDEN_DIM 768
#define N_LAYERS 6
#define N_HEADS 6
#define N_KV_HEADS 6
#define VOCAB_SIZE 32000
#define SEQ_LEN 256
#define MAX_TOKENS 100

// Token ids used by this tiny tokenizer export.
// NOTE: encode() currently inserts BOS=1.
#define TOKEN_BOS 1
#define TOKEN_EOS 2

static int has_suffix_repeat(const int* tokens, int n_tokens, int span) {
    if (span <= 0) return 0;
    if (n_tokens < 2 * span) return 0;
    for (int i = 0; i < span; i++) {
        if (tokens[n_tokens - span + i] != tokens[n_tokens - 2 * span + i]) return 0;
    }
    return 1;
}

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

static EFI_STATUS read_exact(EFI_FILE_HANDLE file, void *dst, UINTN total_bytes) {
    UINT8 *p = (UINT8 *)dst;
    UINTN remaining = total_bytes;
    UINTN done = 0;
    UINTN next_report = 0;
    while (remaining > 0) {
        UINTN chunk = remaining;
        // Large reads can fail on some UEFI implementations; keep chunks modest.
        if (chunk > (16U * 1024U * 1024U)) chunk = (16U * 1024U * 1024U);
        UINTN got = chunk;
        EFI_STATUS st = uefi_call_wrapper(file->Read, 3, file, &got, p);
        if (EFI_ERROR(st)) return st;
        if (got == 0) return EFI_LOAD_ERROR;
        p += got;
        done += got;
        if (got > remaining) return EFI_LOAD_ERROR;
        remaining -= got;

        // Progress (avoid spamming): report every 64MB for large reads.
        if (total_bytes >= (128U * 1024U * 1024U)) {
            if (done >= next_report) {
                UINTN mb_done = done / (1024U * 1024U);
                UINTN mb_total = total_bytes / (1024U * 1024U);
                Print(L"  Reading weights... %d / %d MB\r\n", (int)mb_done, (int)mb_total);
                next_report = done + (64U * 1024U * 1024U);
            }
        }
    }
    return EFI_SUCCESS;
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

int my_strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == 0) return 0;
    }
    return 0;
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
    // DjibLAS computes (column-major): C(m×n) = A(k×m)^T · B(k×n)
    // We want (row-major weights): xout(d) = W(d×n) · x(n)
    // Trick: W(d×n) row-major has the same memory layout as B(k×n_out)
    // column-major when k=n and n_out=d (because W[i*n + l] == B[l + k*i]).
    // Use A = x as a (k×1) column-major matrix.
    // Result C is (1×d) column-major, so it lands contiguous into xout.
    djiblas_sgemm_f32(
        /*m=*/1, /*n=*/d, /*k=*/n,
        /*A=*/x, /*lda=*/n,
        /*B=*/w, /*ldb=*/n,
        /*C=*/xout, /*ldc=*/1
    );
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

// Simple PRNG for sampling
static unsigned int g_seed = 1234567;

static float randf(void) {
    g_seed = g_seed * 1664525 + 1013904223;
    return (float)(g_seed >> 8) / 16777216.0f;
}

// Sample with temperature + top-p + repetition penalty
int sample_advanced(float* logits, int n, float temperature, float top_p,
                    int* recent_tokens, int n_recent, float repeat_penalty) {
    // Apply repetition penalty
    if (repeat_penalty != 1.0f && n_recent > 0) {
        for (int i = 0; i < n_recent; i++) {
            int tok = recent_tokens[i];
            if (tok >= 0 && tok < n) {
                if (logits[tok] > 0) {
                    logits[tok] /= repeat_penalty;
                } else {
                    logits[tok] *= repeat_penalty;
                }
            }
        }
    }
    
    // Greedy if temp=0
    if (temperature <= 0.0f) {
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
    
    // Apply temperature
    for (int i = 0; i < n; i++) {
        logits[i] /= temperature;
    }
    
    // Softmax
    float max_val = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > max_val) max_val = logits[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        logits[i] = fast_exp(logits[i] - max_val);
        sum += logits[i];
    }
    for (int i = 0; i < n; i++) {
        logits[i] /= sum;
    }
    
    // Top-p (nucleus) sampling
    if (top_p < 1.0f) {
        // IMPORTANT: vocab is 32k; do NOT full-sort with O(n^2) bubble sort.
        // Approximate nucleus sampling by considering only TOP_K highest-prob tokens.
        // This makes generation responsive in UEFI/QEMU.
        #define TOP_K 128
        static int top_idx[TOP_K];
        static float top_prob[TOP_K];
        int top_count = 0;

        // Keep a descending list of the TOP_K tokens.
        for (int i = 0; i < n; i++) {
            float p = logits[i];
            if (top_count < TOP_K) {
                int j = top_count;
                while (j > 0 && top_prob[j - 1] < p) {
                    top_prob[j] = top_prob[j - 1];
                    top_idx[j] = top_idx[j - 1];
                    j--;
                }
                top_prob[j] = p;
                top_idx[j] = i;
                top_count++;
            } else if (p > top_prob[top_count - 1]) {
                int j = top_count - 1;
                while (j > 0 && top_prob[j - 1] < p) {
                    top_prob[j] = top_prob[j - 1];
                    top_idx[j] = top_idx[j - 1];
                    j--;
                }
                top_prob[j] = p;
                top_idx[j] = i;
            }
        }

        float mass = 0.0f;
        int cutoff = 0;
        for (int i = 0; i < top_count; i++) {
            mass += top_prob[i];
            cutoff++;
            if (mass >= top_p) break;
        }
        if (cutoff < 1) cutoff = 1;

        // Sample from the nucleus subset.
        float r = randf() * mass;
        float cdf = 0.0f;
        for (int i = 0; i < cutoff; i++) {
            cdf += top_prob[i];
            if (r < cdf) {
                return top_idx[i];
            }
        }
        return top_idx[cutoff - 1];
        #undef TOP_K
    }
    
    // Sample from distribution
    float r = randf();
    float cumsum = 0.0f;
    for (int i = 0; i < n; i++) {
        cumsum += logits[i];
        if (r < cumsum) {
            return i;
        }
    }
    
    return n - 1;
}

int sample(float* logits, int n) {
    // Simple greedy for now (kept for compatibility)
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
    tokens[(*n_tokens)++] = TOKEN_BOS;
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

    // Disable the UEFI watchdog timer (large model loads can take minutes).
    // If not disabled, firmware may reset/reboot mid-load and it looks like a hang.
    uefi_call_wrapper(BS->SetWatchdogTimer, 4, 0, 0, 0, NULL);
    
    Print(L"\r\n");
    Print(L"----------------------------------------\r\n");
    Print(L"  LLAMA2 CHAT REPL V3 - Full Loop\r\n");
    Print(L"----------------------------------------\r\n\r\n");
    
    // ========================================================================
    // [1/7] File System
    // ========================================================================
    
    Print(L"[1/7] Opening file system...\r\n");
    
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_STATUS status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &LoadedImageProtocol, &LoadedImage);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: LoadedImage protocol failed\r\n");
        return status;
    }
    
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &FileSystemProtocol, &FileSystem);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: FileSystem protocol failed\r\n");
        return status;
    }
    
    EFI_FILE_HANDLE Root;
    status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: OpenVolume failed\r\n");
        return status;
    }
    
    Print(L"OK: File system ready\r\n\r\n");

    // CPU feature detection (djiblas) - TEMPORARILY DISABLED (CPUID issue in UEFI)
    // Print(L"[1.5/7] Detecting CPU features...\r\n");
    // CPUFeatures cpu_features;
    // djiblas_detect_cpu(&cpu_features);
    Print(L"[DJIBLAS] Using optimized SGEMM (SSE2 baseline)\r\n\r\n");
    
    // ========================================================================
    // [2/7] Load Model Header
    // ========================================================================
    
    Print(L"[2/7] Loading model...\r\n");
    
    EFI_FILE_HANDLE ModelFile;
    CHAR16 *model_filename = L"stories110M.bin";
    status = uefi_call_wrapper(Root->Open, 5, Root, &ModelFile, model_filename, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        model_filename = L"stories15M.bin";
        status = uefi_call_wrapper(Root->Open, 5, Root, &ModelFile, model_filename, EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(status)) {
            Print(L"ERROR: Model file not found (expected stories110M.bin or stories15M.bin)\r\n");
            return status;
        }
    }
    
    Config config;
    UINTN bytes_to_read = 7 * sizeof(int);
    uefi_call_wrapper(ModelFile->Read, 3, ModelFile, &bytes_to_read, &config);
    
    // In llama2.c format, a negative vocab_size indicates shared classifier weights.
    int shared_classifier = (config.vocab_size < 0);
    if (config.vocab_size < 0) config.vocab_size = -config.vocab_size;

    // Some exported model files may *still* share classifier weights even if vocab_size is positive.
    // Detect this by comparing expected weights size vs actual file size.
    UINT64 model_file_size = 0;
    {
        EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
        UINTN info_size = 0;
        EFI_STATUS st = uefi_call_wrapper(ModelFile->GetInfo, 4, ModelFile, &FileInfoGuid, &info_size, NULL);
        if (st == EFI_BUFFER_TOO_SMALL && info_size > 0) {
            EFI_FILE_INFO *info = NULL;
            st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, info_size, (void **)&info);
            if (!EFI_ERROR(st) && info) {
                st = uefi_call_wrapper(ModelFile->GetInfo, 4, ModelFile, &FileInfoGuid, &info_size, info);
                if (!EFI_ERROR(st)) {
                    model_file_size = info->FileSize;
                }
                uefi_call_wrapper(BS->FreePool, 1, info);
            }
        }
    }
    
        Print(L"OK: Model loaded: %s (dim=%d, layers=%d, heads=%d, kv=%d, vocab=%d, seq=%d)\r\n\r\n",
                    model_filename, config.dim, config.n_layers, config.n_heads, config.n_kv_heads, config.vocab_size, config.seq_len);

    // ========================================================================
    // [3/7] Heap (auto-sized)
    // ========================================================================

    int kv_dim = (config.dim * config.n_kv_heads) / config.n_heads;
    int head_size = config.dim / config.n_heads;

    // Compute total weights size (floats)
    UINTN n_floats_base = 0;
    n_floats_base += (UINTN)config.vocab_size * (UINTN)config.dim;                   // token_embedding_table
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim;                     // rms_att_weight
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim; // wq
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;     // wk
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;     // wv
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim; // wo
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim;                     // rms_ffn_weight
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim; // w1
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.hidden_dim * (UINTN)config.dim; // w2
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim; // w3
    n_floats_base += (UINTN)config.dim;                                              // rms_final_weight
    n_floats_base += (UINTN)config.seq_len * (UINTN)head_size / 2;                   // freq_cis_real
    n_floats_base += (UINTN)config.seq_len * (UINTN)head_size / 2;                   // freq_cis_imag

    UINTN n_floats_with_cls = n_floats_base + (UINTN)config.vocab_size * (UINTN)config.dim;

    // If file size is known, use it to infer whether wcls is present.
    if (model_file_size > 0) {
        UINT64 available = model_file_size;
        UINT64 header_bytes = (UINT64)(7 * sizeof(int));
        if (available > header_bytes) available -= header_bytes;
        UINT64 bytes_base = (UINT64)n_floats_base * sizeof(float);
        UINT64 bytes_with = (UINT64)n_floats_with_cls * sizeof(float);

        if (available < bytes_with && available >= bytes_base) {
            shared_classifier = 1;
        } else if (available >= bytes_with) {
            shared_classifier = 0;
        }
    }

    UINTN n_floats = shared_classifier ? n_floats_base : n_floats_with_cls;
    UINTN weights_bytes = n_floats * sizeof(float);
    UINTN state_bytes = 0;
    state_bytes += (UINTN)config.dim * sizeof(float) * 3; // x, xb, xb2
    state_bytes += (UINTN)config.hidden_dim * sizeof(float) * 2; // hb, hb2
    state_bytes += (UINTN)config.dim * sizeof(float); // q
    state_bytes += (UINTN)kv_dim * sizeof(float) * 2; // k, v
    state_bytes += (UINTN)config.n_heads * (UINTN)config.seq_len * sizeof(float); // att
    state_bytes += (UINTN)config.vocab_size * sizeof(float); // logits
    state_bytes += (UINTN)config.n_layers * (UINTN)config.seq_len * (UINTN)kv_dim * sizeof(float) * 2; // key/value cache

    // Tokenizer: pointers + scores + strings (strings size varies; reserve a safe budget)
    UINTN tokenizer_bytes = (UINTN)config.vocab_size * (sizeof(char*) + sizeof(float));
    tokenizer_bytes += 4 * 1024 * 1024; // string storage budget

    UINTN slack_bytes = 16 * 1024 * 1024;
    heap_size = weights_bytes + state_bytes + tokenizer_bytes + slack_bytes;
    if (heap_size < 100ULL * 1024ULL * 1024ULL) heap_size = 100ULL * 1024ULL * 1024ULL;

    Print(L"[3/7] Allocating heap (%d MB)...\r\n", (int)(heap_size / (1024 * 1024)));
    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, heap_size, &heap_base);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Heap allocation failed (need more RAM). Try QEMU -m 2048M for 110M.\r\n");
        return status;
    }
    heap_offset = 0;
    Print(L"OK: Heap ready\r\n\r\n");
    
    // ========================================================================
    // [4/7] Weight Pointers
    // ========================================================================
    
    Print(L"[4/7] Mapping weights...\r\n");
    bytes_to_read = weights_bytes;
    float* weights_mem = (float*)simple_alloc(bytes_to_read);
    if (weights_mem == NULL) {
        Print(L"ERROR: Out of heap while allocating weights (%d MB needed)\r\n", (int)(bytes_to_read / (1024 * 1024)));
        return EFI_OUT_OF_RESOURCES;
    }
    status = read_exact(ModelFile, weights_mem, bytes_to_read);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to read weights (need model file + enough RAM).\r\n");
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
    
    Print(L"OK: Weights mapped\r\n\r\n");
    
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
    
    Print(L"OK: State buffers allocated\r\n\r\n");
    
    // ========================================================================
    // [6/7] Tokenizer
    // ========================================================================
    
    Print(L"[6/7] Loading tokenizer...\r\n");
    
    EFI_FILE_HANDLE TokFile;
    status = uefi_call_wrapper(Root->Open, 5, Root, &TokFile, L"tokenizer.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Tokenizer file not found\r\n");
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
    
    Print(L"OK: Tokenizer loaded (%d tokens)\r\n\r\n", tokenizer.vocab_size);
    
    // ========================================================================
    // [7/7] Interactive REPL Loop
    // ========================================================================
    
    Print(L"[7/7] Entering chat loop...\r\n\r\n");
    
    Print(L"----------------------------------------\r\n");
    Print(L"  CHAT MODE ACTIVE\r\n");
    Print(L"  Type 'quit' or 'exit' to stop\r\n");
    Print(L"  Commands: /temp /top_p /repeat /help\r\n");
    Print(L"----------------------------------------\r\n\r\n");
    
    // Sampling parameters
    float temperature = 0.8f;
    float top_p = 0.9f;
    float repeat_penalty = 1.1f;
    
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
            Print(L"----------------------------------------\r\n");
            Print(L"  Goodbye! Had %d conversations.\r\n", conversation_count - 1);
            Print(L"----------------------------------------\r\n\r\n");
            break;
        }
        
        // Check for commands
        if (prompt[0] == '/') {
            if (my_strncmp(prompt, "/temp ", 6) == 0) {
                float val = 0.0f;
                int i = 6;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10.0f + (prompt[i] - '0');
                    i++;
                }
                if (prompt[i] == '.') {
                    i++;
                    float frac = 0.1f;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        val += (prompt[i] - '0') * frac;
                        frac /= 10.0f;
                        i++;
                    }
                }
                temperature = val;
                Print(L"  Temperature set to: ");
                Print(L"%d.", (int)temperature);
                Print(L"%d\r\n", (int)((temperature - (int)temperature) * 100.0f));
                continue;
            } else if (my_strncmp(prompt, "/top_p ", 7) == 0) {
                float val = 0.0f;
                int i = 7;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10.0f + (prompt[i] - '0');
                    i++;
                }
                if (prompt[i] == '.') {
                    i++;
                    float frac = 0.1f;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        val += (prompt[i] - '0') * frac;
                        frac /= 10.0f;
                        i++;
                    }
                }
                top_p = val;
                Print(L"  Top-p set to: ");
                Print(L"%d.", (int)top_p);
                Print(L"%d\r\n", (int)((top_p - (int)top_p) * 100.0f));
                continue;
            } else if (my_strncmp(prompt, "/repeat ", 8) == 0) {
                float val = 0.0f;
                int i = 8;
                while (prompt[i] >= '0' && prompt[i] <= '9') {
                    val = val * 10.0f + (prompt[i] - '0');
                    i++;
                }
                if (prompt[i] == '.') {
                    i++;
                    float frac = 0.1f;
                    while (prompt[i] >= '0' && prompt[i] <= '9') {
                        val += (prompt[i] - '0') * frac;
                        frac /= 10.0f;
                        i++;
                    }
                }
                repeat_penalty = val;
                Print(L"  Repetition penalty set to: ");
                Print(L"%d.", (int)repeat_penalty);
                Print(L"%d\r\n", (int)((repeat_penalty - (int)repeat_penalty) * 100.0f));
                continue;
            } else if (my_strncmp(prompt, "/help", 5) == 0) {
                Print(L"\r\nCommands:\r\n");
                Print(L"  /temp <val>   - Set temperature (0.0=greedy, 1.0=creative)\r\n");
                Print(L"  /top_p <val>  - Set nucleus sampling (0.0-1.0)\r\n");
                Print(L"  /repeat <val> - Set repetition penalty (1.0=none, 1.5=strong)\r\n");
                Print(L"  /help         - Show this help\r\n\r\n");
                Print(L"Current settings:\r\n");
                Print(L"  Temperature: ");
                Print(L"%d.", (int)temperature);
                Print(L"%d\r\n", (int)((temperature - (int)temperature) * 100.0f));
                Print(L"  Top-p: ");
                Print(L"%d.", (int)top_p);
                Print(L"%d\r\n", (int)((top_p - (int)top_p) * 100.0f));
                Print(L"  Repeat penalty: ");
                Print(L"%d.", (int)repeat_penalty);
                Print(L"%d\r\n\r\n", (int)((repeat_penalty - (int)repeat_penalty) * 100.0f));
                continue;
            }
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
        
        // Process prompt tokens through model first (prefill)
        for (int i = 0; i < n_prompt_tokens; i++) {
            transformer_forward(&state, &weights, &config, prompt_tokens[i], i);
        }
        
        // Start generation from the last prompt token.
        // After prefill, state.logits already corresponds to the last prompt token at position (n_prompt_tokens-1).
        int next;
        int token = prompt_tokens[n_prompt_tokens - 1];
        int pos = n_prompt_tokens - 1;
        
        int generated_count = 0;
        int repeat_count = 0;
        int last_token = -1;
        
        // Track context for repetition penalty and loop detection.
        int context_tokens[256 + MAX_TOKENS];
        int n_context_tokens = 0;
        for (int i = 0; i < n_prompt_tokens && n_context_tokens < (int)(sizeof(context_tokens) / sizeof(context_tokens[0])); i++) {
            context_tokens[n_context_tokens++] = prompt_tokens[i];
        }

        for (int step = 0; step < MAX_TOKENS; step++) {
            // We sample from the logits produced by the previous forward pass.
            // For step==0, logits come from the final prompt token (prefill).

            // Sample next token (temperature/top_p + repetition penalty)
            int n_recent = n_context_tokens;
            if (n_recent > 64) n_recent = 64;
            int* recent = (n_recent > 0) ? &context_tokens[n_context_tokens - n_recent] : (int*)0;
            next = sample_advanced(state.logits, config.vocab_size, temperature, top_p, recent, n_recent, repeat_penalty);
            
            // Check for EOS (some exports may still emit BOS; treat both as stop)
            if (next == TOKEN_EOS || next == TOKEN_BOS) break;
            
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

            // Append to context and apply a simple loop-stop heuristic.
            if (n_context_tokens < (int)(sizeof(context_tokens) / sizeof(context_tokens[0]))) {
                context_tokens[n_context_tokens++] = next;
            }
            // Stop if the tail repeats (common failure mode: short loops).
            // spans chosen to be cheap and effective in practice.
            if (has_suffix_repeat(context_tokens, n_context_tokens, 8) ||
                has_suffix_repeat(context_tokens, n_context_tokens, 12) ||
                has_suffix_repeat(context_tokens, n_context_tokens, 16)) {
                break;
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
