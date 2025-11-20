/*
 * gpt_nano.h - Nano GPT for bare metal (no dependencies)
 * 
 * This is a REAL tiny transformer implementation:
 * - 2 layers, 2 heads, 64 dims
 * - Trained on Tiny Shakespeare
 */

#ifndef GPT_NANO_H
#define GPT_NANO_H

#include <efi.h>
#include <efilib.h>
#include "trained_weights.h"

// Config for Nano GPT (from trained_weights.h)
#define VOCAB_SIZE TRAINED_V
#define BLOCK_SIZE 64       // Hardcoded max seq len from training
#define N_EMBD TRAINED_C
#define N_HEAD TRAINED_NH
#define N_LAYER TRAINED_L
#define HEAD_SIZE (N_EMBD / N_HEAD)

// Simple math functions (no stdlib)
static float gpt_exp(float x) {
    // Fast approximation
    if (x < -10.0f) return 0.0f;
    if (x > 10.0f) return 22026.0f;
    
    float result = 1.0f;
    float term = 1.0f;
    for (int i = 1; i < 10; i++) {
        term *= x / i;
        result += term;
    }
    return result;
}

static float gpt_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    
    float guess = x;
    for (int i = 0; i < 10; i++) {
        guess = (guess + x / guess) / 2.0f;
    }
    return guess;
}

static float gpt_tanh(float x) {
    float exp2x = gpt_exp(2.0f * x);
    return (exp2x - 1.0f) / (exp2x + 1.0f);
}

static float gpt_gelu(float x) {
    return 0.5f * x * (1.0f + gpt_tanh(0.7978845608f * (x + 0.044715f * x * x * x)));
}

// Softmax
static void softmax(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = gpt_exp(x[i] - max_val);
        sum += x[i];
    }
    
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

// Layer norm
static void layer_norm(float* x, const float* gamma, const float* beta, int size) {
    float mean = 0.0f;
    for (int i = 0; i < size; i++) {
        mean += x[i];
    }
    mean /= size;
    
    float variance = 0.0f;
    for (int i = 0; i < size; i++) {
        float diff = x[i] - mean;
        variance += diff * diff;
    }
    variance /= size;
    
    float std = gpt_sqrt(variance + 1e-5f);
    for (int i = 0; i < size; i++) {
        x[i] = gamma[i] * (x[i] - mean) / std + beta[i];
    }
}

// Matrix multiplication: y = x @ w + b
// x: [in_dim], w: [in_dim * out_dim] (row-major), b: [out_dim]
// y: [out_dim]
static void matmul(float* y, const float* x, const float* w, const float* b, int in_dim, int out_dim) {
    for (int i = 0; i < out_dim; i++) {
        float val = (b != NULL) ? b[i] : 0.0f;
        for (int j = 0; j < in_dim; j++) {
            val += x[j] * w[j * out_dim + i];
        }
        y[i] = val;
    }
}

// Model weights (pointers to static data)
typedef struct {
    const float* token_embedding; // [VOCAB_SIZE * N_EMBD]
    const float* position_embedding; // [BLOCK_SIZE * N_EMBD]
    const float* ln1_gamma; // [N_LAYER * N_EMBD]
    const float* ln1_beta; // [N_LAYER * N_EMBD]
    const float* qkv_weight; // [N_LAYER * N_EMBD * 3 * N_EMBD]
    const float* qkv_bias; // [N_LAYER * 3 * N_EMBD]
    const float* att_proj_weight; // [N_LAYER * N_EMBD * N_EMBD]
    const float* att_proj_bias; // [N_LAYER * N_EMBD]
    const float* ln2_gamma; // [N_LAYER * N_EMBD]
    const float* ln2_beta; // [N_LAYER * N_EMBD]
    const float* fc_weight; // [N_LAYER * N_EMBD * 4 * N_EMBD]
    const float* fc_bias; // [N_LAYER * 4 * N_EMBD]
    const float* fc_proj_weight; // [N_LAYER * 4 * N_EMBD * N_EMBD]
    const float* fc_proj_bias; // [N_LAYER * N_EMBD]
    const float* ln_f_gamma; // [N_EMBD]
    const float* ln_f_beta; // [N_EMBD]
    int n_params;
} GPTNano;

