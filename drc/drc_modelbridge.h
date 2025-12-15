/*
 * DRC ModelBridge - GGUF ↔ BIN Streaming Conversion
 * Zero-copy, chunk-based model loading
 */

#ifndef DRC_MODELBRIDGE_H
#define DRC_MODELBRIDGE_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// MODELBRIDGE STRUCTURES - UNIVERSAL FORMAT SUPPORT
// ═══════════════════════════════════════════════════════════════

#define CHUNK_SIZE (4 * 1024 * 1024)  // 4MB chunks
#define MAX_TENSORS 512
#define MAX_TENSOR_NAME 128

// Model format detection
typedef enum {
    MODEL_FORMAT_UNKNOWN = 0,
    MODEL_FORMAT_GGUF,        // llama.cpp GGUF format
    MODEL_FORMAT_BIN,         // llama2.c raw binary format
    MODEL_FORMAT_SAFETENSORS, // HuggingFace SafeTensors
    MODEL_FORMAT_PYTORCH      // PyTorch checkpoint
} ModelFormat;

// GGUF data types
typedef enum {
    GGUF_TYPE_F32 = 0,
    GGUF_TYPE_F16 = 1,
    GGUF_TYPE_Q4_0 = 2,
    GGUF_TYPE_Q4_1 = 3,
    GGUF_TYPE_Q5_0 = 6,
    GGUF_TYPE_Q5_1 = 7,
    GGUF_TYPE_Q8_0 = 8
} GGUFDataType;

// Tensor info from GGUF
typedef struct {
    CHAR8 name[MAX_TENSOR_NAME];
    UINT32 n_dims;
    UINT64 dimensions[4];
    GGUFDataType type;
    UINT64 offset;  // Offset in file
    UINT64 size;    // Size in bytes
} TensorInfo;

typedef struct {
    UINT8 chunk_buffer[CHUNK_SIZE];
    UINT32 chunk_offset;
    UINT32 chunk_size;
    BOOLEAN has_data;
} ChunkBuffer;

typedef struct {
    // Universal file handle
    EFI_FILE_PROTOCOL* model_file;
    ModelFormat format;  // Auto-detected format
    
    // Transient buffer (no persistent conversion)
    ChunkBuffer current_chunk;
    ChunkBuffer next_chunk;
    
    // Model metadata (extracted from any format)
    UINT32 n_layers;
    UINT32 n_heads;
    UINT32 n_embd;
    UINT32 n_vocab;
    
    // Format-specific offsets
    UINT64 weights_offset;  // Where weights start in file
    
    // Tensor map (parsed from GGUF)
    TensorInfo tensors[MAX_TENSORS];
    UINT32 tensor_count;
    UINT64 tensor_data_offset;  // Start of tensor data in file
    
    // Streaming state
    UINT64 current_offset;
    UINT64 total_size;
    BOOLEAN eof;
    
    // Performance
    UINT32 chunks_loaded;
    UINT32 cache_hits;
    UINT32 cache_misses;
} ModelBridge;

// ═══════════════════════════════════════════════════════════════
// MODELBRIDGE FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize ModelBridge from any supported model file
 * Auto-detects format (GGUF, .bin, SafeTensors, etc.)
 * Reads header and metadata, prepares for streaming
 */
EFI_STATUS modelbridge_init(ModelBridge* bridge, const CHAR16* model_path);

/**
 * Detect model format from file header
 */
ModelFormat modelbridge_detect_format(const UINT8* header, UINTN size);

/**
 * Load a specific weight chunk on-demand
 * Returns pointer to weight data in transient buffer
 */
EFI_STATUS modelbridge_load_chunk(ModelBridge* bridge, UINT64 offset, void** data);

/**
 * Get weight by layer and type
 * Automatically handles chunked loading
 */
EFI_STATUS modelbridge_get_weight(ModelBridge* bridge, 
                                   UINT32 layer, 
                                   const CHAR8* weight_name,
                                   void** data,
                                   UINT64* size);

/**
 * Pre-fetch next chunk in background (if possible)
 */
EFI_STATUS modelbridge_prefetch(ModelBridge* bridge, UINT64 offset);

/**
 * Get model metadata
 */
EFI_STATUS modelbridge_get_metadata(ModelBridge* bridge,
                                     UINT32* n_layers,
                                     UINT32* n_heads,
                                     UINT32* n_embd,
                                     UINT32* n_vocab);

/**
 * Print statistics
 */
void modelbridge_print_stats(ModelBridge* bridge);

/**
 * Cleanup and close file
 */
EFI_STATUS modelbridge_cleanup(ModelBridge* bridge);

/**
 * Find tensor by name in tensor map
 */
TensorInfo* modelbridge_find_tensor(ModelBridge* bridge, const CHAR8* name);

/**
 * Dequantize Q4_0 data to F32
 */
void modelbridge_dequantize_q4_0(const void* src, float* dst, UINT64 count);

#endif // DRC_MODELBRIDGE_H
