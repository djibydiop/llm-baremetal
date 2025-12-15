/*
 * DRC Semantic Clustering Implementation
 */

#include "drc_semcluster.h"

// Math helpers (OPTIMIZED: Loop unrolling for SIMD)
static float vec_dot(const float* a, const float* b, UINT32 dim) {
    float sum = 0.0f;
    UINT32 i = 0;
    
    // Unroll by 4 for better SIMD utilization
    for (; i + 3 < dim; i += 4) {
        sum += a[i] * b[i];
        sum += a[i+1] * b[i+1];
        sum += a[i+2] * b[i+2];
        sum += a[i+3] * b[i+3];
    }
    
    // Handle remainder
    for (; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

static float vec_norm(const float* v, UINT32 dim) {
    float sum = 0.0f;
    UINT32 i = 0;
    
    // Unroll by 4
    for (; i + 3 < dim; i += 4) {
        sum += v[i] * v[i];
        sum += v[i+1] * v[i+1];
        sum += v[i+2] * v[i+2];
        sum += v[i+3] * v[i+3];
    }
    for (; i < dim; i++) {
        sum += v[i] * v[i];
    }
    
    // Fast sqrt approximation (reduced iterations)
    float x = sum;
    float y = 1.0f;
    for (int j = 0; j < 6; j++) {  // Reduced from 10 to 6 iterations
        y = (y + x / y) * 0.5f;
    }
    return y;
}

/**
 * Initialize semantic clustering
 */
EFI_STATUS semcluster_init(SemanticClusterContext* ctx) {
    if (!ctx) return EFI_INVALID_PARAMETER;
    
    // Clear all
    for (UINT32 i = 0; i < sizeof(SemanticClusterContext); i++) {
        ((UINT8*)ctx)[i] = 0;
    }
    
    ctx->similarity_threshold = 0.7f;
    ctx->min_cluster_size = 3;
    ctx->max_cluster_size = MAX_TOKENS_PER_CLUSTER;
    ctx->enable_dynamic_clustering = TRUE;
    
    // Initialize cache to invalid
    for (UINT32 i = 0; i < 512; i++) {
        ctx->token_to_cluster[i] = 0xFFFFFFFF;
    }
    
    return EFI_SUCCESS;
}

/**
 * Add token to clustering system
 */
void semcluster_add_token(SemanticClusterContext* ctx,
                          UINT32 token_id,
                          const float* embedding) {
    if (!ctx || !embedding || token_id >= 512) return;
    
    // Find best matching cluster
    UINT32 best_cluster = 0xFFFFFFFF;
    float best_similarity = 0.0f;
    
    for (UINT32 i = 0; i < ctx->cluster_count; i++) {
        SemanticCluster* cluster = &ctx->clusters[i];
        
        // Calculate similarity to centroid
        float sim = semcluster_similarity(embedding, cluster->centroid);
        
        if (sim > best_similarity && sim >= ctx->similarity_threshold) {
            best_similarity = sim;
            best_cluster = i;
        }
    }
    
    // Add to existing cluster or create new
    if (best_cluster != 0xFFFFFFFF && 
        ctx->clusters[best_cluster].token_count < ctx->max_cluster_size) {
        
        SemanticCluster* cluster = &ctx->clusters[best_cluster];
        TokenInfo* token = &cluster->tokens[cluster->token_count++];
        
        token->token_id = token_id;
        for (UINT32 i = 0; i < EMBEDDING_DIM; i++) {
            token->embedding[i] = embedding[i];
        }
        token->frequency = 1.0f;
        token->last_seen = ctx->total_tokens_clustered;
        
        // Update centroid (OPTIMIZED: cached reciprocal, avoid division in loop)
        float inv_count = 1.0f / cluster->token_count;
        float weight_old = (cluster->token_count - 1) * inv_count;
        for (UINT32 i = 0; i < EMBEDDING_DIM; i++) {
            cluster->centroid[i] = cluster->centroid[i] * weight_old + embedding[i] * inv_count;
        }
        
        ctx->token_to_cluster[token_id] = best_cluster;
        
    } else if (ctx->cluster_count < MAX_CLUSTERS) {
        // Create new cluster
        SemanticCluster* cluster = &ctx->clusters[ctx->cluster_count];
        cluster->cluster_id = ctx->cluster_count;
        cluster->token_count = 1;
        
        // Initialize centroid with this token's embedding
        for (UINT32 i = 0; i < EMBEDDING_DIM; i++) {
            cluster->centroid[i] = embedding[i];
        }
        
        TokenInfo* token = &cluster->tokens[0];
        token->token_id = token_id;
        for (UINT32 i = 0; i < EMBEDDING_DIM; i++) {
            token->embedding[i] = embedding[i];
        }
        token->frequency = 1.0f;
        
        ctx->token_to_cluster[token_id] = ctx->cluster_count;
        ctx->cluster_count++;
    }
    
    ctx->total_tokens_clustered++;
}

/**
 * Find cluster for token
 */
UINT32 semcluster_find_cluster(SemanticClusterContext* ctx, UINT32 token_id) {
    if (!ctx || token_id >= 512) return 0xFFFFFFFF;
    
    UINT32 cluster_id = ctx->token_to_cluster[token_id];
    
    if (cluster_id != 0xFFFFFFFF) {
        ctx->cache_hits++;
    } else {
        ctx->cache_misses++;
    }
    
    return cluster_id;
}

/**
 * Get similar tokens from same cluster
 */
UINT32 semcluster_get_similar(SemanticClusterContext* ctx,
                              UINT32 token_id,
                              UINT32* similar_tokens,
                              UINT32 max_count) {
    if (!ctx || !similar_tokens) return 0;
    
    UINT32 cluster_id = semcluster_find_cluster(ctx, token_id);
    if (cluster_id == 0xFFFFFFFF || cluster_id >= ctx->cluster_count) return 0;
    
    SemanticCluster* cluster = &ctx->clusters[cluster_id];
    cluster->access_count++;
    
    UINT32 count = 0;
    for (UINT32 i = 0; i < cluster->token_count && count < max_count; i++) {
        if (cluster->tokens[i].token_id != token_id) {
            similar_tokens[count++] = cluster->tokens[i].token_id;
        }
    }
    
    return count;
}

/**
 * Calculate similarity between two embeddings (cosine similarity)
 */
float semcluster_similarity(const float* emb1, const float* emb2) {
    if (!emb1 || !emb2) return 0.0f;
    
    float dot = vec_dot(emb1, emb2, EMBEDDING_DIM);
    float norm1 = vec_norm(emb1, EMBEDDING_DIM);
    float norm2 = vec_norm(emb2, EMBEDDING_DIM);
    
    if (norm1 < 0.0001f || norm2 < 0.0001f) return 0.0f;
    
    return dot / (norm1 * norm2);
}

/**
 * Merge similar clusters
 */
void semcluster_merge_clusters(SemanticClusterContext* ctx) {
    if (!ctx || !ctx->enable_dynamic_clustering) return;
    
    for (UINT32 i = 0; i < ctx->cluster_count; i++) {
        for (UINT32 j = i + 1; j < ctx->cluster_count; j++) {
            float sim = semcluster_similarity(ctx->clusters[i].centroid, 
                                             ctx->clusters[j].centroid);
            
            if (sim >= ctx->similarity_threshold) {
                // Merge cluster j into cluster i
                SemanticCluster* dst = &ctx->clusters[i];
                SemanticCluster* src = &ctx->clusters[j];
                
                UINT32 space = ctx->max_cluster_size - dst->token_count;
                UINT32 to_copy = src->token_count < space ? src->token_count : space;
                
                for (UINT32 k = 0; k < to_copy; k++) {
                    dst->tokens[dst->token_count++] = src->tokens[k];
                    ctx->token_to_cluster[src->tokens[k].token_id] = i;
                }
                
                // Remove cluster j (shift remaining)
                for (UINT32 k = j; k < ctx->cluster_count - 1; k++) {
                    ctx->clusters[k] = ctx->clusters[k + 1];
                }
                ctx->cluster_count--;
                j--;  // Check this position again
            }
        }
    }
}

/**
 * Get cluster centroid
 */
const float* semcluster_get_centroid(const SemanticClusterContext* ctx, UINT32 cluster_id) {
    if (!ctx || cluster_id >= ctx->cluster_count) return NULL;
    return ctx->clusters[cluster_id].centroid;
}

/**
 * Print clustering statistics
 */
void semcluster_print_report(const SemanticClusterContext* ctx) {
    if (!ctx) return;
    
    Print(L"\r\n═══════════════════════════════════════════════════════════\r\n");
    Print(L"  SEMANTIC CLUSTERING REPORT\r\n");
    Print(L"═══════════════════════════════════════════════════════════\r\n");
    
    Print(L"  Total Clusters:     %d / %d\r\n", ctx->cluster_count, MAX_CLUSTERS);
    Print(L"  Tokens Clustered:   %d\r\n", ctx->total_tokens_clustered);
    Print(L"  Cache Hits:         %d\r\n", ctx->cache_hits);
    Print(L"  Cache Misses:       %d\r\n", ctx->cache_misses);
    Print(L"  Hit Rate:           %.1f%%\r\n", semcluster_get_hit_rate(ctx) * 100.0f);
    Print(L"\r\n");
    
    Print(L"  Top Clusters:\r\n");
    for (UINT32 i = 0; i < ctx->cluster_count && i < 5; i++) {
        const SemanticCluster* c = &ctx->clusters[i];
        Print(L"    [%d] %d tokens, accessed %d times\r\n", 
              c->cluster_id, c->token_count, c->access_count);
    }
    
    Print(L"═══════════════════════════════════════════════════════════\r\n");
}

/**
 * Get cache hit rate
 */
float semcluster_get_hit_rate(const SemanticClusterContext* ctx) {
    if (!ctx) return 0.0f;
    
    UINT32 total = ctx->cache_hits + ctx->cache_misses;
    if (total == 0) return 0.0f;
    
    return (float)ctx->cache_hits / (float)total;
}
