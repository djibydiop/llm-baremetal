/* Inference for Llama-2 Transformer model in UEFI - Direct port from Karpathy's run.c */

#include <efi.h>
#include <efilib.h>

// Math functions
float sqrtf(float x) {
    float result;
    __asm__ volatile ("sqrtss %1, %0" : "=x"(result) : "x"(x));
    return result;
}

float expf(float x) {
    if (x > 88.0f) return 3.4e38f;
    if (x < -88.0f) return 0.0f;
    float result = 1.0f;
    float term = 1.0f;
    for (int i = 1; i < 20; i++) {
        term *= x / i;
        result += term;
        if (term < 1e-7f && term > -1e-7f) break;
    }
    return result;
}

float powf(float base, float exp) {
    if (base == 0.0f) return 0.0f;
    if (exp == 0.0f) return 1.0f;
    float result = 1.0f;
    int exp_int = (int)exp;
    for (int i = 0; i < exp_int; i++) {
        result *= base;
    }
    return result;
}

float cosf(float x) {
    while (x > 3.14159265f) x -= 6.28318531f;
    while (x < -3.14159265f) x += 6.28318531f;
    float x2 = x * x;
    return 1.0f - x2/2.0f + x2*x2/24.0f - x2*x2*x2/720.0f;
}

float sinf(float x) {
    while (x > 3.14159265f) x -= 6.28318531f;
    while (x < -3.14159265f) x += 6.28318531f;
    float x2 = x * x;
    return x - x*x2/6.0f + x*x2*x2/120.0f - x*x2*x2*x2/5040.0f;
}

// Memory functions
void* efi_malloc(UINTN size) {
    void* ptr = NULL;
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, size, &ptr);
    if (EFI_ERROR(status)) return NULL;
    return ptr;
}

void efi_free(void* ptr) {
    if (ptr) uefi_call_wrapper(BS->FreePool, 1, ptr);
}

void* efi_calloc(UINTN nmemb, UINTN size) {
    UINTN total = nmemb * size;
    void* ptr = efi_malloc(total);
    if (ptr) {
        for (UINTN i = 0; i < total; i++) {
            ((UINT8*)ptr)[i] = 0;
        }
    }
    return ptr;
}

void efi_memcpy(void* dest, const void* src, UINTN n) {
    UINT8* d = (UINT8*)dest;
    const UINT8* s = (const UINT8*)src;
    for (UINTN i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

void efi_memset(void* s, int c, UINTN n) {
    UINT8* p = (UINT8*)s;
    for (UINTN i = 0; i < n; i++) {
        p[i] = (UINT8)c;
    }
}

int efi_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

UINTN efi_strlen(const char* s) {
    UINTN len = 0;
    while (s[len]) len++;
    return len;
}

void efi_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Print functions
void print_str(const char* str) {
    CHAR16 buf[2] = {0, 0};
    while (*str) {
        buf[0] = (CHAR16)*str;
        uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, buf);
        str++;
    }
}

void print_num(UINT64 num) {
    char buf[32];
    int i = 0;
    if (num == 0) {
        print_str("0");
        return;
    }
    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    while (i > 0) {
        CHAR16 c[2] = {buf[--i], 0};
        uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, c);
    }
}

// ----------------------------------------------------------------------------
// Transformer model (EXACT copy from Karpathy)

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
    float* rms_ffn_weight;
    float* wq;
    float* wk;
    float* wv;
    float* wo;
    float* w1;
    float* w2;
    float* w3;
    float* rms_final_weight;
    float* wcls;
} TransformerWeights;

typedef struct {
    float *x;
    float *xb;
    float *xb2;
    float *hb;
    float *hb2;
    float *q;
    float *k;
    float *v;
    float *att;
    float *logits;
    float* key_cache;
    float* value_cache;
} RunState;

typedef struct {
    Config config;
    TransformerWeights weights;
    RunState state;
    float* data;
    UINTN file_size;
} Transformer;

