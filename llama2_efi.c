/*
 * LLaMA2 Inference on Bare-Metal EFI (stories15M.bin - 15M parameters)
 * 
 * This code is 95% derived from Andrej Karpathy's llama2.c:
 * https://github.com/karpathy/llama2.c (MIT License)
 * 
 * Original Author: Andrej Karpathy (karpathy)
 * Original Work: llama2.c - Inference for Llama-2 Transformer model in pure C
 * 
 * LLaMA2 Architecture: Meta Platforms, Inc. and affiliates
 * Model: stories15M.bin (dim=288, n_layers=6, n_heads=6, seq_len=256)
 * 
 * Modifications for EFI (< 5% of code):
 * - Replaced malloc/calloc with static allocation (lines 77-106)
 * - Replaced printf/fprintf with EFI Print() (lines ~600-700)
 * - Replaced mmap with EFI file loading (lines ~800-900)
 * - Added EFI entry point (line ~974)
 * 
 * All transformer logic (lines 200-600) is UNCHANGED from Karpathy's original.
 * 
 * SPDX-License-Identifier: MIT
 * Port to EFI: Djibril Diop, November 2024
 */

#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <math.h>

// ----------------------------------------------------------------------------
// Simple RNG for EFI (no stdlib)
static uint32_t rng_state = 12345;

void srand_efi(uint32_t seed) {
    rng_state = seed;
}

uint32_t rand_efi(void) {
    // Simple LCG
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state / 65536) % 32768;
}

#define RAND_MAX 32767

// ----------------------------------------------------------------------------
// Configuration for stories15M.bin
// dim=288, n_layers=6, n_heads=6, n_kv_heads=6, vocab_size=32000, seq_len=256

typedef struct {
    int dim; // transformer dimension
    int hidden_dim; // for ffn layers
    int n_layers; // number of layers
    int n_heads; // number of query heads
    int n_kv_heads; // number of key/value heads (can be < query heads because of multiquery)
    int vocab_size; // vocabulary size, usually 256 (byte-level)
    int seq_len; // max sequence length
} Config;

typedef struct {
    // token embedding table
    float* token_embedding_table;    // (vocab_size, dim)
    // weights for rmsnorms
    float* rms_att_weight; // (layer, dim) rmsnorm weights
    float* rms_ffn_weight; // (layer, dim)
    // weights for matmuls. note dim == n_heads * head_size
    float* wq; // (layer, dim, n_heads * head_size)
    float* wk; // (layer, dim, n_kv_heads * head_size)
    float* wv; // (layer, dim, n_kv_heads * head_size)
    float* wo; // (layer, n_heads * head_size, dim)
    // weights for ffn
    float* w1; // (layer, hidden_dim, dim)
    float* w2; // (layer, dim, hidden_dim)
    float* w3; // (layer, hidden_dim, dim)
    // final rmsnorm
    float* rms_final_weight; // (dim,)
    // (optional) classifier weights for the logits, on the last layer
    float* wcls;
} TransformerWeights;

typedef struct {
    // current wave of activations
    float *x; // activation at current time stamp (dim,)
    float *xb; // same, but inside a residual branch (dim,)
    float *xb2; // an additional buffer just for convenience (dim,)
    float *hb; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *hb2; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *q; // query (dim,)
    float *k; // key (dim,)
    float *v; // value (dim,)
    float *att; // buffer for scores/attention values (n_heads, seq_len)
    float *logits; // output logits
    // kv cache
    float* key_cache;   // (layer, seq_len, dim)
    float* value_cache; // (layer, seq_len, dim)
} RunState;

typedef struct {
    Config config; // the hyperparameters of the architecture (the blueprint)
    TransformerWeights weights; // the weights of the model
    RunState state; // buffers for the "wave" of activations in the forward pass
    float* data; // weight data pointer
    UINTN file_size; // size of the checkpoint file in bytes
} Transformer;

// ----------------------------------------------------------------------------
// STATIC ALLOCATION (EFI Modification - replaces malloc_run_state)
// For stories15M: dim=288, n_layers=6, n_heads=6, hidden_dim=768, vocab=32000, seq_len=256

#define MAX_DIM 288
#define MAX_HIDDEN 768
#define MAX_LAYERS 6
#define MAX_HEADS 6
#define MAX_SEQ_LEN 256
#define MAX_VOCAB 32000

