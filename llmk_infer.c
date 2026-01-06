#include "llmk_infer.h"

#include "djiblas.h"
#include "llmk_log.h"

// Token ids used by tokenizer export.
#define LLMK_TOKEN_BOS 1
#define LLMK_TOKEN_EOS 2

static UINT64 llmk_rdtsc(void) {
    UINT32 lo, hi;
    __asm__ __volatile__("lfence\nrdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return ((UINT64)hi << 32) | (UINT64)lo;
}

static float fast_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float xhalf = 0.5f * x;
    int i = *(int*)&x;
    i = 0x5f3759df - (i >> 1);
    x = *(float*)&i;
    x = x * (1.5f - xhalf * x * x);
    x = x * (1.5f - xhalf * x * x);
    return 1.0f / x;
}

static float fast_exp(float x) {
    if (x < -10.0f) return 0.0f;
    if (x > 10.0f) return 22026.0f;
    x = 1.0f + x / 256.0f;
    x *= x; x *= x; x *= x; x *= x;
    x *= x; x *= x; x *= x; x *= x;
    return x;
}

static void rmsnorm(float* o, float* x, float* weight, int size) {
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= (float)size;
    ss += 1e-5f;
    ss = 1.0f / fast_sqrt(ss);
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

static void matmul(float* xout, float* x, float* w, int n, int d) {
    // DjibLAS computes (column-major): C(m×n) = A(k×m)^T · B(k×n)
    // Here we want xout(d) = W(d×n) · x(n)
    djiblas_sgemm_f32(
        /*m=*/1, /*n=*/d, /*k=*/n,
        /*A=*/x, /*lda=*/n,
        /*B=*/w, /*ldb=*/n,
        /*C=*/xout, /*ldc=*/1
    );
}

static void softmax(float* x, int size) {
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

static int my_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static int my_strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int str_lookup(char* str, char** vocab, int vocab_size) {
    for (int i = 0; i < vocab_size; i++) {
        if (vocab[i] && my_strcmp(str, vocab[i]) == 0) return i;
    }
    return -1;
}

static void encode(char* text, int* tokens, int* n_tokens, int max_tokens, LlmkTokenizer* t) {
    *n_tokens = 0;
    if (max_tokens <= 0) return;

    // BOS
    tokens[(*n_tokens)++] = LLMK_TOKEN_BOS;
    if (*n_tokens >= max_tokens) return;

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
            if (i != len) continue;
            piece[i] = '\0';

            int id = str_lookup(piece, t->vocab, t->vocab_size);
            if (id >= 0) {
                best_id = id;
                best_len = len;
                break;
            }
        }

        if (best_id >= 0) {
            tokens[(*n_tokens)++] = best_id;
            str += best_len;
        } else {
            char single[2];
            single[0] = *str;
            single[1] = '\0';
            int id = str_lookup(single, t->vocab, t->vocab_size);
            if (id >= 0) {
                tokens[(*n_tokens)++] = id;
            }
            str++;
        }
    }
}

static EFI_STATUS read_exact(EFI_FILE_HANDLE file, void *dst, UINTN total_bytes) {
    UINT8 *p = (UINT8 *)dst;
    UINTN remaining = total_bytes;

    while (remaining > 0) {
        UINTN chunk = remaining;
        if (chunk > (16U * 1024U * 1024U)) chunk = (16U * 1024U * 1024U);
        UINTN got = chunk;
        EFI_STATUS st = uefi_call_wrapper(file->Read, 3, file, &got, p);
        if (EFI_ERROR(st)) return st;
        if (got == 0) return EFI_LOAD_ERROR;
        if (got > remaining) return EFI_LOAD_ERROR;
        p += got;
        remaining -= got;
    }
    return EFI_SUCCESS;
}