void malloc_run_state(RunState* s, Config* p) {
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    s->x = efi_calloc(p->dim, sizeof(float));
    s->xb = efi_calloc(p->dim, sizeof(float));
    s->xb2 = efi_calloc(p->dim, sizeof(float));
    s->hb = efi_calloc(p->hidden_dim, sizeof(float));
    s->hb2 = efi_calloc(p->hidden_dim, sizeof(float));
    s->q = efi_calloc(p->dim, sizeof(float));
    s->key_cache = efi_calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
    s->value_cache = efi_calloc(p->n_layers * p->seq_len * kv_dim, sizeof(float));
    s->att = efi_calloc(p->n_heads * p->seq_len, sizeof(float));
    s->logits = efi_calloc(p->vocab_size, sizeof(float));
    
    if (!s->x || !s->xb || !s->xb2 || !s->hb || !s->hb2 || !s->q
     || !s->key_cache || !s->value_cache || !s->att || !s->logits) {
        print_str("malloc_run_state failed!\r\n");
    }
}

void free_run_state(RunState* s) {
    efi_free(s->x);
    efi_free(s->xb);
    efi_free(s->xb2);
    efi_free(s->hb);
    efi_free(s->hb2);
    efi_free(s->q);
    efi_free(s->att);
    efi_free(s->logits);
    efi_free(s->key_cache);
    efi_free(s->value_cache);
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
    w->rms_ffn_weight = ptr;
    ptr += n_layers * p->dim;
    w->w1 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->w2 = ptr;
    ptr += n_layers * p->hidden_dim * p->dim;
    w->w3 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->rms_final_weight = ptr;
    ptr += p->dim;
    ptr += p->seq_len * head_size / 2;
    ptr += p->seq_len * head_size / 2;
    w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

// Global ImageHandle (set in efi_main)
static EFI_HANDLE g_ImageHandle = NULL;

// UEFI file reading
EFI_STATUS read_checkpoint_efi(CHAR16* checkpoint_name, Config* config, TransformerWeights* weights, float** data, UINTN* file_size) {
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *root = NULL, *file = NULL;
    EFI_LOADED_IMAGE *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    
    status = uefi_call_wrapper(BS->HandleProtocol, 3, g_ImageHandle, &LoadedImageProtocol, (void**)&loaded_image);
    if (EFI_ERROR(status)) return status;
    
    status = uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle, &FileSystemProtocol, (void**)&fs);
    if (EFI_ERROR(status)) return status;
    
    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status)) return status;
    
    status = uefi_call_wrapper(root->Open, 5, root, &file, checkpoint_name, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(root->Close, 1, root);
        return status;
    }
    
    // Read config header
    UINTN config_size = sizeof(Config);
    status = uefi_call_wrapper(file->Read, 3, file, &config_size, config);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);
        return status;
    }
    
    int shared_weights = config->vocab_size > 0 ? 1 : 0;
    config->vocab_size = (config->vocab_size < 0) ? -(config->vocab_size) : config->vocab_size;
    
    // Get file size
    EFI_FILE_INFO *file_info = NULL;
    UINTN info_size = sizeof(EFI_FILE_INFO) + 512;
    file_info = efi_malloc(info_size);
    status = uefi_call_wrapper(file->GetInfo, 4, file, &GenericFileInfo, &info_size, file_info);
    if (EFI_ERROR(status)) {
        efi_free(file_info);
        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);
        return status;
    }
    
    *file_size = file_info->FileSize;
    efi_free(file_info);
    
    // Allocate buffer and read entire file
    *data = efi_malloc(*file_size);
    if (!*data) {
        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Reset to beginning
    UINT64 pos = 0;
    uefi_call_wrapper(file->SetPosition, 2, file, pos);
    
    // Read entire file
    UINTN read_size = *file_size;
    status = uefi_call_wrapper(file->Read, 3, file, &read_size, *data);
    
    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(root->Close, 1, root);
    
    if (EFI_ERROR(status)) {
        efi_free(*data);
        return status;
    }
    
    float* weights_ptr = *data + sizeof(Config)/sizeof(float);
    memory_map_weights(weights, config, weights_ptr, shared_weights);
    
    return EFI_SUCCESS;
}