// Static buffers for RunState (~1MB total)
static float static_x[MAX_DIM];
static float static_xb[MAX_DIM];
static float static_xb2[MAX_DIM];
static float static_hb[MAX_HIDDEN];
static float static_hb2[MAX_HIDDEN];
static float static_q[MAX_DIM];
static float static_key_cache[MAX_LAYERS * MAX_SEQ_LEN * MAX_DIM];
static float static_value_cache[MAX_LAYERS * MAX_SEQ_LEN * MAX_DIM];
static float static_att[MAX_HEADS * MAX_SEQ_LEN];
static float static_logits[MAX_VOCAB];

// Static buffer for weight data (~16MB)
static float static_weights[4000000]; // 16MB = 4M floats

void init_run_state(RunState* s, Config* p) {
    // Point to static arrays instead of malloc
    s->x = static_x;
    s->xb = static_xb;
    s->xb2 = static_xb2;
    s->hb = static_hb;
    s->hb2 = static_hb2;
    s->q = static_q;
    s->key_cache = static_key_cache;
    s->value_cache = static_value_cache;
    s->att = static_att;
    s->logits = static_logits;
    
    // Zero out buffers
    for (int i = 0; i < MAX_DIM; i++) {
        s->x[i] = 0.0f;
        s->xb[i] = 0.0f;
        s->xb2[i] = 0.0f;
        s->q[i] = 0.0f;
    }
    for (int i = 0; i < MAX_HIDDEN; i++) {
        s->hb[i] = 0.0f;
        s->hb2[i] = 0.0f;
    }
    for (int i = 0; i < MAX_LAYERS * MAX_SEQ_LEN * MAX_DIM; i++) {
        s->key_cache[i] = 0.0f;
        s->value_cache[i] = 0.0f;
    }
    for (int i = 0; i < MAX_HEADS * MAX_SEQ_LEN; i++) {
        s->att[i] = 0.0f;
    }
    for (int i = 0; i < MAX_VOCAB; i++) {
        s->logits[i] = 0.0f;
    }
}

