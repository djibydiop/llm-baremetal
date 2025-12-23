/*
 * Model Streaming 2.0 - Parallel chunk download with cache
 */

#ifndef MODEL_STREAMING_H
#define MODEL_STREAMING_H

#include <efi.h>
#include <efilib.h>

#define MAX_PARALLEL_CHUNKS 4
#define CHUNK_SIZE (4 * 1024 * 1024)  // 4 MB
#define CACHE_SIZE (32 * 1024 * 1024)  // 32 MB

// Chunk download state
typedef struct {
    UINT32 chunk_id;
    UINT64 offset;
    UINT64 size;
    UINT8 *buffer;
    BOOLEAN complete;
    BOOLEAN in_progress;
} ChunkDownload;

// Streaming context
typedef struct {
    const CHAR8 *url;
    UINT64 total_size;
    UINT64 downloaded;
    ChunkDownload chunks[MAX_PARALLEL_CHUNKS];
    UINT8 *cache;
    UINT32 cache_used;
    UINT32 active_chunks;
} StreamingContext;

// API
EFI_STATUS streaming_init(StreamingContext *ctx, const CHAR8 *url, UINT64 file_size);
EFI_STATUS streaming_download_parallel(StreamingContext *ctx);
EFI_STATUS streaming_get_data(StreamingContext *ctx, UINT64 offset, UINT64 size, UINT8 **data);
void streaming_free(StreamingContext *ctx);

#endif // MODEL_STREAMING_H
