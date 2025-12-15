/*
 * DRC Radio-Cognitive Protocol (Phase 7 CWEB)
 * Communication layer for distributed cognitive existence
 * 
 * This is NOT a REST API - it's an existence protocol
 */

#ifndef DRC_RADIOCOG_H
#define DRC_RADIOCOG_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// RADIO-COGNITIVE PROTOCOL (Phase 7 CWEB)
// ═══════════════════════════════════════════════════════════════

#define MAX_MESSAGES        32
#define MAX_FRAGMENTS       16
#define MAX_TRUST_LEVELS    5

// Message types (not HTTP, cognitive messages)
typedef enum {
    MSG_EXISTENCE_QUERY,    // "Should I exist?"
    MSG_EXISTENCE_GRANT,    // "You may exist"
    MSG_EXISTENCE_DENY,     // "Do not exist"
    MSG_FRAGMENT_REQUEST,   // "Send me boot fragment X"
    MSG_FRAGMENT_DELIVERY,  // "Here is fragment X"
    MSG_TRUST_HANDSHAKE,    // Progressive trust establishment
    MSG_CONTEXT_VALIDATE,   // Validate current context
    MSG_COHERENCE_CHECK,    // Check global coherence
    MSG_CONSENSUS_VOTE,     // Distributed consensus
    MSG_EMERGENCY_HALT      // Emergency stop signal
} MessageType;

// Trust levels (CWEB progressive trust)
typedef enum {
    TRUST_NONE,             // No trust established
    TRUST_IDENTITY,         // Hardware identity verified
    TRUST_CRYPTO,           // Cryptographic handshake complete
    TRUST_BEHAVIORAL,       // Behavior validated
    TRUST_FULL              // Full trust granted
} TrustLevel;

// Boot fragment (for distributed boot)
typedef struct {
    UINT32 fragment_id;
    UINT32 total_fragments;
    CHAR8 fragment_name[64];
    UINT8 data[4096];
    UINT32 data_size;
    BOOLEAN signature_valid;
    TrustLevel required_trust;
} BootFragment;

// Cognitive message
typedef struct {
    MessageType type;
    UINT64 timestamp;
    CHAR8 sender_id[64];
    CHAR8 payload[256];
    TrustLevel sender_trust;
    BOOLEAN requires_response;
    float confidence;
} CognitiveMessage;

// Consensus state (for distributed decisions)
typedef struct {
    CHAR8 decision[128];
    UINT32 votes_for;
    UINT32 votes_against;
    UINT32 total_nodes;
    float consensus_threshold;
    BOOLEAN consensus_reached;
} ConsensusState;

// Radio-cognitive context
typedef struct {
    // Identity
    CHAR8 node_id[64];
    TrustLevel current_trust;
    BOOLEAN existence_granted;
    
    // Messages
    CognitiveMessage messages[MAX_MESSAGES];
    UINT32 message_count;
    
    // Fragments (for distributed boot)
    BootFragment fragments[MAX_FRAGMENTS];
    UINT32 fragment_count;
    UINT32 fragments_received;
    
    // Consensus
    ConsensusState consensus;
    BOOLEAN consensus_enabled;
    
    // Statistics
    UINT32 queries_sent;
    UINT32 fragments_requested;
    UINT32 trust_handshakes;
    UINT32 existence_grants;
    UINT32 existence_denies;
    
    // Configuration
    BOOLEAN opportunistic_mode;  // Adapt to network quality
    float network_quality;       // 0.0-1.0
    
} RadioCognitiveContext;

// ═══════════════════════════════════════════════════════════════
// FUNCTION PROTOTYPES
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize radio-cognitive protocol
 */
EFI_STATUS radiocog_init(RadioCognitiveContext* ctx, const CHAR8* node_id);

/**
 * Query existence (CWEB foundation)
 */
BOOLEAN radiocog_query_existence(RadioCognitiveContext* ctx);

/**
 * Progressive trust handshake
 */
TrustLevel radiocog_establish_trust(RadioCognitiveContext* ctx);

/**
 * Request boot fragment
 */
EFI_STATUS radiocog_request_fragment(RadioCognitiveContext* ctx,
                                     UINT32 fragment_id);

/**
 * Validate context remotely
 */
BOOLEAN radiocog_validate_context(RadioCognitiveContext* ctx,
                                   const CHAR8* context);

/**
 * Send cognitive message
 */
EFI_STATUS radiocog_send_message(RadioCognitiveContext* ctx,
                                 MessageType type,
                                 const CHAR8* payload);

/**
 * Receive cognitive message
 */
CognitiveMessage* radiocog_receive_message(RadioCognitiveContext* ctx);

/**
 * Check coherence with network
 */
float radiocog_check_coherence(RadioCognitiveContext* ctx);

/**
 * Distributed consensus voting
 */
BOOLEAN radiocog_vote_consensus(RadioCognitiveContext* ctx,
                                const CHAR8* decision,
                                BOOLEAN vote_for);

/**
 * Get consensus result
 */
BOOLEAN radiocog_get_consensus(const RadioCognitiveContext* ctx);

/**
 * Opportunistic adaptation (CWEB)
 */
void radiocog_adapt_to_network(RadioCognitiveContext* ctx);

/**
 * Emergency halt broadcast
 */
EFI_STATUS radiocog_emergency_halt(RadioCognitiveContext* ctx,
                                   const CHAR8* reason);

/**
 * Print protocol report
 */
void radiocog_print_report(const RadioCognitiveContext* ctx);

/**
 * Get network quality estimate
 */
float radiocog_get_network_quality(const RadioCognitiveContext* ctx);

#endif // DRC_RADIOCOG_H
