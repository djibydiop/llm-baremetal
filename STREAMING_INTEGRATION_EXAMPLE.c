/*
 * Network Streaming Integration Example
 * 
 * This shows how to integrate HTTP streaming into llama2_efi.c
 * to load large models (stories110M, TinyLlama) that exceed UEFI memory limits
 */

// Add to llama2_efi.c includes:
// #include "network_boot.h" (already there)

// Add these global variables near the top:
static HttpStreamSession g_stream_session;
static BOOLEAN g_streaming_mode = FALSE;

// Add this function to load weights on-demand:
float* load_weight_chunk_network(UINT64 offset, UINT64 size) {
    static UINT8 weight_buffer[STREAM_CHUNK_SIZE];
    
    if (!g_streaming_mode) {
        // Fallback to disk/memory
        return NULL;
    }
    
    VOID* chunk_data = NULL;
    UINT64 bytes_read = 0;
    
    EFI_STATUS status = http_stream_get_chunk(
        gImageHandle,
        ST,
        &g_stream_session,
        offset,
        size,
        &chunk_data,
        &bytes_read
    );
    
    if (EFI_ERROR(status)) {
        Print(L"[Network] Failed to load chunk at offset %d\r\n", offset);
        return NULL;
    }
    
    Print(L"[Network] Loaded %d KB from offset %d KB\r\n", 
          bytes_read / 1024, offset / 1024);
    
    return (float*)chunk_data;
}

// Modify transformer forward pass to use streaming:
void transformer_forward_streaming(
    int token, 
    int pos,
    Config* p,
    RunState* s,
    TransformerWeights* w
) {
    // Calculate which layer we need
    int layer = pos / p->seq_len;  // Simplified - actual logic depends on architecture
    
    // Calculate byte offset for this layer's weights
    UINT64 layer_offset = calculate_layer_offset(layer, p);
    UINT64 layer_size = calculate_layer_size(layer, p);
    
    // Stream this layer's weights
    float* layer_weights = load_weight_chunk_network(layer_offset, layer_size);
    
    if (!layer_weights) {
        Print(L"[ERROR] Failed to stream layer %d weights\r\n", layer);
        return;
    }
    
    // Use streamed weights for forward pass
    // ... rest of forward pass logic using layer_weights ...
    
    // Weights are automatically freed when next chunk is loaded (reuses buffer)
}

// Modify main boot function:
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    // ... existing initialization ...
    
    // Check if network is available
    BOOLEAN network_available = check_network_available(SystemTable);
    
    if (network_available) {
        Print(L"\r\n[NETWORK] TCP/IP stack available\r\n");
        Print(L"[NETWORK] Large model streaming enabled!\r\n\r\n");
        
        // Ask user which mode
        Print(L"Select mode:\r\n");
        Print(L"  1. Disk mode (stories15M.bin - 60MB)\r\n");
        Print(L"  2. Network streaming (stories110M.bin - 418MB)\r\n");
        Print(L"  3. Network streaming (TinyLlama 1.1B - 1.1GB)\r\n");
        Print(L"\r\nChoice (1-3): ");
        
        // For now, default to option 2 for demo
        int choice = 2;
        
        if (choice == 2 || choice == 3) {
            g_streaming_mode = TRUE;
            
            const CHAR8* model_url = (choice == 2) 
                ? "http://10.0.2.2:8080/stories110M.bin"
                : "http://10.0.2.2:8080/tinyllama-1.1b-chat.bin";
            
            Print(L"\r\n[NETWORK] Initializing streaming from: %a\r\n", model_url);
            
            EFI_STATUS status = http_stream_init(
                ImageHandle,
                SystemTable,
                model_url,
                &g_stream_session
            );
            
            if (EFI_ERROR(status)) {
                Print(L"[ERROR] Failed to initialize streaming: %r\r\n", status);
                Print(L"[FALLBACK] Switching to disk mode...\r\n");
                g_streaming_mode = FALSE;
            } else {
                Print(L"[NETWORK] Streaming ready!\r\n");
                Print(L"[NETWORK] Model will be loaded chunk-by-chunk (4MB each)\r\n");
                Print(L"[NETWORK] No memory limits! Can stream 100GB+ models!\r\n\r\n");
            }
        }
    }
    
    // Rest of boot process...
    // If streaming mode, use transformer_forward_streaming() instead of normal forward pass
    
    // Cleanup at the end
    if (g_streaming_mode) {
        http_stream_cleanup(&g_stream_session);
    }
    
    return EFI_SUCCESS;
}

// Helper functions to calculate offsets (example for llama2.c format):
UINT64 calculate_layer_offset(int layer, Config* p) {
    // llama2.c format: config (7*4 bytes) + all weights sequentially
    UINT64 offset = 28;  // Skip config
    
    // Each layer has: wq, wk, wv, wo, w1, w2, w3, rms_att, rms_ffn
    // Sizes depend on dimensions
    UINT64 layer_size = 0;
    
    // Token embedding (only in layer 0)
    if (layer == 0) {
        offset += p->vocab_size * p->dim * sizeof(float);
    }
    
    // For each previous layer
    for (int i = 0; i < layer; i++) {
        layer_size = 0;
        layer_size += p->dim * p->dim * sizeof(float);  // wq
        layer_size += p->dim * p->dim * sizeof(float);  // wk
        layer_size += p->dim * p->dim * sizeof(float);  // wv
        layer_size += p->dim * p->dim * sizeof(float);  // wo
        layer_size += p->dim * p->hidden_dim * sizeof(float);  // w1
        layer_size += p->hidden_dim * p->dim * sizeof(float);  // w2
        layer_size += p->dim * p->hidden_dim * sizeof(float);  // w3
        layer_size += p->dim * sizeof(float);  // rms_att_weight
        layer_size += p->dim * sizeof(float);  // rms_ffn_weight
        offset += layer_size;
    }
    
    return offset;
}

UINT64 calculate_layer_size(int layer, Config* p) {
    UINT64 size = 0;
    
    size += p->dim * p->dim * sizeof(float);  // wq
    size += p->dim * p->dim * sizeof(float);  // wk
    size += p->dim * p->dim * sizeof(float);  // wv
    size += p->dim * p->dim * sizeof(float);  // wo
    size += p->dim * p->hidden_dim * sizeof(float);  // w1
    size += p->hidden_dim * p->dim * sizeof(float);  // w2
    size += p->dim * p->hidden_dim * sizeof(float);  // w3
    size += p->dim * sizeof(float);  // rms_att_weight
    size += p->dim * sizeof(float);  // rms_ffn_weight
    
    return size;
}