static void transformer_forward(LlmkRunState* s, LlmkTransformerWeights* w, LlmkConfig* p, int token, int pos) {
    int dim = p->dim;
    int hidden_dim = p->hidden_dim;
    int n_layers = p->n_layers;
    int n_heads = p->n_heads;
    int head_size = dim / n_heads;
    int kv_dim = (dim * p->n_kv_heads) / n_heads;
    int kv_mul = n_heads / p->n_kv_heads;

    float* content_row = w->token_embedding_table + (UINTN)token * (UINTN)dim;
    for (int i = 0; i < dim; i++) s->x[i] = content_row[i];

    for (int l = 0; l < n_layers; l++) {
        rmsnorm(s->xb, s->x, w->rms_att_weight + (UINTN)l * (UINTN)dim, dim);

        matmul(s->q, s->xb, w->wq + (UINTN)l * (UINTN)dim * (UINTN)dim, dim, dim);
        matmul(s->k, s->xb, w->wk + (UINTN)l * (UINTN)dim * (UINTN)kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + (UINTN)l * (UINTN)dim * (UINTN)kv_dim, dim, kv_dim);

        int loff = l * p->seq_len * kv_dim;
        float* key_cache_row = s->key_cache + (UINTN)loff + (UINTN)pos * (UINTN)kv_dim;
        float* value_cache_row = s->value_cache + (UINTN)loff + (UINTN)pos * (UINTN)kv_dim;
        for (int i = 0; i < kv_dim; i++) {
            key_cache_row[i] = s->k[i];
            value_cache_row[i] = s->v[i];
        }

        for (int h = 0; h < n_heads; h++) {
            float* q_h = s->q + (UINTN)h * (UINTN)head_size;
            int att_offset = h * p->seq_len;

            for (int t = 0; t <= pos; t++) {
                float* k_t = s->key_cache + (UINTN)loff + (UINTN)t * (UINTN)kv_dim + (UINTN)(h / kv_mul) * (UINTN)head_size;
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) score += q_h[i] * k_t[i];
                score /= fast_sqrt((float)head_size);
                s->att[att_offset + t] = score;
            }

            softmax(s->att + att_offset, pos + 1);

            float* xb_h = s->xb + (UINTN)h * (UINTN)head_size;
            for (int i = 0; i < head_size; i++) xb_h[i] = 0.0f;

            for (int t = 0; t <= pos; t++) {
                float* v_t = s->value_cache + (UINTN)loff + (UINTN)t * (UINTN)kv_dim + (UINTN)(h / kv_mul) * (UINTN)head_size;
                float a = s->att[att_offset + t];
                for (int i = 0; i < head_size; i++) xb_h[i] += a * v_t[i];
            }
        }

        matmul(s->xb2, s->xb, w->wo + (UINTN)l * (UINTN)dim * (UINTN)dim, dim, dim);
        for (int i = 0; i < dim; i++) s->x[i] += s->xb2[i];

        rmsnorm(s->xb, s->x, w->rms_ffn_weight + (UINTN)l * (UINTN)dim, dim);

        matmul(s->hb, s->xb, w->w1 + (UINTN)l * (UINTN)dim * (UINTN)hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + (UINTN)l * (UINTN)dim * (UINTN)hidden_dim, dim, hidden_dim);

        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            val *= (1.0f / (1.0f + fast_exp(-val)));
            s->hb[i] = val * s->hb2[i];
        }

        matmul(s->xb, s->hb, w->w2 + (UINTN)l * (UINTN)hidden_dim * (UINTN)dim, hidden_dim, dim);
        for (int i = 0; i < dim; i++) s->x[i] += s->xb[i];
    }

    rmsnorm(s->x, s->x, w->rms_final_weight, dim);
    matmul(s->logits, s->x, w->wcls, dim, p->vocab_size);
}

static int sample_greedy(float* logits, int n) {
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

static EFI_STATUS load_tokenizer(EFI_FILE_HANDLE Root,
                                LlmkSentinel *sentinel,
                                const CHAR16 *tokenizer_filename,
                                int vocab_size,
                                LlmkTokenizer *out_tok) {
    if (!Root || !sentinel || !out_tok) return EFI_INVALID_PARAMETER;

    EFI_FILE_HANDLE TokFile;
    EFI_STATUS st = uefi_call_wrapper(Root->Open, 5, Root, &TokFile, (CHAR16*)tokenizer_filename, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) return st;

    UINTN bytes_to_read = sizeof(int);
    st = uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, &out_tok->max_token_length);
    if (EFI_ERROR(st)) {
        uefi_call_wrapper(TokFile->Close, 1, TokFile);
        return st;
    }

    out_tok->vocab_size = vocab_size;
    out_tok->vocab = (char**)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)vocab_size * sizeof(char*), 8, L"tok vocab ptrs");
    out_tok->vocab_scores = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)vocab_size * sizeof(float), 16, L"tok scores");
    if (sentinel->tripped || !out_tok->vocab || !out_tok->vocab_scores) {
        uefi_call_wrapper(TokFile->Close, 1, TokFile);
        return EFI_OUT_OF_RESOURCES;
    }

    for (int i = 0; i < vocab_size; i++) {
        bytes_to_read = sizeof(float);
        st = uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, &out_tok->vocab_scores[i]);
        if (EFI_ERROR(st)) break;

        int len = 0;
        bytes_to_read = sizeof(int);
        st = uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, &len);
        if (EFI_ERROR(st)) break;
        if (len < 0 || len > 1024) {
            st = EFI_LOAD_ERROR;
            break;
        }

        out_tok->vocab[i] = (char*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)len + 1ULL, 1, L"tok str");
        if (sentinel->tripped || !out_tok->vocab[i]) {
            st = EFI_OUT_OF_RESOURCES;
            break;
        }

        bytes_to_read = (UINTN)len;
        st = uefi_call_wrapper(TokFile->Read, 3, TokFile, &bytes_to_read, out_tok->vocab[i]);
        if (EFI_ERROR(st)) break;
        out_tok->vocab[i][len] = '\0';
    }

    uefi_call_wrapper(TokFile->Close, 1, TokFile);
    return st;
}