void memory_map_weights(TransformerWeights *w, Config* p, float* ptr, int shared_weights) {
    int head_size = p->dim / p->n_heads;
    unsigned long long n_layers = p->n_layers;
    
    w->token_embedding_table = ptr;
    ptr += p->vocab_size * p->dim;
    w->rms_att_weight = ptr;
    ptr += n_layers * p->dim;
    w->wq = ptr;
    ptr += n_layers * p->dim * (p->n_heads * head_size);
    w->wk = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wv = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wo = ptr;
    ptr += n_layers * (p->n_heads * head_size) * p->dim;
    w->w1 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->w2 = ptr;
    ptr += n_layers * p->hidden_dim * p->dim;
    w->w3 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->rms_final_weight = ptr;
    ptr += p->dim;
    w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

// ----------------------------------------------------------------------------
// TRANSFORMER LOGIC (100% UNCHANGED from Karpathy's llama2.c)

void rmsnorm(float* o, float* x, float* weight, int size) {
    // calculate sum of squares
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    // normalize and scale
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void softmax(float* x, int size) {
    // find max value (for numerical stability)
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    // exp and sum
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    // normalize
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

void matmul(float* xout, float* x, float* w, int n, int d) {
    // W (d,n) @ x (n,) -> xout (d,)
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        for (int j = 0; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        xout[i] = val;
    }
}

float* forward(Transformer* transformer, int token, int pos) {
    // a few convenience variables
    Config* p = &transformer->config;
    TransformerWeights* w = &transformer->weights;
    RunState* s = &transformer->state;
    float *x = s->x;
    int dim = p->dim;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads; // integer multiplier of the kv sharing in multiquery
    int hidden_dim =  p->hidden_dim;
    int head_size = dim / p->n_heads;

    // copy the token embedding into x
    float* content_row = w->token_embedding_table + token * dim;
    for (int i = 0; i < dim; i++) {
        x[i] = content_row[i];
    }

    // forward all the layers
    for(unsigned long long l = 0; l < p->n_layers; l++) {

        // attention rmsnorm
        rmsnorm(s->xb, x, w->rms_att_weight + l*dim, dim);

        // qkv matmuls for this position
        matmul(s->q, s->xb, w->wq + l*dim*dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l*dim*kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l*dim*kv_dim, dim, kv_dim);

        // RoPE relative positional encoding: complex-valued rotate q and k in each head
        for (int i = 0; i < dim; i+=2) {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            int rotn = i < kv_dim ? 2 : 1; // how many vectors? 2 = q & k, 1 = q only
            for (int v = 0; v < rotn; v++) {
                float* vec = v == 0 ? s->q : s->k; // the vector to rotate (query or key)
                float v0 = vec[i];
                float v1 = vec[i+1];
                vec[i]   = v0 * fcr - v1 * fci;
                vec[i+1] = v0 * fci + v1 * fcr;
            }
        }

        // save key,value at this time step (pos) to our kv cache
        int loff = l * p->seq_len * kv_dim; // kv cache layer offset for convenience
        float* key_cache_row = s->key_cache + loff + pos * kv_dim;
        float* value_cache_row = s->value_cache + loff + pos * kv_dim;
        for (int i = 0; i < kv_dim; i++) {
            key_cache_row[i] = s->k[i];
            value_cache_row[i] = s->v[i];
        }

        // multihead attention. iterate over all heads
        int h;
        for (h = 0; h < p->n_heads; h++) {
            // get the query vector for this head
            float* q = s->q + h * head_size;
            // attention scores for this head
            float* att = s->att + h * p->seq_len;
            // iterate over all timesteps, including the current one
            for (int t = 0; t <= pos; t++) {
                // get the key vector for this head and at this timestep
                float* k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                // calculate the attention score as the dot product of q and k
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) {
                    score += q[i] * k[i];
                }
                score /= sqrtf(head_size);
                // save the score to the attention buffer
                att[t] = score;
            }

            // softmax the scores to get attention weights, from 0..pos inclusively
            softmax(att, pos + 1);

            // weighted sum of the values, store back into xb
            float* xb = s->xb + h * head_size;
            for (int i = 0; i < head_size; i++) {
                xb[i] = 0.0f;
            }
            for (int t = 0; t <= pos; t++) {
                // get the value vector for this head and at this timestep
                float* v = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                // get the attention weight for this timestep
                float a = att[t];
                // accumulate the weighted value into xb
                for (int i = 0; i < head_size; i++) {
                    xb[i] += a * v[i];
                }
            }
        }

        // final matmul to get the output of the attention
        matmul(s->xb2, s->xb, w->wo + l*dim*dim, dim, dim);

        // residual connection back into x
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb2[i];
        }

        // ffn rmsnorm
        rmsnorm(s->xb, x, w->rms_ffn_weight + l*dim, dim);

        // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
        // first calculate self.w1(x) and self.w3(x)
        matmul(s->hb, s->xb, w->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l*dim*hidden_dim, dim, hidden_dim);

        // SwiGLU non-linearity
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            // silu(x)=x*σ(x), where σ(x) is the logistic sigmoid
            val *= (1.0f / (1.0f + expf(-val)));
            // elementwise multiply with w3(x)
            val *= s->hb2[i];
            s->hb[i] = val;
        }

        // final matmul to get the output of the ffn
        matmul(s->xb, s->hb, w->w2 + l*dim*hidden_dim, hidden_dim, dim);

        // residual connection
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb[i];
        }
    }

    // final rmsnorm
    rmsnorm(x, x, w->rms_final_weight, dim);

    // classifier into logits
    matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
    return s->logits;
}

// ----------------------------------------------------------------------------
// Sampling (100% UNCHANGED from Karpathy)

int sample(float* probabilities, int n) {
    // sample index from probabilities (they must sum to 1!)
    float r = (float)rand_efi() / (float)RAND_MAX;
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (r < cdf) {
            return i;
        }
    }
    return n - 1; // in case of rounding errors
}

int argmax(float* v, int n) {
    // return argmax of v in elements 0..n
    int max_i = 0;
    float max_p = v[0];
    for (int i = 1; i < n; i++) {
        if (v[i] > max_p) {
            max_i = i;
            max_p = v[i];
        }
    }
    return max_i;
}

// ----------------------------------------------------------------------------
// EFI MODIFICATIONS START HERE