// Initialize
static void gpt_nano_init(GPTNano* model) {
    model->token_embedding = WTE;
    model->position_embedding = WPE;
    model->ln1_gamma = LN1W;
    model->ln1_beta = LN1B;
    model->qkv_weight = QKVW;
    model->qkv_bias = QKVB;
    model->att_proj_weight = ATTPROJW;
    model->att_proj_bias = ATTPROJB;
    model->ln2_gamma = LN2W;
    model->ln2_beta = LN2B;
    model->fc_weight = FCW;
    model->fc_bias = FCB;
    model->fc_proj_weight = FCPROJW;
    model->fc_proj_bias = FCPROJB;
    model->ln_f_gamma = LNFW;
    model->ln_f_beta = LNFB;
    
    model->n_params = sizeof(WTE)/sizeof(float) + sizeof(WPE)/sizeof(float) + 
                      sizeof(LN1W)/sizeof(float) + sizeof(LN1B)/sizeof(float) +
                      sizeof(QKVW)/sizeof(float) + sizeof(QKVB)/sizeof(float) +
                      sizeof(ATTPROJW)/sizeof(float) + sizeof(ATTPROJB)/sizeof(float) +
                      sizeof(LN2W)/sizeof(float) + sizeof(LN2B)/sizeof(float) +
                      sizeof(FCW)/sizeof(float) + sizeof(FCB)/sizeof(float) +
                      sizeof(FCPROJW)/sizeof(float) + sizeof(FCPROJB)/sizeof(float) +
                      sizeof(LNFW)/sizeof(float) + sizeof(LNFB)/sizeof(float);
}

// KV Cache for attention (shared across all calls)
// Layer 0 and 1, each stores K and V for all positions in context
static float kv_cache[N_LAYER][2][BLOCK_SIZE][N_EMBD] __attribute__((aligned(16)));
static int kv_cache_len = 0;  // Current length of cached KV

// Helper: Build KV cache for all previous tokens in context
static void build_kv_cache(GPTNano* model, UINT8* context, int context_len) {
    if(context_len == 0) return;
    
    // For each position in context (except last which we'll process in main forward)
    for(int pos=0; pos<context_len-1; pos++) {
        int token = context[pos];
        
        // Embedding
        float x[N_EMBD];
        for(int i=0; i<N_EMBD; i++) {
            x[i] = model->token_embedding[token * N_EMBD + i] + 
                   model->position_embedding[pos * N_EMBD + i];
        }
        
        // Process through layers
        for(int l=0; l<N_LAYER; l++) {
            float residual[N_EMBD];
            for(int i=0; i<N_EMBD; i++) residual[i] = x[i];
            
            // LN1
            layer_norm(x, 
                       model->ln1_gamma + l * N_EMBD, 
                       model->ln1_beta + l * N_EMBD, 
                       N_EMBD);
            
            // QKV
            float qkv[3 * N_EMBD];
            matmul(qkv, x, 
                   model->qkv_weight + l * N_EMBD * 3 * N_EMBD, 
                   model->qkv_bias + l * 3 * N_EMBD, 
                   N_EMBD, 3 * N_EMBD);
            
            // Extract and cache K, V
            for(int i=0; i<N_EMBD; i++) {
                kv_cache[l][0][pos][i] = qkv[N_EMBD + i];      // K
                kv_cache[l][1][pos][i] = qkv[2 * N_EMBD + i];  // V
            }
            
            // Skip attention output since we only need cache
            // Just pass through with residual for next layer
            for(int i=0; i<N_EMBD; i++) x[i] = residual[i];
        }
    }
}