EFI_STATUS llmk_infer_load(EFI_HANDLE ImageHandle,
                           EFI_SYSTEM_TABLE *SystemTable,
                           LlmkSentinel *sentinel,
                           EFI_FILE_HANDLE Root,
                           const CHAR16 *model_filename,
                           const CHAR16 *tokenizer_filename,
                           LlmkModel *out_model) {
    (void)ImageHandle;
    (void)SystemTable;

    if (!sentinel || !Root || !model_filename || !tokenizer_filename || !out_model) return EFI_INVALID_PARAMETER;
    if (sentinel->tripped) return EFI_ABORTED;

    EFI_FILE_HANDLE ModelFile;
    EFI_STATUS st = uefi_call_wrapper(Root->Open, 5, Root, &ModelFile, (CHAR16*)model_filename, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) return st;

    LlmkConfig config;
    UINTN bytes_to_read = 7 * sizeof(int);
    st = uefi_call_wrapper(ModelFile->Read, 3, ModelFile, &bytes_to_read, &config);
    if (EFI_ERROR(st) || bytes_to_read != 7 * sizeof(int)) {
        uefi_call_wrapper(ModelFile->Close, 1, ModelFile);
        return EFI_LOAD_ERROR;
    }

    int shared_classifier = (config.vocab_size < 0);
    if (config.vocab_size < 0) config.vocab_size = -config.vocab_size;

    UINT64 model_file_size = 0;
    {
        EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
        UINTN info_size = 0;
        EFI_STATUS st_info = uefi_call_wrapper(ModelFile->GetInfo, 4, ModelFile, &FileInfoGuid, &info_size, NULL);
        if (st_info == EFI_BUFFER_TOO_SMALL && info_size > 0) {
            EFI_FILE_INFO *info = NULL;
            st_info = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, info_size, (void **)&info);
            if (!EFI_ERROR(st_info) && info) {
                st_info = uefi_call_wrapper(ModelFile->GetInfo, 4, ModelFile, &FileInfoGuid, &info_size, info);
                if (!EFI_ERROR(st_info)) {
                    model_file_size = info->FileSize;
                }
                uefi_call_wrapper(SystemTable->BootServices->FreePool, 1, info);
            }
        }
    }

    int kv_dim = (config.dim * config.n_kv_heads) / config.n_heads;
    int head_size = config.dim / config.n_heads;

    UINTN n_floats_base = 0;
    n_floats_base += (UINTN)config.vocab_size * (UINTN)config.dim;
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim;
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim;
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim;
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim;
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim;
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.hidden_dim * (UINTN)config.dim;
    n_floats_base += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim;
    n_floats_base += (UINTN)config.dim;
    n_floats_base += (UINTN)config.seq_len * (UINTN)head_size / 2;
    n_floats_base += (UINTN)config.seq_len * (UINTN)head_size / 2;

    UINTN n_floats_with_cls = n_floats_base + (UINTN)config.vocab_size * (UINTN)config.dim;

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

    float* weights_mem = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_WEIGHTS, (UINT64)weights_bytes, 64, L"weights");
    if (sentinel->tripped || !weights_mem) {
        uefi_call_wrapper(ModelFile->Close, 1, ModelFile);
        return EFI_OUT_OF_RESOURCES;
    }

    st = read_exact(ModelFile, weights_mem, weights_bytes);
    uefi_call_wrapper(ModelFile->Close, 1, ModelFile);
    if (EFI_ERROR(st)) return st;

    float* weights_ptr = weights_mem;
    LlmkTransformerWeights weights;
    weights.token_embedding_table = weights_ptr;
    weights_ptr += (UINTN)config.vocab_size * (UINTN)config.dim;
    weights.rms_att_weight = weights_ptr;
    weights_ptr += (UINTN)config.n_layers * (UINTN)config.dim;
    weights.wq = weights_ptr;
    weights_ptr += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim;
    weights.wk = weights_ptr;
    weights_ptr += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;
    weights.wv = weights_ptr;
    weights_ptr += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)kv_dim;
    weights.wo = weights_ptr;
    weights_ptr += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.dim;
    weights.rms_ffn_weight = weights_ptr;
    weights_ptr += (UINTN)config.n_layers * (UINTN)config.dim;
    weights.w1 = weights_ptr;
    weights_ptr += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim;
    weights.w2 = weights_ptr;
    weights_ptr += (UINTN)config.n_layers * (UINTN)config.hidden_dim * (UINTN)config.dim;
    weights.w3 = weights_ptr;
    weights_ptr += (UINTN)config.n_layers * (UINTN)config.dim * (UINTN)config.hidden_dim;
    weights.rms_final_weight = weights_ptr;
    weights_ptr += (UINTN)config.dim;

    // Skip RoPE freqs
    weights_ptr += (UINTN)config.seq_len * (UINTN)head_size / 2;
    weights_ptr += (UINTN)config.seq_len * (UINTN)head_size / 2;

    weights.wcls = shared_classifier ? weights.token_embedding_table : weights_ptr;

    // State allocations
    LlmkRunState state;
    state.x = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)config.dim * sizeof(float), 64, L"x");
    state.xb = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)config.dim * sizeof(float), 64, L"xb");
    state.xb2 = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)config.dim * sizeof(float), 64, L"xb2");
    state.hb = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)config.hidden_dim * sizeof(float), 64, L"hb");
    state.hb2 = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)config.hidden_dim * sizeof(float), 64, L"hb2");
    state.q = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)config.dim * sizeof(float), 64, L"q");
    state.k = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)kv_dim * sizeof(float), 64, L"k");
    state.v = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)kv_dim * sizeof(float), 64, L"v");
    state.att = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)config.n_heads * (UINT64)config.seq_len * sizeof(float), 64, L"att");
    state.logits = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_ACTIVATIONS, (UINT64)config.vocab_size * sizeof(float), 64, L"logits");

    UINT64 kv_floats = (UINT64)config.n_layers * (UINT64)config.seq_len * (UINT64)kv_dim;
    state.key_cache = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_KV_CACHE, kv_floats * sizeof(float), 64, L"key cache");
    state.value_cache = (float*)llmk_sentinel_alloc(sentinel, LLMK_ARENA_KV_CACHE, kv_floats * sizeof(float), 64, L"value cache");

    if (sentinel->tripped || !state.x || !state.key_cache || !state.value_cache) return EFI_OUT_OF_RESOURCES;

    // Zero KV cache
    for (UINT64 i = 0; i < kv_floats; i++) {
        state.key_cache[i] = 0.0f;
        state.value_cache[i] = 0.0f;
    }

    // Tokenizer
    LlmkTokenizer tok;
    st = load_tokenizer(Root, sentinel, tokenizer_filename, config.vocab_size, &tok);
    if (EFI_ERROR(st) || sentinel->tripped) return st;

    out_model->cfg = config;
    out_model->w = weights;
    out_model->st = state;
    out_model->tok = tok;

    return EFI_SUCCESS;
}