void build_transformer(Transformer *t, CHAR16* checkpoint_path) {
    EFI_STATUS status = read_checkpoint_efi(checkpoint_path, &t->config, &t->weights, &t->data, &t->file_size);
    if (EFI_ERROR(status)) {
        print_str("Failed to read checkpoint\r\n");
        return;
    }
    malloc_run_state(&t->state, &t->config);
}

void free_transformer(Transformer* t) {
    if (t->data) efi_free(t->data);
    free_run_state(&t->state);
}

// ----------------------------------------------------------------------------
// Neural net blocks (EXACT copy from Karpathy)

void rmsnorm(float* o, float* x, float* weight, int size) {
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void softmax(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
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

float* forward(Transformer* transformer, int token, int pos) {
    Config* p = &transformer->config;
    TransformerWeights* w = &transformer->weights;
    RunState* s = &transformer->state;
    float *x = s->x;
    int dim = p->dim;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads;
    int hidden_dim = p->hidden_dim;
    int head_size = dim / p->n_heads;

    float* content_row = w->token_embedding_table + token * dim;
    efi_memcpy(x, content_row, dim*sizeof(*x));

    for(unsigned long long l = 0; l < p->n_layers; l++) {
        rmsnorm(s->xb, x, w->rms_att_weight + l*dim, dim);

        int loff = l * p->seq_len * kv_dim;
        s->k = s->key_cache + loff + pos * kv_dim;
        s->v = s->value_cache + loff + pos * kv_dim;

        matmul(s->q, s->xb, w->wq + l*dim*dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l*dim*kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l*dim*kv_dim, dim, kv_dim);

        for (int i = 0; i < dim; i+=2) {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            int rotn = i < kv_dim ? 2 : 1;
            for (int v = 0; v < rotn; v++) {
                float* vec = v == 0 ? s->q : s->k;
                float v0 = vec[i];
                float v1 = vec[i+1];
                vec[i]   = v0 * fcr - v1 * fci;
                vec[i+1] = v0 * fci + v1 * fcr;
            }
        }

        for (int h = 0; h < p->n_heads; h++) {
            float* q = s->q + h * head_size;
            float* att = s->att + h * p->seq_len;
            for (int t = 0; t <= pos; t++) {
                float* k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) {
                    score += q[i] * k[i];
                }
                score /= sqrtf(head_size);
                att[t] = score;
            }

            softmax(att, pos + 1);

            float* xb = s->xb + h * head_size;
            efi_memset(xb, 0, head_size * sizeof(float));
            for (int t = 0; t <= pos; t++) {
                float* v = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                float a = att[t];
                for (int i = 0; i < head_size; i++) {
                    xb[i] += a * v[i];
                }
            }
        }

        matmul(s->xb2, s->xb, w->wo + l*dim*dim, dim, dim);

        for (int i = 0; i < dim; i++) {
            x[i] += s->xb2[i];
        }

        rmsnorm(s->xb, x, w->rms_ffn_weight + l*dim, dim);

        matmul(s->hb, s->xb, w->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l*dim*hidden_dim, dim, hidden_dim);

        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            val *= (1.0f / (1.0f + expf(-val)));
            val *= s->hb2[i];
            s->hb[i] = val;
        }

        matmul(s->xb, s->hb, w->w2 + l*dim*hidden_dim, hidden_dim, dim);

        for (int i = 0; i < dim; i++) {
            x[i] += s->xb[i];
        }
    }

    rmsnorm(x, x, w->rms_final_weight, dim);

    matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
    return s->logits;
}

// ----------------------------------------------------------------------------
// Tokenizer (EXACT copy from Karpathy)

typedef struct {
    char *str;
    int id;
} TokenIndex;

typedef struct {
    char** vocab;
    float* vocab_scores;
    TokenIndex *sorted_vocab;
    int vocab_size;
    unsigned int max_token_length;
    unsigned char byte_pieces[512];
} Tokenizer;