// Forward pass that returns logits (for sampling)
// abs_pos: absolute position in the sequence (for positional embedding)
// is_first_token: if true, rebuild KV cache from scratch
static void gpt_nano_forward_logits(GPTNano* model, UINT8* context, int context_len, int abs_pos, float* logits) {
    if (context_len == 0 || context_len > BLOCK_SIZE || abs_pos >= BLOCK_SIZE) {
        // Return uniform distribution
        for (int i = 0; i < VOCAB_SIZE; i++) {
            logits[i] = 0.0f;
        }
        return;
    }
    
    // Buffers
    float qkv[3 * N_EMBD] __attribute__((aligned(16)));
    float att[N_EMBD] __attribute__((aligned(16)));
    float fch[4 * N_EMBD] __attribute__((aligned(16)));
    
    // Build KV cache for previous tokens (if this is first token of new context)
    // We detect this by checking if cache is empty or context changed
    if(kv_cache_len != context_len - 1) {
        build_kv_cache(model, context, context_len);
    }
    
    // Get embedding for last token with absolute position
    int last_token = context[context_len - 1];
    float x[N_EMBD];
    for (int i = 0; i < N_EMBD; i++) {
        x[i] = model->token_embedding[last_token * N_EMBD + i] + 
               model->position_embedding[abs_pos * N_EMBD + i];
    }
    
    // Update KV cache length
    kv_cache_len = context_len;
    
    // Layers
    for (int l = 0; l < N_LAYER; l++) {
        // Save residual
        float residual[N_EMBD];
        for(int i=0; i<N_EMBD; i++) residual[i] = x[i];
        
        // LN1
        layer_norm(x, 
                   model->ln1_gamma + l * N_EMBD, 
                   model->ln1_beta + l * N_EMBD, 
                   N_EMBD);
        
        // QKV projection for current token
        matmul(qkv, x, 
               model->qkv_weight + l * N_EMBD * 3 * N_EMBD, 
               model->qkv_bias + l * 3 * N_EMBD, 
               N_EMBD, 3 * N_EMBD);
        
        // Split into Q, K, V
        float q[N_EMBD], k[N_EMBD], v[N_EMBD];
        for(int i=0; i<N_EMBD; i++) {
            q[i] = qkv[i];
            k[i] = qkv[N_EMBD + i];
            v[i] = qkv[2 * N_EMBD + i];
        }
        
        // Store K, V in cache for this position
        int cache_pos = context_len - 1;
        for(int i=0; i<N_EMBD; i++) {
            kv_cache[l][0][cache_pos][i] = k[i];  // K
            kv_cache[l][1][cache_pos][i] = v[i];  // V
        }
        
        // Compute attention: Q * K^T / sqrt(d_k)
        float scale = 1.0f / gpt_sqrt((float)(N_EMBD / N_HEAD));
        float att_scores[BLOCK_SIZE];
        float max_score = -1e10f;
        
        // Calculate attention scores with all cached K
        for(int t=0; t<kv_cache_len; t++) {
            float score = 0.0f;
            for(int i=0; i<N_EMBD; i++) {
                score += q[i] * kv_cache[l][0][t][i];
            }
            score *= scale;
            att_scores[t] = score;
            if(score > max_score) max_score = score;
        }
        
        // Softmax (numerically stable with max subtraction)
        float sum_exp = 0.0f;
        for(int t=0; t<kv_cache_len; t++) {
            att_scores[t] = gpt_exp(att_scores[t] - max_score);
            sum_exp += att_scores[t];
        }
        for(int t=0; t<kv_cache_len; t++) {
            att_scores[t] /= sum_exp;
        }
        
        // Weighted sum with V (attention output)
        for(int i=0; i<N_EMBD; i++) {
            att[i] = 0.0f;
        }
        for(int t=0; t<kv_cache_len; t++) {
            for(int i=0; i<N_EMBD; i++) {
                att[i] += att_scores[t] * kv_cache[l][1][t][i];
            }
        }
        
        // Project attention output
        float att_proj[N_EMBD];
        matmul(att_proj, att,
               model->att_proj_weight + l * N_EMBD * N_EMBD,
               model->att_proj_bias + l * N_EMBD,
               N_EMBD, N_EMBD);
               
        // Residual 1
        for(int i=0; i<N_EMBD; i++) x[i] = residual[i] + att_proj[i];
        
        // Save residual 2
        for(int i=0; i<N_EMBD; i++) residual[i] = x[i];
        
        // LN2
        layer_norm(x, 
                   model->ln2_gamma + l * N_EMBD, 
                   model->ln2_beta + l * N_EMBD, 
                   N_EMBD);
                   
        // MLP
        matmul(fch, x, 
               model->fc_weight + l * N_EMBD * 4 * N_EMBD, 
               model->fc_bias + l * 4 * N_EMBD, 
               N_EMBD, 4 * N_EMBD);
               
        // GELU
        for(int i=0; i<4*N_EMBD; i++) fch[i] = gpt_gelu(fch[i]);
        
        // Projection
        matmul(x, fch, 
               model->fc_proj_weight + l * 4 * N_EMBD * N_EMBD, 
               model->fc_proj_bias + l * N_EMBD, 
               4 * N_EMBD, N_EMBD);
               
        // Residual 2
        for(int i=0; i<N_EMBD; i++) x[i] = residual[i] + x[i];
    }
    
    // Final LN
    layer_norm(x, model->ln_f_gamma, model->ln_f_beta, N_EMBD);
    
    // Logits
    for (int i = 0; i < VOCAB_SIZE; i++) {
        logits[i] = 0.0f;
        for (int j = 0; j < N_EMBD; j++) {
            logits[i] += x[j] * model->token_embedding[i * N_EMBD + j];
        }
    }
}

#endif // GPT_NANO_H
