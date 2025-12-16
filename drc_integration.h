/*
 * DRC Integration Layer
 * Connects URS reasoning engine with LLaMA2 inference
 */

#ifndef DRC_INTEGRATION_H
#define DRC_INTEGRATION_H

#include "drc/drc.h"

// ═══════════════════════════════════════════════════════════════
// INTEGRATION API
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize DRC system for inference
 * Called once at startup
 */
EFI_STATUS drc_inference_init(void);

/**
 * URS Pre-Inference: Generate reasoning hypotheses BEFORE token generation
 * This runs before the transformer forward pass
 * 
 * @param prompt - The input prompt
 * @param pos - Current position in sequence
 * @return Best reasoning strategy to use
 */
UINT32 drc_urs_before_inference(const CHAR8* prompt, UINT32 pos);

/**
 * Apply URS reasoning to logits before sampling
 * Modifies logits based on selected reasoning path
 * 
 * @param logits - Raw logits from model
 * @param vocab_size - Size of vocabulary
 * @param pos - Current position
 * @param reasoning_mode - Mode selected by URS
 */
void drc_apply_reasoning(float* logits, UINT32 vocab_size, UINT32 pos, UINT32 reasoning_mode);

/**
 * Verify token selection with extended checks
 * Called after token is sampled but before committing
 * 
 * @param token - Sampled token
 * @param logits - Logits used for sampling
 * @param vocab_size - Vocabulary size
 * @return TRUE if token passes verification, FALSE if needs resample
 */
BOOLEAN drc_verify_token(UINT32 token, float* logits, UINT32 vocab_size);

/**
 * Update URS context after token generation
 * Learns from outcomes for adaptive reasoning
 * 
 * @param token - Generated token
 * @param success - Whether generation was successful
 */
void drc_urs_update(UINT32 token, BOOLEAN success);

/**
 * Print DRC status and statistics
 */
void drc_print_status(void);

#endif // DRC_INTEGRATION_H