int compare_tokens(const void *a, const void *b) {
    const char* str_a = ((TokenIndex*)a)->str;
    const char* str_b = ((TokenIndex*)b)->str;
    return efi_strcmp(str_a, str_b);
}

// Quicksort for tokenizer (efficient O(n log n))
void token_sort_internal(TokenIndex* arr, int low, int high) {
    if (low < high) {
        // Partition
        TokenIndex pivot = arr[high];
        int i = low - 1;
        
        for (int j = low; j < high; j++) {
            if (compare_tokens(&arr[j], &pivot) <= 0) {
                i++;
                TokenIndex temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
        
        TokenIndex temp = arr[i + 1];
        arr[i + 1] = arr[high];
        arr[high] = temp;
        
        int pi = i + 1;
        
        // Recursively sort
        token_sort_internal(arr, low, pi - 1);
        token_sort_internal(arr, pi + 1, high);
    }
}

void token_sort(TokenIndex* arr, int n) {
    if (n > 0) {
        token_sort_internal(arr, 0, n - 1);
    }
}

EFI_STATUS build_tokenizer_efi(Tokenizer* t, CHAR16* tokenizer_path, int vocab_size) {
    t->vocab_size = vocab_size;
    t->vocab = efi_malloc(vocab_size * sizeof(char*));
    t->vocab_scores = efi_malloc(vocab_size * sizeof(float));
    t->sorted_vocab = NULL;
    
    for (int i = 0; i < 256; i++) {
        t->byte_pieces[i * 2] = (unsigned char)i;
        t->byte_pieces[i * 2 + 1] = '\0';
    }
    
    EFI_STATUS status;
    EFI_FILE_PROTOCOL *root = NULL, *file = NULL;
    EFI_LOADED_IMAGE *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    
    status = uefi_call_wrapper(BS->HandleProtocol, 3, g_ImageHandle, &LoadedImageProtocol, (void**)&loaded_image);
    if (EFI_ERROR(status)) return status;
    
    status = uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle, &FileSystemProtocol, (void**)&fs);
    if (EFI_ERROR(status)) return status;
    
    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status)) return status;
    
    status = uefi_call_wrapper(root->Open, 5, root, &file, tokenizer_path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(root->Close, 1, root);
        return status;
    }
    
    UINTN size = sizeof(int);
    uefi_call_wrapper(file->Read, 3, file, &size, &t->max_token_length);
    
    for (int i = 0; i < vocab_size; i++) {
        size = sizeof(float);
        uefi_call_wrapper(file->Read, 3, file, &size, &t->vocab_scores[i]);
        
        int len;
        size = sizeof(int);
        uefi_call_wrapper(file->Read, 3, file, &size, &len);
        
        t->vocab[i] = efi_malloc(len + 1);
        size = len;
        uefi_call_wrapper(file->Read, 3, file, &size, t->vocab[i]);
        t->vocab[i][len] = '\0';
    }
    
    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(root->Close, 1, root);
    
    return EFI_SUCCESS;
}

void free_tokenizer(Tokenizer* t) {
    for (int i = 0; i < t->vocab_size; i++) {
        efi_free(t->vocab[i]);
    }
    efi_free(t->vocab);
    efi_free(t->vocab_scores);
    if (t->sorted_vocab) efi_free(t->sorted_vocab);
}

char* decode(Tokenizer* t, int prev_token, int token) {
    char *piece = t->vocab[token];
    if (prev_token == 1 && piece[0] == ' ') { piece++; }
    unsigned char byte_val;
    if (piece[0] == '<' && piece[1] == '0' && piece[2] == 'x') {
        // Parse hex byte like <0x0A>
        byte_val = 0;
        char c1 = piece[3];
        char c2 = piece[4];
        if (c1 >= '0' && c1 <= '9') byte_val = (c1 - '0') << 4;
        else if (c1 >= 'A' && c1 <= 'F') byte_val = (c1 - 'A' + 10) << 4;
        else if (c1 >= 'a' && c1 <= 'f') byte_val = (c1 - 'a' + 10) << 4;
        if (c2 >= '0' && c2 <= '9') byte_val |= (c2 - '0');
        else if (c2 >= 'A' && c2 <= 'F') byte_val |= (c2 - 'A' + 10);
        else if (c2 >= 'a' && c2 <= 'f') byte_val |= (c2 - 'a' + 10);
        piece = (char*)t->byte_pieces + byte_val * 2;
    }
    return piece;
}

