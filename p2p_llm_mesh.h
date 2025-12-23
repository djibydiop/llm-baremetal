/*
 * P2P LLM Mesh Network
 * Distributed inference across bare-metal nodes
 * Made in Senegal 🇸🇳
 */

#ifndef P2P_LLM_MESH_H
#define P2P_LLM_MESH_H

#include <efi.h>
#include <efilib.h>

// Maximum mesh nodes
#define MAX_MESH_NODES 16
#define MESH_PORT 8888
#define HEARTBEAT_INTERVAL 5000000  // 5 seconds in microseconds

// Node types
typedef enum {
    NODE_TYPE_COORDINATOR,   // Coordinates inference distribution
    NODE_TYPE_WORKER,        // Executes layer computations
    NODE_TYPE_CACHE,         // Stores KV cache
    NODE_TYPE_AGGREGATOR     // Combines results
} NodeType;

// Node state
typedef enum {
    NODE_STATE_OFFLINE,
    NODE_STATE_DISCOVERING,
    NODE_STATE_READY,
    NODE_STATE_BUSY,
    NODE_STATE_ERROR
} NodeState;

// Mesh node descriptor
typedef struct {
    UINT8 mac_address[6];
    UINT32 ip_address;
    NodeType type;
    NodeState state;
    UINT64 last_heartbeat;
    UINT32 compute_capacity;  // Tokens/sec
    UINT32 memory_mb;
    BOOLEAN is_available;
} MeshNode;

// Inference task distribution
typedef struct {
    UINT32 layer_start;
    UINT32 layer_end;
    UINT32 assigned_node_id;
    UINT64 start_time;
    UINT64 end_time;
    BOOLEAN completed;
} InferenceTask;

// P2P Mesh context
typedef struct {
    MeshNode nodes[MAX_MESH_NODES];
    UINT32 node_count;
    UINT32 self_node_id;
    NodeType self_type;
    
    InferenceTask active_tasks[32];
    UINT32 task_count;
    
    UINT64 total_tokens_distributed;
    UINT64 total_time_saved_us;
    
    BOOLEAN mesh_active;
} P2PMeshContext;

// === P2P Mesh Functions ===

// Initialize P2P mesh
EFI_STATUS p2p_mesh_init(P2PMeshContext *ctx, NodeType self_type);

// Discover nearby nodes (WiFi broadcast)
EFI_STATUS p2p_mesh_discover(P2PMeshContext *ctx);

// Register this node with mesh
EFI_STATUS p2p_mesh_announce(P2PMeshContext *ctx);

// Send heartbeat to all nodes
EFI_STATUS p2p_mesh_heartbeat(P2PMeshContext *ctx);

// Distribute inference task to mesh
EFI_STATUS p2p_mesh_distribute_inference(P2PMeshContext *ctx, UINT32 layer_start, UINT32 layer_end, float *input, float *output);

// Execute task as worker node
EFI_STATUS p2p_mesh_execute_task(P2PMeshContext *ctx, InferenceTask *task);

// Aggregate results from workers
EFI_STATUS p2p_mesh_aggregate(P2PMeshContext *ctx, float *final_output);

// Cleanup mesh
void p2p_mesh_cleanup(P2PMeshContext *ctx);

#endif // P2P_LLM_MESH_H
