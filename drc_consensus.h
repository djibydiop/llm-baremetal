/*
 * DRC Network Consensus
 * Made in Senegal 🇸🇳
 */

#ifndef DRC_CONSENSUS_H
#define DRC_CONSENSUS_H

#include <efi.h>
#include <efilib.h>

#define MAX_VALIDATORS 3
#define CONSENSUS_THRESHOLD 2

typedef struct {
    UINT32 failed_boots;
    const CHAR8* model_name;
    const CHAR8* drc_version;
} SystemState;

typedef struct {
    BOOLEAN approved;
    UINT32 approvals;
    UINT32 rejections;
    CHAR8 reason[256];
} ConsensusResult;

EFI_STATUS drc_request_consensus(SystemState* state, ConsensusResult* result);

#endif