void safe_printf(char *piece) {
    if (piece == NULL) { return; }
    if (piece[0] == '\0') { return; }
    if (piece[1] == '\0') {
        unsigned char byte_val = piece[0];
        if (!(byte_val >= 32 && byte_val < 127) && byte_val != '\n' && byte_val != '\t' && byte_val != '\r') {
            return;
        }
    }
    print_str(piece);
}

int str_lookup(char *str, TokenIndex *sorted_vocab, int vocab_size) {
    // Binary search
    int left = 0;
    int right = vocab_size - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = efi_strcmp(str, sorted_vocab[mid].str);
        if (cmp == 0) return sorted_vocab[mid].id;
        if (cmp < 0) right = mid - 1;
        else left = mid + 1;
    }
    return -1;
}

void encode(Tokenizer* t, char *text, int bos, int eos, int *tokens, int *n_tokens) {
    if (text == NULL) return;
    
    print_str("  [encode] Setting up sorted_vocab...\r\n");
    if (t->sorted_vocab == NULL) {
        t->sorted_vocab = efi_malloc(t->vocab_size * sizeof(TokenIndex));
        if (!t->sorted_vocab) {
            print_str("  [encode] ERROR: Failed to allocate sorted_vocab!\r\n");
            return;
        }
        print_str("  [encode] Copying vocab pointers...\r\n");
        for (int i = 0; i < t->vocab_size; i++) {
            t->sorted_vocab[i].str = t->vocab[i];
            t->sorted_vocab[i].id = i;
        }
        print_str("  [encode] Sorting vocab (this may take a minute)...\r\n");
        token_sort(t->sorted_vocab, t->vocab_size);
        print_str("  [encode] Sorting complete!\r\n");
    }
    
    char* str_buffer = efi_malloc((t->max_token_length*2 +1 +2) * sizeof(char));
    UINTN str_len = 0;
    
    *n_tokens = 0;
    
    if (bos) tokens[(*n_tokens)++] = 1;
    
    if (text[0] != '\0') {
        int dummy_prefix = str_lookup(" ", t->sorted_vocab, t->vocab_size);
        tokens[(*n_tokens)++] = dummy_prefix;
    }
    
    for (char *c = text; *c != '\0'; c++) {
        if ((*c & 0xC0) != 0x80) {
            str_len = 0;
        }
        
        str_buffer[str_len++] = *c;
        str_buffer[str_len] = '\0';
        
        if ((*(c+1) & 0xC0) == 0x80 && str_len < 4) {
            continue;
        }
        
        int id = str_lookup(str_buffer, t->sorted_vocab, t->vocab_size);
        
        if (id != -1) {
            tokens[(*n_tokens)++] = id;
        } else {
            for (UINTN i=0; i < str_len; i++) {
                tokens[(*n_tokens)++] = (unsigned char)str_buffer[i] + 3;
            }
        }
        str_len = 0;
    }
    
    // Merge best pairs
    while (1) {
        float best_score = -1e10;
        int best_id = -1;
        int best_idx = -1;
        
        for (int i=0; i < (*n_tokens-1); i++) {
            // Simple concatenation
            char* s1 = t->vocab[tokens[i]];
            char* s2 = t->vocab[tokens[i+1]];
            UINTN len1 = efi_strlen(s1);
            UINTN len2 = efi_strlen(s2);
            if (len1 + len2 < t->max_token_length*2) {
                efi_memcpy(str_buffer, s1, len1);
                efi_memcpy(str_buffer + len1, s2, len2 + 1);
                
                int id = str_lookup(str_buffer, t->sorted_vocab, t->vocab_size);
                if (id != -1 && t->vocab_scores[id] > best_score) {
                    best_score = t->vocab_scores[id];
                    best_id = id;
                    best_idx = i;
                }
            }
        }
        
        if (best_idx == -1) {
            break;
        }
        
        tokens[best_idx] = best_id;
        for (int i = best_idx+1; i < (*n_tokens-1); i++) {
            tokens[i] = tokens[i+1];
        }
        (*n_tokens)--;
    }
    
    if (eos) tokens[(*n_tokens)++] = 2;
    
    efi_free(str_buffer);
}

