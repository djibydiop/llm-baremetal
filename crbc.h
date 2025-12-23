/*
 * CRBC - Cognitive Rollback & Checkpoint System
 * Time-travel debugging and state recovery
 * Made in Senegal 🇸🇳
 */

#ifndef CRBC_H
#define CRBC_H

#include <efi.h>
#include <efilib.h>

// Maximum checkpoints
#define MAX_CHECKPOINTS 32
#define MAX_ROLLBACK_DEPTH 16

// Checkpoint types
typedef enum {
    CHECKPOINT_TYPE_AUTO,       // Automatic periodic checkpoint
    CHECKPOINT_TYPE_MANUAL,     // User-requested checkpoint
    CHECKPOINT_TYPE_PRE_RISK,   // Before risky operation
    CHECKPOINT_TYPE_INFERENCE   // Before each token generation
} CheckpointType;

// Rollback reason
typedef enum {
    ROLLBACK_REASON_ERROR,      // Runtime error detected
    ROLLBACK_REASON_QUALITY,    // Output quality too low
    ROLLBACK_REASON_LOOP,       // Infinite loop detected
    ROLLBACK_REASON_MANUAL      // Manual rollback request
} RollbackReason;

// Memory snapshot (CRBC-specific)
typedef struct {
    void *memory_region;
    UINT64 size;
    UINT8 *snapshot_data;
    UINT64 checksum;
} CRBCMemorySnapshot;

// CPU state snapshot
typedef struct {
    UINT64 rax, rbx, rcx, rdx;
    UINT64 rsi, rdi, rbp, rsp;
    UINT64 r8, r9, r10, r11;
    UINT64 r12, r13, r14, r15;
    UINT64 rip, rflags;
} CPUState;

// Checkpoint descriptor
typedef struct {
    UINT64 checkpoint_id;
    UINT64 timestamp;
    CheckpointType type;
    
    // DRC state
    UINT32 token_history[512];
    UINT32 history_length;
    float last_logits[32000];
    
    // Memory snapshots
    CRBCMemorySnapshot snapshots[8];
    UINT32 snapshot_count;
    
    // CPU state
    CPUState cpu;
    
    // Metadata
    CHAR8 description[128];
    UINT64 inference_step;
    float quality_score;
    
    BOOLEAN valid;
} Checkpoint;

// CRBC context
typedef struct {
    Checkpoint checkpoints[MAX_CHECKPOINTS];
    UINT32 checkpoint_count;
    UINT32 current_checkpoint;
    
    UINT64 total_checkpoints_created;
    UINT64 total_rollbacks;
    UINT64 total_time_saved_us;
    
    // Rollback history (for debugging loops)
    UINT64 rollback_history[MAX_ROLLBACK_DEPTH];
    UINT32 rollback_depth;
    
    BOOLEAN crbc_enabled;
    BOOLEAN auto_checkpoint;
    UINT32 checkpoint_interval;  // Tokens between auto-checkpoints
} CRBCContext;

// === CRBC Functions ===

// Initialize CRBC system
EFI_STATUS crbc_init(CRBCContext *ctx);

// Create checkpoint
EFI_STATUS crbc_checkpoint(CRBCContext *ctx, CheckpointType type, const CHAR8 *description);

// Rollback to checkpoint
EFI_STATUS crbc_rollback(CRBCContext *ctx, UINT64 checkpoint_id, RollbackReason reason);

// Rollback N steps
EFI_STATUS crbc_rollback_n(CRBCContext *ctx, UINT32 n_steps, RollbackReason reason);

// List all checkpoints
void crbc_list_checkpoints(CRBCContext *ctx);

// Delete old checkpoints (keep only last N)
EFI_STATUS crbc_prune(CRBCContext *ctx, UINT32 keep_count);

// Snapshot memory region
EFI_STATUS crbc_snapshot_memory(Checkpoint *cp, void *addr, UINT64 size);

// Restore memory from snapshot
EFI_STATUS crbc_restore_memory(Checkpoint *cp, UINT32 snapshot_id);

// Calculate quality score for current state
float crbc_evaluate_quality(CRBCContext *ctx);

// Detect inference loop (same token repeated)
BOOLEAN crbc_detect_loop(CRBCContext *ctx, UINT32 *tokens, UINT32 len);

// Auto-recovery: rollback and retry with different temperature
EFI_STATUS crbc_auto_recover(CRBCContext *ctx);

// Report statistics
void crbc_report(CRBCContext *ctx);

#endif // CRBC_H