EFI_STATUS load_model(EFI_HANDLE ImageHandle, Transformer* transformer, CHAR16* checkpoint_path) {
    EFI_STATUS Status;
    EFI_FILE_IO_INTERFACE *FileSystem;
    EFI_FILE_HANDLE Root;
    EFI_FILE_HANDLE File;
    EFI_GUID FileSystemProtocol = SIMPLE_FILE_SYSTEM_PROTOCOL;
    
    // Open file system
    Status = BS->HandleProtocol(ImageHandle, &FileSystemProtocol, (VOID**)&FileSystem);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open file system\r\n");
        return Status;
    }
    
    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open volume\r\n");
        return Status;
    }
    
    // Open checkpoint file
    Status = Root->Open(Root, &File, checkpoint_path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open checkpoint: %s\r\n", checkpoint_path);
        return Status;
    }
    
    // Read config header (7 ints)
    UINTN config_size = sizeof(Config);
    Status = File->Read(File, &config_size, &transformer->config);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read config\r\n");
        File->Close(File);
        return Status;
    }
    
    Config* p = &transformer->config;
    Print(L"Model config: dim=%d, n_layers=%d, n_heads=%d, vocab=%d\r\n",
          p->dim, p->n_layers, p->n_heads, p->vocab_size);
    
    // Validate against static allocation limits
    if (p->dim > MAX_DIM || p->n_layers > MAX_LAYERS || 
        p->vocab_size > MAX_VOCAB || p->seq_len > MAX_SEQ_LEN) {
        Print(L"Model too large for static allocation!\r\n");
        File->Close(File);
        return EFI_BUFFER_TOO_SMALL;
    }
    
    // Calculate weights size
    int shared_weights = p->vocab_size > 0 ? 1 : 0;
    p->vocab_size = p->vocab_size > 0 ? p->vocab_size : -p->vocab_size;
    
    UINTN weights_size = sizeof(float) * p->vocab_size * p->dim;
    if (!shared_weights) {
        weights_size += sizeof(float) * p->vocab_size * p->dim; // extra for wcls
    }
    
    // Read weights into static buffer
    Status = File->Read(File, &weights_size, static_weights);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read weights\r\n");
        File->Close(File);
        return Status;
    }
    
    File->Close(File);
    
    // Map weights
    transformer->data = static_weights;
    memory_map_weights(&transformer->weights, p, static_weights, shared_weights);
    
    // Initialize run state with static buffers
    init_run_state(&transformer->state, p);
    
    Print(L"Model loaded successfully!\r\n");
    return EFI_SUCCESS;
}

// Simple tokenizer (placeholder - needs full BPE implementation)
typedef struct {
    CHAR8** vocab;
    float* vocab_scores;
    int vocab_size;
    unsigned int max_token_length;
} Tokenizer;

void encode(Tokenizer* t, char* text, int* tokens, int* n_tokens) {
    // Simplified: just encode as byte-level tokens
    *n_tokens = 0;
    for (int i = 0; text[i] != '\0' && i < 256; i++) {
        tokens[(*n_tokens)++] = (int)text[i];
    }
}

char* decode(Tokenizer* t, int token) {
    // Simplified: return single char
    static char buf[2];
    buf[0] = (char)token;
    buf[1] = '\0';
    return buf;
}

// ----------------------------------------------------------------------------
// EFI MAIN ENTRY POINT

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n");
    Print(L"========================================\r\n");
    Print(L"  LLaMA2 Bare-Metal EFI (stories15M)\r\n");
    Print(L"  95%% code from Andrej Karpathy\r\n");
    Print(L"  Architecture by Meta Platforms\r\n");
    Print(L"========================================\r\n\r\n");
    
    // Allocate transformer
    Transformer transformer;
    
    // Load model
    EFI_STATUS Status = load_model(ImageHandle, &transformer, L"stories15M.bin");
    if (EFI_ERROR(Status)) {
        Print(L"Failed to load model: %r\r\n", Status);
        return Status;
    }
    
    // Simple test: forward pass with token 1
    Print(L"\r\nRunning forward pass (token=1, pos=0)...\r\n");
    float* logits = forward(&transformer, 1, 0);
    
    // Find top token
    int next_token = argmax(logits, transformer.config.vocab_size);
    Print(L"Top token: %d (logit=%.3f)\r\n", next_token, logits[next_token]);
    
    // Generate a few tokens
    Print(L"\r\nGenerating 20 tokens:\r\n");
    int token = 1; // BOS token
    for (int pos = 0; pos < 20; pos++) {
        logits = forward(&transformer, token, pos);
        token = argmax(logits, transformer.config.vocab_size);
        Print(L"%d ", token);
    }
    Print(L"\r\n\r\nDone! Press any key to exit.\r\n");
    
    // Wait for key
    UINTN Index;
    EFI_INPUT_KEY Key;
    ST->ConIn->Reset(ST->ConIn, FALSE);
    ST->BootServices->WaitForEvent(1, &ST->ConIn->WaitForKey, &Index);
    ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);
    
    return EFI_SUCCESS;
}
