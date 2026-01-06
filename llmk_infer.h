#ifndef LLMK_INFER_H
#define LLMK_INFER_H

#include <efi.h>
#include <efilib.h>

#include "llmk_zones.h"
#include "llmk_sentinel.h"

#ifdef __cplusplus
extern "C" {
#endif

// Minimal inference plumbing for the LLM-Kernel track.
// Extracted from the stable REPL codepath, but built as a separate module.

typedef struct {
    int dim;
    int hidden_dim;
    int n_layers;
    int n_heads;
    int n_kv_heads;
    int vocab_size;
    int seq_len;
} LlmkConfig;

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
} LlmkTransformerWeights;

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
} LlmkRunState;

typedef struct {
    char** vocab;
    float* vocab_scores;
    int vocab_size;
    int max_token_length;
} LlmkTokenizer;

typedef struct {
    LlmkConfig cfg;
    LlmkTransformerWeights w;
    LlmkRunState st;
    LlmkTokenizer tok;
} LlmkModel;

EFI_STATUS llmk_infer_load(EFI_HANDLE ImageHandle,
                           EFI_SYSTEM_TABLE *SystemTable,
                           LlmkSentinel *sentinel,
                           EFI_FILE_HANDLE Root,
                           const CHAR16 *model_filename,
                           const CHAR16 *tokenizer_filename,
                           LlmkModel *out_model);

// Runs a small prompt->generate demo. Budgets are enforced via sentinel:
// - one prefill phase spanning the prompt
// - one decode phase per generated token
EFI_STATUS llmk_infer_demo(EFI_HANDLE ImageHandle,
                           EFI_SYSTEM_TABLE *SystemTable,
                           LlmkSentinel *sentinel,
                           EFI_FILE_HANDLE Root);

#ifdef __cplusplus
}
#endif

#endif