static void print_piece_ascii(const char *piece) {
    if (!piece) return;
    int len = my_strlen(piece);
    if (len <= 0) return;

    CHAR16 wpiece[256];
    int copy_len = len;
    if (copy_len > 255) copy_len = 255;

    int o = 0;
    for (int i = 0; i < copy_len; i++) {
        unsigned char ch = (unsigned char)piece[i];
        if (ch == '\n' || ch == '\r' || (ch >= 32 && ch < 127)) {
            wpiece[o++] = (CHAR16)ch;
        }
    }
    wpiece[o] = 0;
    if (o > 0) Print(L"%s", wpiece);
}

EFI_STATUS llmk_infer_demo(EFI_HANDLE ImageHandle,
                           EFI_SYSTEM_TABLE *SystemTable,
                           LlmkSentinel *sentinel,
                           EFI_FILE_HANDLE Root) {
    if (!ImageHandle || !SystemTable || !sentinel || !Root) return EFI_INVALID_PARAMETER;

    Print(L"[llmk][infer] loading model+tokenizer...\r\n");

    LlmkModel model;
    EFI_STATUS st = llmk_infer_load(ImageHandle, SystemTable, sentinel, Root, L"stories15M.bin", L"tokenizer.bin", &model);
    if (EFI_ERROR(st) || sentinel->tripped) {
        Print(L"[llmk][infer] load failed: %r\r\n", st);
        return st;
    }

    Print(L"[llmk][infer] model ready: dim=%d layers=%d heads=%d vocab=%d seq=%d\r\n",
          model.cfg.dim, model.cfg.n_layers, model.cfg.n_heads, model.cfg.vocab_size, model.cfg.seq_len);

    // Prompt
    char prompt[] = "Once upon a time";
    int prompt_tokens[256];
    int n_prompt_tokens = 0;
    encode(prompt, prompt_tokens, &n_prompt_tokens, 256, &model.tok);
    if (n_prompt_tokens <= 0) return EFI_LOAD_ERROR;

    // Calibrate budgets using one real forward call (budgets disabled during measure).
    // We overwrite the sentinel config directly (this is the kernel track).
    UINT64 t0 = llmk_rdtsc();
    transformer_forward(&model.st, &model.w, &model.cfg, prompt_tokens[0], 0);
    UINT64 t1 = llmk_rdtsc();
    UINT64 baseline = (t1 >= t0) ? (t1 - t0) : 0;
    if (baseline < 100000ULL) baseline = 100000ULL;

    sentinel->cfg.max_cycles_decode = baseline * 6ULL;
    sentinel->cfg.max_cycles_prefill = baseline * (UINT64)(n_prompt_tokens + 1) * 6ULL;

    if (sentinel->log) {
        llmk_log_event(sentinel->log, LLMK_EVT_INFO, -1, baseline, sentinel->cfg.max_cycles_decode, L"infer budget calibrated");
    }

    Print(L"[llmk][infer] budgets: prefill=%lu decode=%lu (baseline=%lu)\r\n",
          sentinel->cfg.max_cycles_prefill, sentinel->cfg.max_cycles_decode, baseline);

    // Prefill (skip token 0 because calibration already computed it at pos=0)
    llmk_sentinel_phase_start(sentinel, LLMK_PHASE_PREFILL);
    for (int i = 1; i < n_prompt_tokens; i++) {
        if (sentinel->tripped) break;
        transformer_forward(&model.st, &model.w, &model.cfg, prompt_tokens[i], i);
    }
    (void)llmk_sentinel_phase_end(sentinel);

    if (sentinel->tripped) {
        Print(L"[llmk][infer] prefill tripped fail-safe\r\n");
        return EFI_ABORTED;
    }

    int token = prompt_tokens[n_prompt_tokens - 1];
    int pos = n_prompt_tokens - 1;

    Print(L"[llmk][infer] prompt: %a\r\n", prompt);
    Print(L"[llmk][infer] gen: ");

    for (int step = 0; step < 64; step++) {
        // Sample from logits produced by previous forward.
        int next = sample_greedy(model.st.logits, model.cfg.vocab_size);
        if (next == LLMK_TOKEN_EOS || next == LLMK_TOKEN_BOS) break;

        if (sentinel->log) {
            llmk_log_event(sentinel->log, LLMK_EVT_TOKEN, LLMK_PHASE_DECODE, (UINT64)step, (UINT64)next, L"tok");
        }

        if (next >= 0 && next < model.cfg.vocab_size && model.tok.vocab[next]) {
            print_piece_ascii(model.tok.vocab[next]);
        }

        token = next;
        pos++;
        if (pos >= model.cfg.seq_len) break;

        llmk_sentinel_phase_start(sentinel, LLMK_PHASE_DECODE);
        transformer_forward(&model.st, &model.w, &model.cfg, token, pos);
        if (!llmk_sentinel_phase_end(sentinel) || sentinel->tripped) {
            Print(L"\r\n[llmk][infer] stopped at step=%d pos=%d\r\n", step, pos);
            break;
        }
    }

    Print(L"\r\n");
    return EFI_SUCCESS;
}