// ----------------------------------------------------------------------------
// Sampler (EXACT copy from Karpathy)

typedef struct {
    float prob;
    int index;
} ProbIndex;

typedef struct {
    int vocab_size;
    ProbIndex* probindex;
    float temperature;
    float topp;
    unsigned long long rng_state;
} Sampler;

int sample_argmax(float* probabilities, int n) {
    int max_i = 0;
    float max_p = probabilities[0];
    for (int i = 1; i < n; i++) {
        if (probabilities[i] > max_p) {
            max_i = i;
            max_p = probabilities[i];
        }
    }
    return max_i;
}

int sample_mult(float* probabilities, int n, float coin) {
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf) {
            return i;
        }
    }
    return n - 1;
}

int compare_prob(const void* a, const void* b) {
    ProbIndex* a_ = (ProbIndex*) a;
    ProbIndex* b_ = (ProbIndex*) b;
    if (a_->prob > b_->prob) return -1;
    if (a_->prob < b_->prob) return 1;
    return 0;
}

void prob_sort(ProbIndex* arr, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (compare_prob(&arr[j], &arr[j + 1]) > 0) {
                ProbIndex temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

int sample_topp(float* probabilities, int n, float topp, ProbIndex* probindex, float coin) {
    int n0 = 0;
    const float cutoff = (1.0f - topp) / (n - 1);
    for (int i = 0; i < n; i++) {
        if (probabilities[i] >= cutoff) {
            probindex[n0].index = i;
            probindex[n0].prob = probabilities[i];
            n0++;
        }
    }
    prob_sort(probindex, n0);
    
    float cumulative_prob = 0.0f;
    int last_idx = n0 - 1;
    for (int i = 0; i < n0; i++) {
        cumulative_prob += probindex[i].prob;
        if (cumulative_prob > topp) {
            last_idx = i;
            break;
        }
    }
    
    float r = coin * cumulative_prob;
    float cdf = 0.0f;
    for (int i = 0; i <= last_idx; i++) {
        cdf += probindex[i].prob;
        if (r < cdf) {
            return probindex[i].index;
        }
    }
    return probindex[last_idx].index;
}

void build_sampler(Sampler* sampler, int vocab_size, float temperature, float topp, unsigned long long rng_seed) {
    sampler->vocab_size = vocab_size;
    sampler->temperature = temperature;
    sampler->topp = topp;
    sampler->rng_state = rng_seed;
    sampler->probindex = efi_malloc(sampler->vocab_size * sizeof(ProbIndex));
}

void free_sampler(Sampler* sampler) {
    efi_free(sampler->probindex);
}

unsigned int random_u32(unsigned long long *state) {
    *state ^= *state >> 12;
    *state ^= *state << 25;
    *state ^= *state >> 27;
    return (*state * 0x2545F4914F6CDD1Dull) >> 32;
}

float random_f32(unsigned long long *state) {
    return (random_u32(state) >> 8) / 16777216.0f;
}

int sample(Sampler* sampler, float* logits) {
    int next;
    if (sampler->temperature == 0.0f) {
        next = sample_argmax(logits, sampler->vocab_size);
    } else {
        for (int q=0; q<sampler->vocab_size; q++) {
            logits[q] /= sampler->temperature;
        }
        softmax(logits, sampler->vocab_size);
        float coin = random_f32(&sampler->rng_state);
        if (sampler->topp <= 0 || sampler->topp >= 1) {
            next = sample_mult(logits, sampler->vocab_size, coin);
        } else {
            next = sample_topp(logits, sampler->vocab_size, sampler->topp, sampler->probindex, coin);
        }
    }
    return next;
}

// ----------------------------------------------------------------------------
// Generation (simplified for UEFI)

void generate(Transformer *transformer, Tokenizer *tokenizer, Sampler *sampler, char *prompt, int steps) {
    char *empty_prompt = "";
    if (prompt == NULL) { prompt = empty_prompt; }
    
    print_str("Encoding prompt: \"");
    print_str(prompt);
    print_str("\"\r\n");
    
    print_str("Allocating token buffer...\r\n");
    int num_prompt_tokens = 0;
    int* prompt_tokens = efi_malloc((efi_strlen(prompt)+3) * sizeof(int));
    if (!prompt_tokens) {
        print_str("ERROR: Failed to allocate token buffer!\r\n");
        return;
    }
    
    print_str("Calling encode()...\r\n");
    encode(tokenizer, prompt, 1, 0, prompt_tokens, &num_prompt_tokens);
    print_str("Encode() returned\r\n");
    
    print_str("Prompt tokens: ");
    print_num(num_prompt_tokens);
    print_str("\r\n");
    
    if (num_prompt_tokens < 1) {
        print_str("ERROR: No prompt tokens!\r\n");
        efi_free(prompt_tokens);
        return;
    }
    
    print_str("\r\nGenerating text ("); print_num(steps); print_str(" steps):\r\n[");
    
    int token = prompt_tokens[0];
    int pos = 0;
    int next;
    
    while (pos < steps) {
        // Debug every 10 tokens
        if (pos % 10 == 0) {
            print_str(".");
        }
        
        float* logits = forward(transformer, token, pos);
        
        if (pos < num_prompt_tokens - 1) {
            next = prompt_tokens[pos + 1];
        } else {
            next = sample(sampler, logits);
        }
        pos++;
        
        if (next == 1) { 
            print_str("]\r\nBOS token - stopping\r\n");
            break; 
        }
        
        char* piece = decode(tokenizer, token, next);
        safe_printf(piece);
        token = next;
    }
    
    print_str("]\r\n\r\nGeneration complete! (");
    print_num(pos);
    print_str(" tokens)\r\n");
    efi_free(prompt_tokens);
}

// ----------------------------------------------------------------------------
// UEFI Entry Point

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    g_ImageHandle = ImageHandle;  // Store for file operations
    
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    
    print_str("=== LLaMA2 UEFI - v5.0 (110M) - FIXED BUILD ===\r\n");
    print_str("Based on Karpathy's run.c\r\n\r\n");
    
    print_str("Press any key to start loading model...\r\n");
    EFI_INPUT_KEY key_start;
    while (uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key_start) != EFI_SUCCESS);

    Transformer transformer;
    print_str("Loading model stories110M.bin...\r\n");
    build_transformer(&transformer, L"stories110M.bin");
    
    print_str("Model config:\r\n");
    print_str("  dim="); print_num(transformer.config.dim); print_str("\r\n");
    print_str("  n_layers="); print_num(transformer.config.n_layers); print_str("\r\n");
    print_str("  vocab_size="); print_num(transformer.config.vocab_size); print_str("\r\n\r\n");
    
    Tokenizer tokenizer;
    print_str("Loading tokenizer...\r\n");
    build_tokenizer_efi(&tokenizer, L"tokenizer.bin", transformer.config.vocab_size);
    
    Sampler sampler;
    print_str("Building sampler (temp=1.0)...\r\n");
    unsigned long long seed = 12345;
    build_sampler(&sampler, transformer.config.vocab_size, 1.0f, 0.9f, seed);
    
    char* prompt = "Once upon a time";
    generate(&transformer, &tokenizer, &sampler, prompt, 80);
    
    free_sampler(&sampler);
    free_tokenizer(&tokenizer);
    free_transformer(&transformer);
    
    print_str("\r\nPress any key to exit...\r\n");
    EFI_INPUT_KEY key;
    while (uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key) != EFI_SUCCESS);
    
    return EFI_SUCCESS;
}
