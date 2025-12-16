/*
 * DRC Semantic Clustering System
 * Groups semantically similar tokens for optimization
 */

#ifndef DRC_SEMCLUSTER_H
#define DRC_SEMCLUSTER_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// CLUSTERING STRUCTURES
// ═══════════════════════════════════════════════════════════════

#define MAX_CLUSTERS 64
#define MAX_TOKENS_PER_CLUSTER 32
#define EMBEDDING_DIM 8  // Simplified embedding for bare-metal

typedef struct {
    UINT32 token_id;
    float embedding[EMBEDDING_DIM];
    float frequency;
    UINT64 last_seen;
} TokenInfo;

typedef struct {
    UINT32 cluster_id;
    float centroid[EMBEDDING_DIM];
    
    TokenInfo tokens[MAX_TOKENS_PER_CLUSTER];
    UINT32 token_count;
    
    float cohesion;  // How tight the cluster is
    UINT32 access_count;
    UINT64 last_accessed;
} SemanticCluster;

typedef struct {
    SemanticCluster clusters[MAX_CLUSTERS];
    UINT32 cluster_count;
    
    // Cache for fast lookup
    UINT32 token_to_cluster[512];  // Maps token_id to cluster_id
    
    // Statistics
    UINT32 total_tokens_clustered;
    UINT32 cache_hits;
    UINT32 cache_misses;
    
    // Configuration
    float similarity_threshold;  // 0.7 default
    UINT32 min_cluster_size;     // 3 minimum
    UINT32 max_cluster_size;     // 32 maximum
    
    BOOLEAN enable_dynamic_clustering;
} SemanticClusterContext;

// ═══════════════════════════════════════════════════════════════
// CLUSTERING FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize semantic clustering
 */
EFI_STATUS semcluster_init(SemanticClusterContext* ctx);

/**
 * Add token to clustering system
 */
void semcluster_add_token(SemanticClusterContext* ctx,
                          UINT32 token_id,
                          const float* embedding);

/**
 * Find cluster for token
 */
UINT32 semcluster_find_cluster(SemanticClusterContext* ctx, UINT32 token_id);

/**
 * Get similar tokens from same cluster
 */
UINT32 semcluster_get_similar(SemanticClusterContext* ctx,
                              UINT32 token_id,
                              UINT32* similar_tokens,
                              UINT32 max_count);

/**
 * Calculate similarity between two embeddings
 */
float semcluster_similarity(const float* emb1, const float* emb2);

/**
 * Merge similar clusters
 */
void semcluster_merge_clusters(SemanticClusterContext* ctx);

/**
 * Get cluster centroid
 */
const float* semcluster_get_centroid(const SemanticClusterContext* ctx, UINT32 cluster_id);

/**
 * Print clustering statistics
 */
void semcluster_print_report(const SemanticClusterContext* ctx);

/**
 * Get cache hit rate
 */
float semcluster_get_hit_rate(const SemanticClusterContext* ctx);

#endif // DRC_SEMCLUSTER_H
