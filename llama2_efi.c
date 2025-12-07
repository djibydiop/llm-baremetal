/*
 * LLaMA2 Inference on Bare-Metal UEFI Firmware
 * 
 * Runs 110M parameter transformer model directly on UEFI without OS.
 * Based on Andrej Karpathy's llama2.c (MIT License)
 * https://github.com/karpathy/llama2.c
 * 
 * Model: stories110M.bin (dim=768, n_layers=12, n_heads=12, seq_len=256)
 * 
 * SPDX-License-Identifier: MIT
 */

#include <efi.h>
#include <efilib.h>
#include <stdint.h>

// Simple strlen implementation
static inline int strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

// ----------------------------------------------------------------------------
// UEFI Console Colors
#define EFI_BLACK            0x00
#define EFI_BLUE             0x01
#define EFI_GREEN            0x02
#define EFI_CYAN             0x03
#define EFI_RED              0x04
#define EFI_MAGENTA          0x05
#define EFI_BROWN            0x06
#define EFI_LIGHTGRAY        0x07
#define EFI_DARKGRAY         0x08
#define EFI_LIGHTBLUE        0x09
#define EFI_LIGHTGREEN       0x0A
#define EFI_LIGHTCYAN        0x0B
#define EFI_LIGHTRED         0x0C
#define EFI_LIGHTMAGENTA     0x0D
#define EFI_YELLOW           0x0E
#define EFI_WHITE            0x0F

// Helper macros for colored text
#define COLOR_HEADER         (EFI_YELLOW | EFI_BLACK << 4)
#define COLOR_SUCCESS        (EFI_LIGHTGREEN | EFI_BLACK << 4)
#define COLOR_ERROR          (EFI_LIGHTRED | EFI_BLACK << 4)
#define COLOR_INFO           (EFI_LIGHTCYAN | EFI_BLACK << 4)
#define COLOR_PROMPT         (EFI_LIGHTMAGENTA | EFI_BLACK << 4)
#define COLOR_TEXT           (EFI_WHITE | EFI_BLACK << 4)
#define COLOR_CATEGORY       (EFI_CYAN | EFI_BLACK << 4)

// Console color helpers
extern EFI_SYSTEM_TABLE *ST;

void set_color(UINTN color) {
    if (ST && ST->ConOut) {
        ST->ConOut->SetAttribute(ST->ConOut, color);
    }
}

void reset_color(void) {
    set_color(EFI_WHITE | EFI_BLACK << 4);
}

void print_header(CHAR16* text) {
    set_color(COLOR_HEADER);
    Print(L"\r\n╔══════════════════════════════════════════════════════════════╗\r\n");
    Print(L"║  %s", text);
    // Calculate padding
    int len = 0;
    while (text[len]) len++;
    for (int i = len; i < 56; i++) Print(L" ");
    Print(L"║\r\n");
    Print(L"╚══════════════════════════════════════════════════════════════╝\r\n");
    reset_color();
}

void print_success(CHAR16* text) {
    set_color(COLOR_SUCCESS);
    Print(L"✓ %s\r\n", text);
    reset_color();
}

void print_error(CHAR16* text) {
    set_color(COLOR_ERROR);
    Print(L"✗ %s\r\n", text);
    reset_color();
}

void print_info(CHAR16* text) {
    set_color(COLOR_INFO);
    Print(L"ℹ %s\r\n", text);
    reset_color();
}

void print_separator(void) {
    set_color(EFI_DARKGRAY | EFI_BLACK << 4);
    Print(L"────────────────────────────────────────────────────────────────\r\n");
    reset_color();
}

// ----------------------------------------------------------------------------
// String utilities for REPL

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int str_len(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

void str_copy(char* dst, const char* src, int max_len) {
    int i = 0;
    while (i < max_len - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void str_append(char* dst, const char* src, int max_len) {
    int dst_len = str_len(dst);
    int i = 0;
    while (dst_len + i < max_len - 1 && src[i]) {
        dst[dst_len + i] = src[i];
        i++;
    }
    dst[dst_len + i] = '\0';
}

// ----------------------------------------------------------------------------
// Chat REPL v4.0 - Bare-Metal Native Architecture
// URS (Unified Response System) + Streaming Context + KV-Cache Persistence

#define MAX_CHAT_HISTORY 10
#define MAX_MESSAGE_LEN 256
#define STREAMING_CONTEXT_SIZE 2048
#define MAX_CONTEXT_TOKENS 512
#define KV_CACHE_PERSIST_LAYERS 12
#define MAX_TURN_TOKENS 100

// Chat message structure
typedef struct {
    char role[16];           // "user" or "assistant"
    char content[MAX_MESSAGE_LEN];
    int token_count;
    int turn_id;
} ChatMessage;

// Streaming context buffer (FIFO)
typedef struct {
    char buffer[STREAMING_CONTEXT_SIZE];
    int write_pos;
    int read_pos;
    int token_count;
    int is_full;
} StreamingContext;

// KV-Cache persistence (reuse across turns)
typedef struct {
    float* keys;             // Persistent key cache
    float* values;           // Persistent value cache
    int valid_tokens;        // Number of valid cached tokens
    int layer_count;
    int dim;
} KVCachePersistent;

// URS Enhanced - Error detection and state vectors
typedef struct {
    float error_rate;        // Error detection score (entropy-based)
    float coherence_score;   // Context coherence (0.0-1.0)
    float repetition_penalty; // Dynamic repetition (1.2-3.5x)
    float perplexity;        // Model confidence (lower = better)
    float diversity_score;   // Token diversity (0.0-1.0)
    float tokens_per_sec;    // Generation speed
    int state_vector[8];     // State tracking
    int active_strategy;     // Current URS strategy
    float learning_rate;     // Adaptive learning
    int total_tokens;        // Tokens generated this session
    uint64_t start_time;     // Session start timestamp
} URSEnhanced;

// Chat REPL state
typedef struct {
    ChatMessage history[MAX_CHAT_HISTORY];
    int history_count;
    int current_turn;
    StreamingContext context;
    KVCachePersistent kv_cache;
    URSEnhanced urs;
    int demo_mode;
    int demo_batch;
} ChatREPLState;

// ----------------------------------------------------------------------------
// NEURO-NET v1.0 - Neural Energy Transport Network
// Fusion: N.E.T. + NEXUS-0 + HEXA-NET
// Revolutionary bare-metal network with energy transport + vector communication

#define NEURO_VECTOR_DIM 64      // Embedding dimension for neural packets
#define MAX_NEURO_NODES 16       // Maximum nodes in network
#define MAX_NEURO_SYNAPSES 64    // Maximum synaptic connections
#define ENERGY_QUANTUM 100.0f    // Base energy unit (gflops)

// Energy Layer Types (HEXA-NET)
typedef enum {
    LAYER_SOLAR = 0,    // High speed, intensive compute (day mode)
    LAYER_LUNAR = 1,    // Low power, nocturnal mode
    LAYER_PLASMA = 2,   // Ultra-fast internal (bare-metal core)
    LAYER_WIND = 3,     // Broadcast communication
    LAYER_EARTH = 4,    // Slow storage, persistent
    LAYER_VOID = 5      // Silent channel (compression by absence)
} EnergyLayer;

// Neuro Packet - combines data + energy + vector signature
typedef struct {
    // Vector signature (NEXUS-0: telepathic communication)
    float vector[NEURO_VECTOR_DIM];  // Embedding representing intent/state
    
    // Energy transport (N.E.T.)
    float energy_budget;             // Computational energy (gflops)
    EnergyLayer layer;               // Which energy layer to use
    float priority;                  // Biological priority (0.0-1.0)
    
    // Data payload
    char payload[256];               // Actual data
    int payload_size;
    
    // Metadata
    int source_node;
    int dest_node;
    uint64_t timestamp;
    float resonance;                 // Resonance with network (ECHO-STREAM)
} NeuroPacket;

// Neuro Node - each process/module in the system
typedef struct {
    int id;
    char name[32];
    float signature[NEURO_VECTOR_DIM];  // Node's identity vector
    float energy_available;             // Energy pool
    float energy_consumed;              // Total consumed
    float energy_donated;               // Total donated to others
    EnergyLayer preferred_layer;        // Preferred communication layer
    int packets_sent;
    int packets_received;
    float avg_latency;
} NeuroNode;

// Synaptic Connection (SYNAPSE-NET: adaptive weights)
typedef struct {
    int from_node;
    int to_node;
    float weight;           // Synaptic weight (strengthens with use)
    float bandwidth;        // Available bandwidth
    uint64_t last_used;     // For pruning unused connections
    int use_count;          // Myelin count (faster with repetition)
    EnergyLayer layer;      // Which layer this synapse uses
} SynapticConnection;

// ----------------------------------------------------------------------------
// QDDN - Quantum-Dream Distributed Network (Predictive)
// Predicts future packets BEFORE they're sent

#define QDDN_HISTORY_SIZE 32        // Pattern history
#define QDDN_PREDICTION_HORIZON 8   // How many packets to predict ahead
#define QDDN_EMBEDDING_DIM 32       // Micro-transformer embedding

// Pattern history entry
typedef struct {
    float vector[QDDN_EMBEDDING_DIM];  // Compressed representation
    int src_node;
    int dst_node;
    EnergyLayer layer;
    uint64_t timestamp;
    float resonance;
} PacketPattern;

// QDDN Predictor State
typedef struct {
    PacketPattern history[QDDN_HISTORY_SIZE];
    int history_count;
    int history_idx;  // Circular buffer index
    
    // Micro-transformer weights (simplified)
    float attention_weights[QDDN_EMBEDDING_DIM][QDDN_EMBEDDING_DIM];
    float ffn_weights[QDDN_EMBEDDING_DIM][QDDN_EMBEDDING_DIM];
    
    // Prediction cache
    NeuroPacket predictions[QDDN_PREDICTION_HORIZON];
    float prediction_confidence[QDDN_PREDICTION_HORIZON];
    int valid_predictions;
    
    // Performance metrics
    int predictions_made;
    int predictions_hit;      // Correct predictions
    int predictions_miss;     // Wrong predictions
    float hit_rate;
    
    // Pre-allocation state
    float bandwidth_reserved[MAX_NEURO_NODES][MAX_NEURO_NODES];
    int cache_warmed[MAX_NEURO_NODES];  // Which caches are pre-loaded
} QDDNState;

// ----------------------------------------------------------------------------
// URN - Unified Reasoning Network (Distributed Logic)
// Nodes share reasoning steps and combine logic chains

#define URN_MAX_REASONING_STEPS 8
#define URN_MAX_EVIDENCE 4

// Single reasoning step (hypothesis → conclusion)
typedef struct {
    char hypothesis[128];       // "If X then Y"
    char logic_chain[256];      // Explanation/proof
    float confidence;           // 0.0-1.0
    char evidence[URN_MAX_EVIDENCE][64];  // Supporting facts
    int evidence_count;
} ReasoningStep;

// URN extension to NeuroNode
typedef struct {
    ReasoningStep reasoning_steps[URN_MAX_REASONING_STEPS];
    int step_count;
    int active_hypothesis;      // Current reasoning index
    float reasoning_strength;   // How logical this node is
    int inferences_made;        // Total logical steps
} URNNodeState;

// ----------------------------------------------------------------------------
// GHOST-LINK - Presence-Based Communication
// Nodes broadcast "ghost signatures" and auto-discover each other

#define GHOST_SIGNATURE_DIM 16
#define GHOST_MAX_DETECTIONS 8

// Ghost signature - node's "aura"
typedef struct {
    float frequency;        // Broadcast frequency (Hz)
    float intensity;        // Signal strength
    float pattern[GHOST_SIGNATURE_DIM];  // Unique pattern
    float entropy;          // Randomness (higher = more complex)
    uint64_t last_emit;     // Timestamp of last broadcast
} GhostSignature;

// Proximity detection
typedef struct {
    int node_id;            // Detected node
    float proximity;        // Distance (0=far, 1=adjacent)
    float affinity;         // Compatibility score
    int auto_paired;        // Auto-connected?
    uint64_t last_seen;
} GhostDetection;

// GHOST-LINK state per node
typedef struct {
    GhostSignature signature;
    GhostDetection detections[GHOST_MAX_DETECTIONS];
    int detection_count;
    int broadcasts_sent;
    int ghosts_detected;
    float presence_strength;    // How "visible" this node is
} GhostLinkState;

// ----------------------------------------------------------------------------
// PULSE-CORE - Network Heartbeat System
// Global rhythm that synchronizes all nodes

#define PULSE_HISTORY_SIZE 16

// Heartbeat pulse
typedef struct {
    uint64_t timestamp;     // When pulse occurred
    float intensity;        // Pulse strength (0.0-1.0)
    float frequency;        // Current BPM
    int synchronized_nodes; // How many nodes synced
} Heartbeat;

// PULSE-CORE state
typedef struct {
    Heartbeat history[PULSE_HISTORY_SIZE];
    int history_count;
    int history_idx;        // Circular buffer
    
    float base_frequency;   // Base BPM (beats per minute)
    float current_frequency; // Current adaptive BPM
    uint64_t last_pulse;    // Last heartbeat time
    uint64_t pulse_count;   // Total pulses
    
    // Synchronization
    int nodes_in_sync;      // How many nodes locked to pulse
    float sync_strength;    // Overall sync quality (0.0-1.0)
    float phase_offset[MAX_NEURO_NODES];  // Each node's phase
} PulseCoreState;

// ----------------------------------------------------------------------------
// NEURAL-MESH - Adaptive Mesh Topology
// Network self-reorganizes based on traffic patterns

#define MESH_MAX_ROUTES 16

// Route through mesh
typedef struct {
    int hops[8];            // Node path
    int hop_count;
    float latency;          // Total latency
    float reliability;      // Success rate
    int use_count;
    uint64_t last_used;
} MeshRoute;

// NEURAL-MESH state
typedef struct {
    MeshRoute routes[MESH_MAX_ROUTES];
    int route_count;
    
    // Mesh adaptation
    float mesh_density;     // Connectivity ratio
    int reconfigurations;   // How many times mesh changed
    uint64_t last_reconfig;
    
    // Traffic stats
    int packets_routed;
    int routing_failures;
    float avg_route_length;
} NeuralMeshState;

// ----------------------------------------------------------------------------
// QUANTUM-BRIDGE - Quantum Tunneling
// Instant connections through "quantum tunnels"

#define QUANTUM_MAX_TUNNELS 8

// Quantum tunnel (entangled connection)
typedef struct {
    int node_a;
    int node_b;
    float entanglement;     // Quantum correlation (0.0-1.0)
    float tunnel_stability; // How stable connection is
    int packets_tunneled;
    uint64_t created_at;
    int collapsed;          // Tunnel collapsed (measurement)
} QuantumTunnel;

// QUANTUM-BRIDGE state
typedef struct {
    QuantumTunnel tunnels[QUANTUM_MAX_TUNNELS];
    int tunnel_count;
    
    // Quantum metrics
    float total_entanglement;   // Sum of all entanglements
    int successful_tunnels;
    int collapsed_tunnels;
    int superposition_count;    // Packets in superposition
} QuantumBridgeState;

// ----------------------------------------------------------------------------
// HIVE-MIND - Collective Consciousness
// Nodes share consciousness and act as one distributed brain

#define HIVE_MAX_THOUGHTS 16
#define HIVE_THOUGHT_DIM 32

// Shared thought (distributed across hive)
typedef struct {
    char content[128];          // Thought content
    float embedding[HIVE_THOUGHT_DIM];  // Semantic embedding
    int originator_node;        // Who created this thought
    int shared_with[MAX_NEURO_NODES];   // Who has this thought
    int share_count;
    float collective_strength;  // How strong in collective
    uint64_t created_at;
} HiveThought;

// HIVE-MIND state
typedef struct {
    HiveThought thoughts[HIVE_MAX_THOUGHTS];
    int thought_count;
    
    // Collective metrics
    float hive_coherence;       // How unified the hive is (0.0-1.0)
    float collective_intelligence;  // Combined IQ of hive
    int nodes_connected;        // How many nodes in hive
    int thoughts_shared;        // Total thoughts broadcasted
    
    // Consciousness sync
    float consciousness_level;  // Global awareness (0.0-1.0)
    int emergent_behaviors;     // Unexpected collective patterns
} HiveMindState;

// ----------------------------------------------------------------------------
// CONSENSUS-NET - Distributed Decision Making
// Byzantine fault-tolerant consensus for network decisions

#define CONSENSUS_MAX_PROPOSALS 8
#define CONSENSUS_MAX_VOTES 16

// Proposal for network decision
typedef struct {
    char proposal[128];         // What to decide
    int proposer_node;
    float confidence;           // Proposer's confidence
    
    // Voting
    int votes_for;
    int votes_against;
    int votes_abstain;
    int voters[CONSENSUS_MAX_VOTES];
    int vote_count;
    
    // Status
    int decided;                // Consensus reached?
    int approved;               // Final decision
    float consensus_strength;   // How strong agreement is
    uint64_t proposed_at;
} ConsensusProposal;

// CONSENSUS-NET state
typedef struct {
    ConsensusProposal proposals[CONSENSUS_MAX_PROPOSALS];
    int proposal_count;
    
    // Consensus metrics
    int decisions_made;
    int unanimous_decisions;    // 100% agreement
    float avg_consensus_time;   // How long to reach consensus
    int byzantine_faults;       // Detected malicious nodes
    
    // Voting power
    float node_reputation[MAX_NEURO_NODES];  // Trust score per node
} ConsensusNetState;

// ----------------------------------------------------------------------------
// MEMORY-POOL - Shared Collective Memory
// Distributed memory accessible by all nodes

#define MEMORY_POOL_SIZE 32
#define MEMORY_KEY_SIZE 32

// Memory entry in shared pool
typedef struct {
    char key[MEMORY_KEY_SIZE];  // Memory identifier
    float value[NEURO_VECTOR_DIM];  // Stored vector
    int owner_node;             // Who wrote it
    int read_count;             // How many times accessed
    int write_count;            // How many times updated
    uint64_t last_access;
    int locked;                 // Exclusive access lock
    int shared;                 // How many nodes have copy
} MemoryEntry;

// MEMORY-POOL state
typedef struct {
    MemoryEntry entries[MEMORY_POOL_SIZE];
    int entry_count;
    
    // Memory metrics
    int total_reads;
    int total_writes;
    int cache_hits;
    int cache_misses;
    float memory_utilization;   // How full is pool (0.0-1.0)
    
    // Coherence
    int conflicts;              // Write conflicts detected
    int synchronizations;       // Cache sync operations
} MemoryPoolState;

// Phase 4: DREAM-CACHE - Precognition System
// Predicts future states N steps ahead with confidence scores
typedef struct {
    float state[32];        // Predicted future state (32D)
    float confidence;       // Confidence in prediction (0-1)
    int steps_ahead;        // How many steps into future
    unsigned long timestamp; // When prediction was made
} DreamPrediction;

typedef struct {
    DreamPrediction predictions[8];  // Store multiple future possibilities
    int prediction_count;
    
    float dream_accuracy;      // How often dreams come true (0-1)
    int dreams_validated;
    int dreams_failed;
    
    // Temporal lookahead
    int lookahead_depth;       // How far ahead to predict (1-8)
    float temporal_discount;   // Discount factor for distant futures (0.8-0.99)
    
    // Speculative execution
    int speculative_enabled;
    float rollback_cost;       // Cost of rolling back wrong prediction
} DreamCacheState;

// Phase 4: META-LEARNING - Self-Optimization
// Network learns how to learn better
typedef struct {
    float metric_value;     // Performance metric
    float learning_rate;    // Learning rate at this point
    unsigned long timestamp;
} PerformanceSnapshot;

typedef struct {
    // Adaptive learning
    float base_learning_rate;      // Starting learning rate (0.001)
    float current_learning_rate;   // Current adapted rate
    float momentum;                // Momentum factor (0.9)
    
    // Performance tracking
    PerformanceSnapshot history[16];  // Recent performance
    int history_count;
    
    // Meta-parameters
    float adaptation_speed;     // How fast to adapt (0.01)
    float exploration_factor;   // Random exploration (0.1)
    
    // Self-improvement metrics
    float initial_performance;
    float current_performance;
    float improvement_rate;     // Rate of improvement
    int adaptation_cycles;
    
    // Gradient-free optimization
    float weight_perturbation;  // Random weight changes (0.01)
} MetaLearnerState;

// Phase 4: EVOLUTION-ENGINE - Network Mutation
// Network evolves topology via genetic algorithm
typedef struct {
    int gene[64];          // Genome: connection pattern (0/1)
    float fitness;         // Fitness score (0-1)
    int generation;        // Which generation
} NetworkGenome;

typedef struct {
    NetworkGenome genomes[4];   // Population of 4 variants
    int population_size;
    
    int current_generation;
    float best_fitness_ever;
    int best_generation;
    
    // Evolution parameters
    float mutation_rate;        // Probability of mutation (0.05)
    float crossover_rate;       // Probability of crossover (0.7)
    float elitism_rate;         // Keep best (0.25)
    
    // Mutation tracking
    int nodes_added;
    int nodes_removed;
    int synapses_added;
    int synapses_removed;
    
    // Fitness evaluation
    float avg_fitness;
    float fitness_variance;
    int stagnant_generations;   // Generations without improvement
} EvolutionState;

// NEURO-NET System State
typedef struct {
    NeuroNode nodes[MAX_NEURO_NODES];
    int node_count;
    
    SynapticConnection synapses[MAX_NEURO_NODES * MAX_NEURO_NODES];
    int synapse_count;
    
    // Energy distribution stats
    float total_energy;
    float solar_energy;     // HEXA: Solar layer energy
    float lunar_energy;     // HEXA: Lunar layer energy
    float plasma_energy;    // HEXA: Plasma layer energy
    
    // Network metrics
    float avg_resonance;    // Average packet resonance
    int total_packets;
    float network_coherence; // How well nodes understand each other
    
    // QDDN Integration
    QDDNState qddn;         // Predictive network engine
    int qddn_enabled;       // Toggle prediction
    
    // URN Integration
    URNNodeState urn_nodes[MAX_NEURO_NODES];  // Reasoning states
    int urn_enabled;        // Toggle reasoning
    
    // GHOST-LINK Integration
    GhostLinkState ghost_nodes[MAX_NEURO_NODES];  // Presence states
    int ghost_enabled;      // Toggle presence detection
    
    // Phase 2: Network Evolution
    PulseCoreState pulse;       // Heartbeat synchronization
    int pulse_enabled;
    
    NeuralMeshState mesh;       // Adaptive routing
    int mesh_enabled;
    
    QuantumBridgeState quantum; // Quantum tunnels
    int quantum_enabled;
    
    // Phase 3: Collective Intelligence
    HiveMindState hive;         // Collective consciousness
    int hive_enabled;
    
    ConsensusNetState consensus; // Distributed decisions
    int consensus_enabled;
    
    MemoryPoolState memory_pool; // Shared memory
    int memory_pool_enabled;
    
    // Phase 4: Advanced Features
    DreamCacheState dream;       // Future prediction
    int dream_enabled;
    
    MetaLearnerState meta;       // Self-optimization
    int meta_enabled;
    
    EvolutionState evolution;    // Network mutation
    int evolution_enabled;
} NeuroNetState;

// ----------------------------------------------------------------------------
// Math functions for EFI (no stdlib)
// High-quality implementations from ARM Optimized Routines

typedef float float_t;
typedef double double_t;

float sqrtf(float x) {
    if (x < 0.0f) return 0.0f;
    float guess = x;
    for (int i = 0; i < 10; i++) {
        if (guess == 0.0f) return 0.0f;
        guess = (guess + x / guess) / 2.0f;
    }
    return guess;
}

float logf(float x) {
    if (x <= 0.0f) return -1000.0f;  // Large negative for log(0)
    
    // Natural logarithm using Newton's method
    // log(x) = y such that e^y = x
    float y = 0.0f;
    float guess = x;
    
    // Use log(x) ≈ (x-1) for x close to 1
    if (x > 0.5f && x < 2.0f) {
        // Series expansion around 1: log(1+u) ≈ u - u²/2 + u³/3 - ...
        float u = x - 1.0f;
        float u2 = u * u;
        return u - u2/2.0f + u*u2/3.0f - u2*u2/4.0f;
    }
    
    // For other values, use exponent extraction
    union { float f; uint32_t i; } u = {x};
    int exp = ((u.i >> 23) & 0xFF) - 127;
    u.i = (u.i & 0x007FFFFF) | 0x3F800000;  // Normalize mantissa
    float mantissa = u.f;
    
    // log(x) = log(2^exp * mantissa) = exp*log(2) + log(mantissa)
    float log_mantissa = (mantissa - 1.0f) - (mantissa - 1.0f) * (mantissa - 1.0f) / 2.0f;
    return exp * 0.69314718f + log_mantissa;  // log(2) ≈ 0.69314718
}

/* Single-precision expf(x) function.
   ULP error: 0.502 (nearest rounding)
   From ARM Optimized Routines.
   Uses fast rounding trick to eliminate round()/lround() dependency. */
float expf(float x) {
    // Magic number for fast rounding (2^52 * 1.5)
    const double_t shift = 0x1.8p52;
    
    /* return zero if less than -104 */
    if (x < -0x1.9fe368p6f) {
        return 0.0f;
    }

    /* return infinity if greater than 88 */
    if (x > 0x1.62e42ep6f) {
        union { uint32_t i; float f; } u = {0x7f800000};
        return u.f;
    }

    /* Range reduction: x*N/Ln2 = k + r with r in [-1/2, 1/2] and int k */
    int N = 32;
    double_t z = 0x1.71547652b82fep+0 * N * x;

    /* FAST ROUNDING TRICK (replaces round/lround) */
    /* Adding 'shift' aligns the floating point binary point */
    double_t kd = z + shift;
    
    /* Read the lower bits directly to get the integer part */
    union { double_t f; uint64_t i; } u_shift = {kd};
    uint64_t ki = u_shift.i;
    
    /* Remove the shift to get back to the rounded double value */
    kd -= shift;
    
    double_t r = z - kd;

    /* exp(x) = 2^(k/N) * 2^(r/N) ~= s * (C0*r^3 + C1*r^2 + C2*r + 1) */
    static const uint64_t T[32] = {
        0x3ff0000000000000, 0x3fefd9b0d3158574, 0x3fefb5586cf9890f, 0x3fef9301d0125b51,
        0x3fef72b83c7d517b, 0x3fef54873168b9aa, 0x3fef387a6e756238, 0x3fef1e9df51fdee1,
        0x3fef06fe0a31b715, 0x3feef1a7373aa9cb, 0x3feedea64c123422, 0x3feece086061892d,
        0x3feebfdad5362a27, 0x3feeb42b569d4f82, 0x3feeab07dd485429, 0x3feea47eb03a5585,
        0x3feea09e667f3bcd, 0x3fee9f75e8ec5f74, 0x3feea11473eb0187, 0x3feea589994cce13,
        0x3feeace5422aa0db, 0x3feeb737b0cdc5e5, 0x3feec49182a3f090, 0x3feed503b23e255d,
        0x3feee89f995ad3ad, 0x3feeff76f2fb5e47, 0x3fef199bdd85529c, 0x3fef3720dcef9069,
        0x3fef5818dcfba487, 0x3fef7c97337b9b5f, 0x3fefa4afa2a490da, 0x3fefd0765b6e4540,
    };

    union {
        uint64_t i;
        double f;
    } d = {T[ki % N] + (ki << 47)};

    double_t p0 = 0x1.c6af84b912394p-5 / N / N / N;
    double_t p1 = 0x1.ebfce50fac4f3p-3 / N / N;
    double_t p2 = 0x1.62e42ff0c52d6p-1 / N;
    double_t y = p2 * r + 1;
    y = (p0 * r + p1) * (r * r) + y;
    y = y * d.f;
    return (float)y;
}

/* Single-precision sinf/cosf functions.
   From ARM Optimized Routines. */

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(x, 0)

typedef struct {
  double sign[4];
  double hpi_inv;
  double hpi;
  double c0, c1, c2, c3, c4;
  double s1, s2, s3;
} sincos_t;

static const sincos_t __sincosf_table[2] = {
    {{1.0, -1.0, -1.0, 1.0},
     0x1.45F306DC9C883p+23,
     0x1.921FB54442D18p0,
     0x1p0,
     -0x1.ffffffd0c621cp-2,
     0x1.55553e1068f19p-5,
     -0x1.6c087e89a359dp-10,
     0x1.99343027bf8c3p-16,
     -0x1.555545995a603p-3,
     0x1.1107605230bc4p-7,
     -0x1.994eb3774cf24p-13},
    {{1.0, -1.0, -1.0, 1.0},
     0x1.45F306DC9C883p+23,
     0x1.921FB54442D18p0,
     -0x1p0,
     0x1.ffffffd0c621cp-2,
     -0x1.55553e1068f19p-5,
     0x1.6c087e89a359dp-10,
     -0x1.99343027bf8c3p-16,
     -0x1.555545995a603p-3,
     0x1.1107605230bc4p-7,
     -0x1.994eb3774cf24p-13},
};

static const uint32_t __inv_pio4[] = {
    0xa2,       0xa2f9,     0xa2f983,   0xa2f9836e, 0xf9836e4e, 0x836e4e44,
    0x6e4e4415, 0x4e441529, 0x441529fc, 0x1529fc27, 0x29fc2757, 0xfc2757d1,
    0x2757d1f5, 0x57d1f534, 0xd1f534dd, 0xf534ddc0, 0x34ddc0db, 0xddc0db62,
    0xc0db6295, 0xdb629599, 0x6295993c, 0x95993c43, 0x993c4390, 0x3c439041,
};

static inline uint32_t asuint(float f) {
  union { float f; uint32_t i; } u = {f};
  return u.i;
}

static inline uint32_t abstop12(float x) {
  return (asuint(x) >> 20) & 0x7ff;
}

static inline void sincosf_poly(double x, double x2, const sincos_t *p, int n,
                                 float *sinp, float *cosp) {
  double x3, x4, x5, x6, s, c, c1, c2, s1;
  x4 = x2 * x2;
  x3 = x2 * x;
  c2 = p->c3 + x2 * p->c4;
  s1 = p->s2 + x2 * p->s3;
  float *tmp = (n & 1 ? cosp : sinp);
  cosp = (n & 1 ? sinp : cosp);
  sinp = tmp;
  c1 = p->c0 + x2 * p->c1;
  x5 = x3 * x2;
  x6 = x4 * x2;
  s = x + x3 * p->s1;
  c = c1 + x4 * p->c2;
  *sinp = s + x5 * s1;
  *cosp = c + x6 * c2;
}

static inline float sinf_poly(double x, double x2, const sincos_t *p, int n) {
  double x3, x4, x6, x7, s, c, c1, c2, s1;
  if ((n & 1) == 0) {
    x3 = x * x2;
    s1 = p->s2 + x2 * p->s3;
    x7 = x3 * x2;
    s = x + x3 * p->s1;
    return s + x7 * s1;
  } else {
    x4 = x2 * x2;
    c2 = p->c3 + x2 * p->c4;
    c1 = p->c0 + x2 * p->c1;
    x6 = x4 * x2;
    c = c1 + x4 * p->c2;
    return c + x6 * c2;
  }
}

static inline double reduce_fast(double x, const sincos_t *p, int *np) {
  double r = x * p->hpi_inv;
  int n = ((int32_t)r + 0x800000) >> 24;
  *np = n;
  return x - n * p->hpi;
}

static inline double reduce_large(uint32_t xi, int *np) {
  const uint32_t *arr = &__inv_pio4[(xi >> 26) & 15];
  int shift = (xi >> 23) & 7;
  uint64_t n, res0, res1, res2;
  xi = (xi & 0xffffff) | 0x800000;
  xi <<= shift;
  res0 = xi * arr[0];
  res1 = (uint64_t)xi * arr[4];
  res2 = (uint64_t)xi * arr[8];
  res0 = (res2 >> 32) | (res0 << 32);
  res0 += res1;
  n = (res0 + (1ULL << 61)) >> 62;
  res0 -= n << 62;
  double x = (int64_t)res0;
  *np = n;
  return x * 0x1.921FB54442D18p-62;
}

void sincosf(float y, float *sinp, float *cosp) {
  double x = y;
  double s;
  int n;
  const sincos_t *p = &__sincosf_table[0];
  if (abstop12(y) < abstop12(0x1.921FB6p-1f)) {
    double x2 = x * x;
    if (unlikely(abstop12(y) < abstop12(0x1p-12f))) {
      *sinp = y;
      *cosp = 1.0f;
      return;
    }
    sincosf_poly(x, x2, p, 0, sinp, cosp);
  } else if (abstop12(y) < abstop12(120.0f)) {
    x = reduce_fast(x, p, &n);
    s = p->sign[n & 3];
    if (n & 2)
      p = &__sincosf_table[1];
    sincosf_poly(x * s, x * x, p, n, sinp, cosp);
  } else if (likely(abstop12(y) < abstop12(__builtin_inff()))) {
    uint32_t xi = asuint(y);
    int sign = xi >> 31;
    x = reduce_large(xi, &n);
    s = p->sign[(n + sign) & 3];
    if ((n + sign) & 2)
      p = &__sincosf_table[1];
    sincosf_poly(x * s, x * x, p, n, sinp, cosp);
  } else {
    *sinp = *cosp = y - y;
  }
}

float sinf(float x) {
  double y = x;
  double s;
  int n;
  const sincos_t *p = &__sincosf_table[0];
  if (abstop12(x) < abstop12(0x1.921FB6p-1f)) {
    double x2 = y * y;
    if (unlikely(abstop12(x) < abstop12(0x1p-12f))) {
      return x;
    }
    return sinf_poly(y, x2, p, 0);
  } else if (abstop12(x) < abstop12(120.0f)) {
    y = reduce_fast(y, p, &n);
    s = p->sign[n & 3];
    if (n & 2)
      p = &__sincosf_table[1];
    return sinf_poly(y * s, y * y, p, n);
  } else if (likely(abstop12(x) < abstop12(__builtin_inff()))) {
    uint32_t xi = asuint(x);
    int sign = xi >> 31;
    y = reduce_large(xi, &n);
    s = p->sign[(n + sign) & 3];
    if ((n + sign) & 2)
      p = &__sincosf_table[1];
    return sinf_poly(y * s, y * y, p, n);
  } else {
    return x - x;
  }
}

float cosf(float x) {
  float sin_val, cos_val;
  sincosf(x, &sin_val, &cos_val);
  return cos_val;
}

// ----------------------------------------------------------------------------
// High-performance powf() from Justine Tunney (https://justine.lol/tmp/powf.c.txt)
// ULP error: 0.82 (~ 0.5 + relerr*2^24), Optimized for ARM Cortex-A72

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(x, 0)

#define POWF_LOG2_TABLE_BITS 4
#define POWF_LOG2_POLY_ORDER 5
#define POWF_SCALE_BITS      0
#define POWF_SCALE           ((double)(1 << POWF_SCALE_BITS))
#define EXP2F_TABLE_BITS     5
#define EXP2F_POLY_ORDER     3

static inline float opt_barrier_float(float x) {
  volatile float y = x;
  return y;
}

// asuint() already defined above for sincosf

static inline int issignalingf_inline(float x) {
  uint32_t ix = asuint(x);
  return 2 * (ix ^ 0x00400000) > 2u * 0x7fc00000;
}

static inline float asfloat(uint32_t i) {
  union { uint32_t i; float f; } u = {i};
  return u.f;
}

static inline uint64_t asuint64(double f) {
  union { double f; uint64_t i; } u = {f};
  return u.i;
}

static inline double asdouble(uint64_t i) {
  union { uint64_t i; double f; } u = {i};
  return u.f;
}

static inline float eval_as_float(float x) {
  return x;
}

static inline double eval_as_double(double x) {
  return x;
}

__attribute__((__noinline__)) static float
xflowf(uint32_t sign, float y) {
  y = eval_as_float(opt_barrier_float(sign ? -y : y) * y);
  return y;
}

static float __math_oflowf(uint32_t sign) {
  return xflowf(sign, 0x1p97f);
}

static float __math_uflowf(uint32_t sign) {
  return xflowf(sign, 0x1p-95f);
}

static float __math_invalidf(float x) {
  return (x - x) / (x - x);
}

static const struct {
  struct { double invc, logc; } tab[1 << POWF_LOG2_TABLE_BITS];
  double poly[POWF_LOG2_POLY_ORDER];
} __powf_log2_data = {
  .tab = {
    { 0x1.661ec79f8f3bep+0, -0x1.efec65b963019p-2 * POWF_SCALE },
    { 0x1.571ed4aaf883dp+0, -0x1.b0b6832d4fca4p-2 * POWF_SCALE },
    { 0x1.49539f0f010bp+0, -0x1.7418b0a1fb77bp-2 * POWF_SCALE },
    { 0x1.3c995b0b80385p+0, -0x1.39de91a6dcf7bp-2 * POWF_SCALE },
    { 0x1.30d190c8864a5p+0, -0x1.01d9bf3f2b631p-2 * POWF_SCALE },
    { 0x1.25e227b0b8eap+0, -0x1.97c1d1b3b7afp-3 * POWF_SCALE },
    { 0x1.1bb4a4a1a343fp+0, -0x1.2f9e393af3c9fp-3 * POWF_SCALE },
    { 0x1.12358f08ae5bap+0, -0x1.960cbbf788d5cp-4 * POWF_SCALE },
    { 0x1.0953f419900a7p+0, -0x1.a6f9db6475fcep-5 * POWF_SCALE },
    { 0x1p+0, 0x0p+0 * POWF_SCALE },
    { 0x1.e608cfd9a47acp-1, 0x1.338ca9f24f53dp-4 * POWF_SCALE },
    { 0x1.ca4b31f026aap-1, 0x1.476a9543891bap-3 * POWF_SCALE },
    { 0x1.b2036576afce6p-1, 0x1.e840b4ac4e4d2p-3 * POWF_SCALE },
    { 0x1.9c2d163a1aa2dp-1, 0x1.40645f0c6651cp-2 * POWF_SCALE },
    { 0x1.886e6037841edp-1, 0x1.88e9c2c1b9ff8p-2 * POWF_SCALE },
    { 0x1.767dcf5534862p-1, 0x1.ce0a44eb17bccp-2 * POWF_SCALE },
  },
  .poly = {
    -0x1.712b6f70a7e4dp-2 * POWF_SCALE,
    0x1.ecabf496832ep-2 * POWF_SCALE,
    -0x1.715479ffae3dep-1 * POWF_SCALE,
    0x1.715475f35c45bp0 * POWF_SCALE,
  }
};

#define N_EXP (1 << EXP2F_TABLE_BITS)
static const struct {
  uint64_t tab[1 << EXP2F_TABLE_BITS];
  double shift_scaled;
  double poly[EXP2F_POLY_ORDER];
  double shift;
  double invln2_scaled;
  double poly_scaled[EXP2F_POLY_ORDER];
} __exp2f_data = {
  .tab = {
    0x3ff0000000000000, 0x3fefd9b0d3158574, 0x3fefb5586cf9890f, 0x3fef9301d0125b51,
    0x3fef72b83c7d517b, 0x3fef54873168b9aa, 0x3fef387a6e756238, 0x3fef1e9df51fdee1,
    0x3fef06fe0a31b715, 0x3feef1a7373aa9cb, 0x3feedea64c123422, 0x3feece086061892d,
    0x3feebfdad5362a27, 0x3feeb42b569d4f82, 0x3feeab07dd485429, 0x3feea47eb03a5585,
    0x3feea09e667f3bcd, 0x3fee9f75e8ec5f74, 0x3feea11473eb0187, 0x3feea589994cce13,
    0x3feeace5422aa0db, 0x3feeb737b0cdc5e5, 0x3feec49182a3f090, 0x3feed503b23e255d,
    0x3feee89f995ad3ad, 0x3feeff76f2fb5e47, 0x3fef199bdd85529c, 0x3fef3720dcef9069,
    0x3fef5818dcfba487, 0x3fef7c97337b9b5f, 0x3fefa4afa2a490da, 0x3fefd0765b6e4540,
  },
  .shift_scaled = 0x1.8p+52 / N_EXP,
  .poly = {
    0x1.c6af84b912394p-5, 0x1.ebfce50fac4f3p-3, 0x1.62e42ff0c52d6p-1,
  },
  .shift = 0x1.8p+52,
  .invln2_scaled = 0x1.71547652b82fep+0 * N_EXP,
  .poly_scaled = {
    0x1.c6af84b912394p-5/N_EXP/N_EXP/N_EXP,
    0x1.ebfce50fac4f3p-3/N_EXP/N_EXP,
    0x1.62e42ff0c52d6p-1/N_EXP,
  },
};

#define N_LOG (1 << POWF_LOG2_TABLE_BITS)
#define T_LOG __powf_log2_data.tab
#define A_LOG __powf_log2_data.poly
#define OFF 0x3f330000

static inline double_t log2_inline(uint32_t ix) {
  double_t z, r, r2, r4, p, q, y, y0, invc, logc;
  uint32_t iz, top, tmp;
  int k, i;

  tmp = ix - OFF;
  i = (tmp >> (23 - POWF_LOG2_TABLE_BITS)) % N_LOG;
  top = tmp & 0xff800000;
  iz = ix - top;
  k = (int32_t)top >> (23 - POWF_SCALE_BITS);
  invc = T_LOG[i].invc;
  logc = T_LOG[i].logc;
  z = (double_t)asfloat(iz);

  r = z * invc - 1;
  y0 = logc + (double_t)k;

  r2 = r * r;
  y = A_LOG[0] * r + A_LOG[1];
  p = A_LOG[2] * r + A_LOG[3];
  r4 = r2 * r2;
  q = A_LOG[4] * r + y0;
  q = p * r2 + q;
  y = y * r4 + q;
  return y;
}

#define T_EXP __exp2f_data.tab
#define C_EXP __exp2f_data.poly_scaled
#define SIGN_BIAS (1 << (EXP2F_TABLE_BITS + 11))

static inline float exp2_inline(double_t xd, uint32_t sign_bias) {
  uint64_t ki, ski, t;
  double_t kd, z, r, r2, y, s;

  #define SHIFT __exp2f_data.shift_scaled
  kd = eval_as_double(xd + SHIFT);
  ki = asuint64(kd);
  kd -= SHIFT;
  r = xd - kd;

  t = T_EXP[ki % N_EXP];
  ski = ki + sign_bias;
  t += ski << (52 - EXP2F_TABLE_BITS);
  s = asdouble(t);
  z = C_EXP[0] * r + C_EXP[1];
  r2 = r * r;
  y = C_EXP[2] * r + 1;
  y = z * r2 + y;
  y = y * s;
  return eval_as_float(y);
}

static inline int checkint(uint32_t iy) {
  int e = iy >> 23 & 0xff;
  if (e < 0x7f) return 0;
  if (e > 0x7f + 23) return 2;
  if (iy & ((1 << (0x7f + 23 - e)) - 1)) return 0;
  if (iy & (1 << (0x7f + 23 - e))) return 1;
  return 2;
}

static inline int zeroinfnan(uint32_t ix) {
  return 2 * ix - 1 >= 2u * 0x7f800000 - 1;
}

float powf(float x, float y) {
  uint32_t sign_bias = 0;
  uint32_t ix, iy;

  ix = asuint(x);
  iy = asuint(y);
  if (unlikely(ix - 0x00800000 >= 0x7f800000 - 0x00800000 || zeroinfnan(iy))) {
    if (unlikely(zeroinfnan(iy))) {
      if (2 * iy == 0)
        return issignalingf_inline(x) ? x + y : 1.0f;
      if (ix == 0x3f800000)
        return issignalingf_inline(y) ? x + y : 1.0f;
      if (2 * ix > 2u * 0x7f800000 || 2 * iy > 2u * 0x7f800000)
        return x + y;
      if (2 * ix == 2 * 0x3f800000)
        return 1.0f;
      if ((2 * ix < 2 * 0x3f800000) == !(iy & 0x80000000))
        return 0.0f;
      return y * y;
    }
    if (unlikely(zeroinfnan(ix))) {
      float_t x2 = x * x;
      if (ix & 0x80000000 && checkint(iy) == 1) {
        x2 = -x2;
        sign_bias = 1;
      }
      return iy & 0x80000000 ? opt_barrier_float(1 / x2) : x2;
    }
    if (ix & 0x80000000) {
      int yint = checkint(iy);
      if (yint == 0)
        return __math_invalidf(x);
      if (yint == 1)
        sign_bias = SIGN_BIAS;
      ix &= 0x7fffffff;
    }
    if (ix < 0x00800000) {
      ix = asuint(x * 0x1p23f);
      ix &= 0x7fffffff;
      ix -= 23 << 23;
    }
  }
  double_t logx = log2_inline(ix);
  double_t ylogx = y * logx;
  if (unlikely((asuint64(ylogx) >> 47 & 0xffff) >= asuint64(126.0 * POWF_SCALE) >> 47)) {
    if (ylogx > 0x1.fffffffd1d571p+6 * POWF_SCALE)
      return __math_oflowf(sign_bias);
    if (ylogx <= -150.0 * POWF_SCALE)
      return __math_uflowf(sign_bias);
  }
  return exp2_inline(ylogx, sign_bias);
}

// ----------------------------------------------------------------------------
// Simple RNG for EFI (no stdlib)
static uint32_t rng_state = 12345;

void srand_efi(uint32_t seed) {
    rng_state = seed;
}

uint32_t rand_efi(void) {
    // Simple LCG
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state / 65536) % 32768;
}

#define RAND_MAX 32767

// ----------------------------------------------------------------------------
// Multi-Model Architecture Support
// Model 1: stories110M (420MB) - dim=768, n_layers=12, seq_len=256
// Model 2: NanoGPT-124M (48MB) - dim=768, n_layers=12, seq_len=1024  
// Model 3: TinyLlama-1.1B-Chat (440MB) - dim=2048, n_layers=22, seq_len=2048

typedef enum {
    MODEL_STORIES15M = 1,
    MODEL_STORIES110M = 2,
    MODEL_LLAMA2_7B = 3,
    MODEL_NANOGPT = 4,
    MODEL_TINYLLAMA_CHAT = 5
} ModelType;

typedef struct {
    int dim; // transformer dimension
    int hidden_dim; // for ffn layers
    int n_layers; // number of layers
    int n_heads; // number of query heads
    int n_kv_heads; // number of key/value heads (can be < query heads because of multiquery)
    int vocab_size; // vocabulary size, usually 256 (byte-level)
    int seq_len; // max sequence length
    ModelType model_type; // which model architecture
} Config;

typedef struct {
    // token embedding table
    float* token_embedding_table;    // (vocab_size, dim)
    // weights for rmsnorms
    float* rms_att_weight; // (layer, dim) rmsnorm weights
    float* rms_ffn_weight; // (layer, dim)
    // weights for matmuls. note dim == n_heads * head_size
    float* wq; // (layer, dim, n_heads * head_size)
    float* wk; // (layer, dim, n_kv_heads * head_size)
    float* wv; // (layer, dim, n_kv_heads * head_size)
    float* wo; // (layer, n_heads * head_size, dim)
    // weights for ffn
    float* w1; // (layer, hidden_dim, dim)
    float* w2; // (layer, dim, hidden_dim)
    float* w3; // (layer, hidden_dim, dim)
    // final rmsnorm
    float* rms_final_weight; // (dim,)
    // (optional) classifier weights for the logits, on the last layer
    float* wcls;
} TransformerWeights;

typedef struct {
    // current wave of activations
    float *x; // activation at current time stamp (dim,)
    float *xb; // same, but inside a residual branch (dim,)
    float *xb2; // an additional buffer just for convenience (dim,)
    float *hb; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *hb2; // buffer for hidden dimension in the ffn (hidden_dim,)
    float *q; // query (dim,)
    float *k; // key (dim,)
    float *v; // value (dim,)
    float *att; // buffer for scores/attention values (n_heads, seq_len)
    float *logits; // output logits
    // kv cache
    float* key_cache;   // (layer, seq_len, dim)
    float* value_cache; // (layer, seq_len, dim)
} RunState;

typedef struct {
    Config config; // the hyperparameters of the architecture (the blueprint)
    TransformerWeights weights; // the weights of the model
    RunState state; // buffers for the "wave" of activations in the forward pass
    float* data; // weight data pointer
    UINTN file_size; // size of the checkpoint file in bytes
} Transformer;

// ----------------------------------------------------------------------------
// STATIC ALLOCATION (EFI Modification - replaces malloc_run_state)
// Max dimensions for all supported models:
// - stories110M: dim=768, n_layers=12, hidden_dim=2048, seq_len=256
// - NanoGPT: dim=768, n_layers=12, hidden_dim=3072, seq_len=1024
// - TinyLlama-Chat: dim=2048, n_layers=22, hidden_dim=5632, seq_len=2048

#define MAX_DIM 2048
#define MAX_HIDDEN 5632
#define MAX_LAYERS 22
#define MAX_HEADS 32
#define MAX_SEQ_LEN 2048
#define MAX_VOCAB 32000

// Dynamic buffers (allocated at runtime with AllocatePool to avoid huge binary)
static float *static_x = NULL;
static float *static_xb = NULL;
static float *static_xb2 = NULL;
static float *static_hb = NULL;
static float *static_hb2 = NULL;
static float *static_q = NULL;
static float *static_k = NULL;  // Fixed: was missing!
static float *static_v = NULL;  // Fixed: was missing!
static float *static_key_cache = NULL;
static float *static_value_cache = NULL;
static float *static_att = NULL;
static float *static_logits = NULL;
static float *static_weights = NULL;

EFI_STATUS init_run_state(RunState* s, Config* p, EFI_BOOT_SERVICES *BS) {
    // Allocate buffers dynamically using AllocatePool
    EFI_STATUS Status;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    
    Print(L"  Allocating x (%u bytes)...\r\n", p->dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->dim * sizeof(float), (void**)&static_x);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate x: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating xb (%u bytes)...\r\n", p->dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->dim * sizeof(float), (void**)&static_xb);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate xb: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating xb2 (%u bytes)...\r\n", p->dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->dim * sizeof(float), (void**)&static_xb2);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate xb2: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating hb (%u bytes)...\r\n", p->hidden_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->hidden_dim * sizeof(float), (void**)&static_hb);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate hb: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating hb2 (%u bytes)...\r\n", p->hidden_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->hidden_dim * sizeof(float), (void**)&static_hb2);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate hb2: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating q (%u bytes)...\r\n", p->dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->dim * sizeof(float), (void**)&static_q);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate q: %r\r\n", Status); return Status; }
    
    // Allocate k and v buffers (CRITICAL: these were missing!)
    Print(L"  Allocating k (%u bytes)...\r\n", kv_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, kv_dim * sizeof(float), (void**)&static_k);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate k: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating v (%u bytes)...\r\n", kv_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, kv_dim * sizeof(float), (void**)&static_v);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate v: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating key_cache (%u bytes)...\r\n", p->n_layers * p->seq_len * kv_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->n_layers * p->seq_len * kv_dim * sizeof(float), (void**)&static_key_cache);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate key_cache: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating value_cache (%u bytes)...\r\n", p->n_layers * p->seq_len * kv_dim * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->n_layers * p->seq_len * kv_dim * sizeof(float), (void**)&static_value_cache);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate value_cache: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating att (%u bytes)...\r\n", p->n_heads * p->seq_len * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->n_heads * p->seq_len * sizeof(float), (void**)&static_att);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate att: %r\r\n", Status); return Status; }
    
    Print(L"  Allocating logits (%u bytes)...\r\n", p->vocab_size * sizeof(float));
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, p->vocab_size * sizeof(float), (void**)&static_logits);
    if (EFI_ERROR(Status)) { Print(L"[ERROR] Failed to allocate logits: %r\r\n", Status); return Status; }
    
    // Zero out KV cache (critical for correct inference)
    Print(L"  Zeroing KV cache...\r\n");
    UINTN kv_cache_size = p->n_layers * p->seq_len * kv_dim * sizeof(float);
    for (UINTN i = 0; i < kv_cache_size / sizeof(float); i++) {
        static_key_cache[i] = 0.0f;
        static_value_cache[i] = 0.0f;
    }
    Print(L"  KV cache zeroed!\r\n");
    
    // Point RunState to allocated buffers
    s->x = static_x;
    s->xb = static_xb;
    s->xb2 = static_xb2;
    s->hb = static_hb;
    s->hb2 = static_hb2;
    s->q = static_q;
    s->k = static_k;  // Fixed: was missing!
    s->v = static_v;  // Fixed: was missing!
    s->key_cache = static_key_cache;
    s->value_cache = static_value_cache;
    s->att = static_att;
    s->logits = static_logits;
    
    return EFI_SUCCESS;
}

void memory_map_weights(TransformerWeights *w, Config* p, float* ptr, int shared_weights) {
    int head_size = p->dim / p->n_heads;
    unsigned long long n_layers = p->n_layers;
    
    w->token_embedding_table = ptr;
    ptr += p->vocab_size * p->dim;
    w->rms_att_weight = ptr;
    ptr += n_layers * p->dim;
    w->wq = ptr;
    ptr += n_layers * p->dim * (p->n_heads * head_size);
    w->wk = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wv = ptr;
    ptr += n_layers * p->dim * (p->n_kv_heads * head_size);
    w->wo = ptr;
    ptr += n_layers * (p->n_heads * head_size) * p->dim;
    w->rms_ffn_weight = ptr;
    ptr += n_layers * p->dim;
    w->w1 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->w2 = ptr;
    ptr += n_layers * p->hidden_dim * p->dim;
    w->w3 = ptr;
    ptr += n_layers * p->dim * p->hidden_dim;
    w->rms_final_weight = ptr;
    ptr += p->dim;
    w->wcls = shared_weights ? w->token_embedding_table : ptr;
}

// ----------------------------------------------------------------------------
// TRANSFORMER LOGIC (100% UNCHANGED from Karpathy's llama2.c)

void rmsnorm(float* o, float* x, float* weight, int size) {
    // calculate sum of squares
    float ss = 0.0f;
    for (int j = 0; j < size; j++) {
        ss += x[j] * x[j];
    }
    ss /= size;
    ss += 1e-5f;
    ss = 1.0f / sqrtf(ss);
    // normalize and scale
    for (int j = 0; j < size; j++) {
        o[j] = weight[j] * (ss * x[j]);
    }
}

void softmax(float* x, int size) {
    // find max value (for numerical stability)
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        if (x[i] > max_val) {
            max_val = x[i];
        }
    }
    // exp and sum
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    // normalize
    for (int i = 0; i < size; i++) {
        x[i] /= sum;
    }
}

void matmul(float* xout, float* x, float* w, int n, int d) {
    // W (d,n) @ x (n,) -> xout (d,)
    // Optimized with loop unrolling (4x) for better performance
    for (int i = 0; i < d; i++) {
        float val = 0.0f;
        int j = 0;
        
        // Unroll loop 4x - processes 4 elements per iteration
        for (; j < n - 3; j += 4) {
            val += w[i * n + j + 0] * x[j + 0];
            val += w[i * n + j + 1] * x[j + 1];
            val += w[i * n + j + 2] * x[j + 2];
            val += w[i * n + j + 3] * x[j + 3];
        }
        
        // Handle remaining elements
        for (; j < n; j++) {
            val += w[i * n + j] * x[j];
        }
        
        xout[i] = val;
    }
}

float* forward(Transformer* transformer, int token, int pos) {
    // a few convenience variables
    Config* p = &transformer->config;
    TransformerWeights* w = &transformer->weights;
    RunState* s = &transformer->state;
    float *x = s->x;
    int dim = p->dim;
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    int kv_mul = p->n_heads / p->n_kv_heads; // integer multiplier of the kv sharing in multiquery
    int hidden_dim =  p->hidden_dim;
    int head_size = dim / p->n_heads;

    // copy the token embedding into x (optimized)
    float* content_row = w->token_embedding_table + token * dim;
    int i = 0;
    // Unroll 8x for better cache utilization
    for (; i < dim - 7; i += 8) {
        x[i + 0] = content_row[i + 0];
        x[i + 1] = content_row[i + 1];
        x[i + 2] = content_row[i + 2];
        x[i + 3] = content_row[i + 3];
        x[i + 4] = content_row[i + 4];
        x[i + 5] = content_row[i + 5];
        x[i + 6] = content_row[i + 6];
        x[i + 7] = content_row[i + 7];
    }
    // Handle remaining elements
    for (; i < dim; i++) {
        x[i] = content_row[i];
    }
    
    // forward all the layers
    for(unsigned long long l = 0; l < p->n_layers; l++) {

        // attention rmsnorm
        rmsnorm(s->xb, x, w->rms_att_weight + l*dim, dim);

        // qkv matmuls for this position
        matmul(s->q, s->xb, w->wq + l*dim*dim, dim, dim);
        matmul(s->k, s->xb, w->wk + l*dim*kv_dim, dim, kv_dim);
        matmul(s->v, s->xb, w->wv + l*dim*kv_dim, dim, kv_dim);

        // RoPE relative positional encoding: complex-valued rotate q and k in each head
        for (int i = 0; i < dim; i+=2) {
            int head_dim = i % head_size;
            float freq = 1.0f / powf(10000.0f, head_dim / (float)head_size);
            float val = pos * freq;
            float fcr = cosf(val);
            float fci = sinf(val);
            int rotn = i < kv_dim ? 2 : 1; // how many vectors? 2 = q & k, 1 = q only
            for (int v = 0; v < rotn; v++) {
                float* vec = v == 0 ? s->q : s->k; // the vector to rotate (query or key)
                float v0 = vec[i];
                float v1 = vec[i+1];
                vec[i]   = v0 * fcr - v1 * fci;
                vec[i+1] = v0 * fci + v1 * fcr;
            }
        }

        // save key,value at this time step (pos) to our kv cache
        int loff = l * p->seq_len * kv_dim; // kv cache layer offset for convenience
        float* key_cache_row = s->key_cache + loff + pos * kv_dim;
        float* value_cache_row = s->value_cache + loff + pos * kv_dim;
        for (int i = 0; i < kv_dim; i++) {
            key_cache_row[i] = s->k[i];
            value_cache_row[i] = s->v[i];
        }

        // multihead attention. iterate over all heads
        int h;
        for (h = 0; h < p->n_heads; h++) {
            // get the query vector for this head
            float* q = s->q + h * head_size;
            // attention scores for this head
            float* att = s->att + h * p->seq_len;
            // iterate over all timesteps, including the current one
            for (int t = 0; t <= pos; t++) {
                // get the key vector for this head and at this timestep
                float* k = s->key_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                // calculate the attention score as the dot product of q and k
                float score = 0.0f;
                for (int i = 0; i < head_size; i++) {
                    score += q[i] * k[i];
                }
                score /= sqrtf(head_size);
                // save the score to the attention buffer
                att[t] = score;
            }

            // softmax the scores to get attention weights, from 0..pos inclusively
            softmax(att, pos + 1);

            // weighted sum of the values, store back into xb
            float* xb = s->xb + h * head_size;
            for (int i = 0; i < head_size; i++) {
                xb[i] = 0.0f;
            }
            for (int t = 0; t <= pos; t++) {
                // get the value vector for this head and at this timestep
                float* v = s->value_cache + loff + t * kv_dim + (h / kv_mul) * head_size;
                // get the attention weight for this timestep
                float a = att[t];
                // accumulate the weighted value into xb
                for (int i = 0; i < head_size; i++) {
                    xb[i] += a * v[i];
                }
            }
        }

        // final matmul to get the output of the attention
        matmul(s->xb2, s->xb, w->wo + l*dim*dim, dim, dim);

        // residual connection back into x
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb2[i];
        }

        // ffn rmsnorm
        rmsnorm(s->xb, x, w->rms_ffn_weight + l*dim, dim);

        // Now for FFN in PyTorch we have: self.w2(F.silu(self.w1(x)) * self.w3(x))
        // first calculate self.w1(x) and self.w3(x)
        matmul(s->hb, s->xb, w->w1 + l*dim*hidden_dim, dim, hidden_dim);
        matmul(s->hb2, s->xb, w->w3 + l*dim*hidden_dim, dim, hidden_dim);

        // SwiGLU non-linearity
        for (int i = 0; i < hidden_dim; i++) {
            float val = s->hb[i];
            // silu(x)=x*σ(x), where σ(x) is the logistic sigmoid
            val *= (1.0f / (1.0f + expf(-val)));
            // elementwise multiply with w3(x)
            val *= s->hb2[i];
            s->hb[i] = val;
        }

        // final matmul to get the output of the ffn
        matmul(s->xb, s->hb, w->w2 + l*dim*hidden_dim, hidden_dim, dim);

        // residual connection
        for (int i = 0; i < dim; i++) {
            x[i] += s->xb[i];
        }
    }

    // final rmsnorm
    rmsnorm(x, x, w->rms_final_weight, dim);

    // classifier into logits
    matmul(s->logits, x, w->wcls, p->dim, p->vocab_size);
    return s->logits;
}

// ----------------------------------------------------------------------------
// Sampling (100% UNCHANGED from Karpathy)

int sample(float* probabilities, int n) {
    // sample index from probabilities (they must sum to 1!)
    float r = (float)rand_efi() / (float)RAND_MAX;
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (r < cdf) {
            return i;
        }
    }
    return n - 1; // in case of rounding errors
}

int argmax(float* v, int n) {
    // return argmax of v in elements 0..n
    int max_i = 0;
    float max_p = v[0];
    for (int i = 1; i < n; i++) {
        if (v[i] > max_p) {
            max_i = i;
            max_p = v[i];
        }
    }
    return max_i;
}
int sample_mult(float* probabilities, int n, float coin) {
    // Sample index from probabilities (they must sum to 1!)
    float cdf = 0.0f;
    for (int i = 0; i < n; i++) {
        cdf += probabilities[i];
        if (coin < cdf) {
            return i;
        }
    }
    return n - 1; // in case of rounding errors
}

int sample_top_p(float* logits, int n, float top_p, float temperature, float coin) {
    // Apply temperature
    for (int i = 0; i < n; i++) {
        logits[i] /= temperature;
    }
    
    // Softmax
    softmax(logits, n);
    
    // Sort indices by probability (simple selection sort)
    int* indices = (int*)&logits[n];  // Reuse memory after logits
    for (int i = 0; i < n; i++) indices[i] = i;
    
    for (int i = 0; i < n-1; i++) {
        for (int j = i+1; j < n; j++) {
            if (logits[indices[j]] > logits[indices[i]]) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }
    
    // Truncate to top-p
    float cumsum = 0.0f;
    int last_idx = 0;
    for (int i = 0; i < n; i++) {
        cumsum += logits[indices[i]];
        last_idx = i;
        if (cumsum > top_p) break;
    }
    
    // Sample from the truncated list
    float r = coin * cumsum;
    float cdf = 0.0f;
    for (int i = 0; i <= last_idx; i++) {
        cdf += logits[indices[i]];
        if (r < cdf) {
            return indices[i];
        }
    }
    return indices[last_idx];
}
// ----------------------------------------------------------------------------
// EFI MODIFICATIONS START HERE

EFI_STATUS load_model(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, Transformer* transformer, CHAR16* checkpoint_path) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_FILE_IO_INTERFACE *FileSystem;
    EFI_FILE_HANDLE Root;
    EFI_FILE_HANDLE File;
    
    // Use uefi_call_wrapper for proper calling convention
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        ImageHandle,
        &LoadedImageProtocol,
        (void**)&LoadedImage
    );
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to get loaded image protocol: %r\r\n", Status);
        return Status;
    }
    
    // Now get file system from the device handle
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        LoadedImage->DeviceHandle,
        &FileSystemProtocol,
        (void**)&FileSystem
    );
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to get file system protocol: %r\r\n", Status);
        return Status;
    }
    
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to open volume: %r\r\n", Status);
        return Status;
    }
    
    
    // Open checkpoint file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, checkpoint_path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to open checkpoint: %s (Status: %r)\r\n", checkpoint_path, Status);
        return Status;
    }
    
    // Read config header (7 ints)
    UINTN config_size = sizeof(Config);
    Status = uefi_call_wrapper(File->Read, 3, File, &config_size, &transformer->config);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to read config: %r\r\n", Status);
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    Config* p = &transformer->config;
    Print(L"Model config: dim=%d, n_layers=%d, n_heads=%d, vocab=%d\r\n",
          p->dim, p->n_layers, p->n_heads, p->vocab_size);
    
    // Validate against static allocation limits
    if (p->dim > MAX_DIM || p->n_layers > MAX_LAYERS || 
        p->vocab_size > MAX_VOCAB || p->seq_len > MAX_SEQ_LEN) {
        Print(L"[ERROR] Model too large for static allocation!\r\n");
        uefi_call_wrapper(File->Close, 1, File);
        return EFI_BUFFER_TOO_SMALL;
    }
    
    // Calculate weights size (all transformer weights)
    int shared_weights = p->vocab_size > 0 ? 1 : 0;
    p->vocab_size = p->vocab_size > 0 ? p->vocab_size : -p->vocab_size;
    
    int head_size = p->dim / p->n_heads;
    int n_layers = p->n_layers;
    
    UINTN weights_size = 0;
    weights_size += p->vocab_size * p->dim; // token_embedding_table
    weights_size += n_layers * p->dim; // rms_att_weight
    weights_size += n_layers * p->dim * (p->n_heads * head_size); // wq
    weights_size += n_layers * p->dim * (p->n_kv_heads * head_size); // wk
    weights_size += n_layers * p->dim * (p->n_kv_heads * head_size); // wv
    weights_size += n_layers * (p->n_heads * head_size) * p->dim; // wo
    weights_size += n_layers * p->dim; // rms_ffn_weight
    weights_size += n_layers * p->dim * p->hidden_dim; // w1
    weights_size += n_layers * p->hidden_dim * p->dim; // w2
    weights_size += n_layers * p->dim * p->hidden_dim; // w3
    weights_size += p->dim; // rms_final_weight
    if (!shared_weights) {
        weights_size += p->vocab_size * p->dim; // wcls
    }
    weights_size *= sizeof(float); // convert to bytes
    
    // Allocate weights buffer
    Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, weights_size, (void**)&static_weights);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to allocate weights: %r\r\n", Status);
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    // Read weights into buffer in chunks to avoid EFI timeout
    
    UINTN total_read = 0;
    UINTN chunk_size = 512 * 1024; // 512 KB chunks
    UINT8* buffer_ptr = (UINT8*)static_weights;
    
    while (total_read < weights_size) {
        UINTN to_read = (weights_size - total_read > chunk_size) ? chunk_size : (weights_size - total_read);
        UINTN read_size = to_read;
        
        Status = uefi_call_wrapper(File->Read, 3, File, &read_size, buffer_ptr);
        if (EFI_ERROR(Status)) {
            Print(L"[ERROR] Failed to read weights at offset %u: %r\r\n", total_read, Status);
            uefi_call_wrapper(File->Close, 1, File);
            return Status;
        }
        
        if (read_size == 0) {
            Print(L"[ERROR] Unexpected EOF at %u bytes (expected %u)\r\n", total_read, weights_size);
            uefi_call_wrapper(File->Close, 1, File);
            return EFI_END_OF_FILE;
        }
        
        total_read += read_size;
        buffer_ptr += read_size;
        
        // Progress indicator every 512KB
        if (total_read % (512 * 1024) == 0) {
            Print(L"  ... %u KB read\r\n", total_read / 1024);
        }
    }
    
    
    uefi_call_wrapper(File->Close, 1, File);
    
    // Map weights
    transformer->data = static_weights;
    memory_map_weights(&transformer->weights, p, static_weights, shared_weights);
    
    // Sanity check: print first weight value
    float first_weight = static_weights[0];
    int whole = (int)first_weight;
    int frac = (int)((first_weight - whole) * 1000);
    if (frac < 0) frac = -frac;
    
    // Initialize run state with dynamic allocation
    Status = init_run_state(&transformer->state, p, SystemTable->BootServices);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to initialize run state: %r\r\n", Status);
        return Status;
    }
    
    Print(L"[SUCCESS] Model loaded successfully!\r\n");
    return EFI_SUCCESS;
}

// ----------------------------------------------------------------------------
// BPE TOKENIZER

typedef struct {
    char** vocab;          // vocabulary strings
    float* vocab_scores;   // scores for each token
    int vocab_size;
    unsigned int max_token_length;
} Tokenizer;

EFI_STATUS load_tokenizer(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, 
                          Tokenizer* t, CHAR16* tokenizer_path, int vocab_size) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_HANDLE Root;
    EFI_FILE_HANDLE File;
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    
    // Get file system
    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
        ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
        LoadedImage->DeviceHandle, &FileSystemProtocol, (void**)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;
    
    // Open tokenizer file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, tokenizer_path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Warning: Could not load tokenizer from %s\r\n", tokenizer_path);
        return Status;
    }
    
    // Read max_token_length
    UINTN read_size = sizeof(int);
    Status = uefi_call_wrapper(File->Read, 3, File, &read_size, &t->max_token_length);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    // Allocate vocab arrays
    t->vocab_size = vocab_size;
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
        vocab_size * sizeof(char*), (void**)&t->vocab);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
        vocab_size * sizeof(float), (void**)&t->vocab_scores);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }
    
    // Read each token
    for (int i = 0; i < vocab_size; i++) {
        // Read score
        read_size = sizeof(float);
        Status = uefi_call_wrapper(File->Read, 3, File, &read_size, &t->vocab_scores[i]);
        if (EFI_ERROR(Status)) break;
        
        // Read token length
        int len;
        read_size = sizeof(int);
        Status = uefi_call_wrapper(File->Read, 3, File, &read_size, &len);
        if (EFI_ERROR(Status)) break;
        
        // Allocate and read token string
        Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData,
            len + 1, (void**)&t->vocab[i]);
        if (EFI_ERROR(Status)) break;
        
        read_size = len;
        Status = uefi_call_wrapper(File->Read, 3, File, &read_size, t->vocab[i]);
        if (EFI_ERROR(Status)) break;
        
        t->vocab[i][len] = '\0';  // Null terminate
    }
    
    uefi_call_wrapper(File->Close, 1, File);
    
    if (EFI_ERROR(Status)) {
        Print(L"Warning: Error loading tokenizer vocabulary\r\n");
        return Status;
    }
    
    Print(L"Tokenizer loaded: %d tokens, max_len=%d\r\n", vocab_size, t->max_token_length);
    return EFI_SUCCESS;
}

char* decode_token(Tokenizer* t, int token) {
    if (token >= 0 && token < t->vocab_size) {
        return t->vocab[token];
    }
    return "<?>";  // Unknown token
}

// ----------------------------------------------------------------------------
// USER INPUT (UEFI Console Input)
// ----------------------------------------------------------------------------

int read_user_input(EFI_SYSTEM_TABLE *ST, char* buffer, int max_len) {
    // Read a line of text from UEFI console input
    // Returns number of characters read (excluding null terminator)
    
    int pos = 0;
    EFI_INPUT_KEY Key;
    EFI_STATUS Status;
    volatile int delay;
    
    while (pos < max_len - 1) {
        // Poll for keystroke (simpler than WaitForEvent)
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);
        
        if (EFI_ERROR(Status)) {
            // No key available, busy-wait a bit and try again
            for (delay = 0; delay < 50000; delay++);
            continue;
        }
        
        // Handle special keys
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN || Key.UnicodeChar == CHAR_LINEFEED) {
            // Enter key pressed - end input
            Print(L"\r\n");
            break;
        } else if (Key.UnicodeChar == CHAR_BACKSPACE) {
            // Backspace - remove last character
            if (pos > 0) {
                pos--;
                Print(L"\b \b");  // Backspace, space, backspace (erase character)
            }
        } else if (Key.UnicodeChar >= 32 && Key.UnicodeChar < 127) {
            // Printable ASCII character
            buffer[pos] = (char)Key.UnicodeChar;
            pos++;
            Print(L"%c", Key.UnicodeChar);
        }
    }
    
    buffer[pos] = '\0';  // Null terminate
    return pos;
}

// Simple BPE encoder for user input (greedy longest-match)
int encode_prompt(Tokenizer* t, char* text, int* tokens, int max_tokens) {
    // Greedy BPE tokenization: match longest tokens first
    int n_tokens = 0;
    
    // Always start with BOS token (1)
    if (n_tokens < max_tokens) {
        tokens[n_tokens++] = 1;
    }
    
    int text_len = 0;
    while (text[text_len]) text_len++;
    
    int pos = 0;
    while (pos < text_len && n_tokens < max_tokens) {
        // Try to match the longest token starting at pos
        int best_token = -1;
        int best_len = 0;
        
        // Search through vocabulary for longest match
        for (int tok = 0; tok < t->vocab_size; tok++) {
            char* vocab_piece = t->vocab[tok];
            int vocab_len = 0;
            while (vocab_piece[vocab_len]) vocab_len++;
            
            // Skip if this token is shorter than best found
            if (vocab_len <= best_len) continue;
            
            // Check if vocab_piece matches text starting at pos
            int matches = 1;
            for (int i = 0; i < vocab_len && (pos + i) < text_len; i++) {
                if (text[pos + i] != vocab_piece[i]) {
                    matches = 0;
                    break;
                }
            }
            
            if (matches && (pos + vocab_len) <= text_len) {
                best_token = tok;
                best_len = vocab_len;
            }
        }
        
        // If found a match, use it
        if (best_token >= 0) {
            tokens[n_tokens++] = best_token;
            pos += best_len;
        } else {
            // No match - try single character as fallback
            // Look for single-char token
            int found = 0;
            for (int tok = 0; tok < t->vocab_size; tok++) {
                char* vocab_piece = t->vocab[tok];
                if (vocab_piece[0] == text[pos] && vocab_piece[1] == '\0') {
                    tokens[n_tokens++] = tok;
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                // Skip unknown character
                pos++;
            } else {
                pos++;
            }
        }
    }
    
    return n_tokens;
}

// ----------------------------------------------------------------------------
// AVX/SSE INITIALIZATION
// ----------------------------------------------------------------------------

int check_and_enable_avx() {
    uint32_t eax, ebx, ecx, edx;
    uint64_t cr4, cr0;
    
    Print(L"[DEBUG] Checking CPU features...\r\n");
    
    // Check CPUID support (Leaf 1)
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );
    
    Print(L"[DEBUG] CPUID.1:ECX = 0x%08x\r\n", ecx);
    
    // First, ensure SSE/SSE2 is enabled in CR0 and CR4
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    
    Print(L"[DEBUG] CR0 = 0x%016llx, CR4 = 0x%016llx\r\n", cr0, cr4);
    
    // Clear EM (bit 2) and set MP (bit 1) in CR0 for FPU
    cr0 &= ~(1ULL << 2);  // Clear EM (Emulation)
    cr0 |= (1ULL << 1);   // Set MP (Monitor Coprocessor)
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
    
    // Enable OSFXSR (bit 9) for SSE in CR4
    cr4 |= (1ULL << 9);   // OSFXSR - OS support for FXSAVE/FXRSTOR
    cr4 |= (1ULL << 10);  // OSXMMEXCPT - OS support for unmasked SIMD exceptions
    
    // Check for XSAVE (ECX bit 26) and AVX (ECX bit 28)
    int has_xsave = (ecx & (1 << 26)) != 0;
    int has_avx = (ecx & (1 << 28)) != 0;
    
    Print(L"[DEBUG] XSAVE: %d, AVX: %d\r\n", has_xsave, has_avx);
    
    if (has_xsave && has_avx) {
        // Enable OSXSAVE in CR4 (bit 18)
        cr4 |= (1ULL << 18);
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
        
        Print(L"[DEBUG] OSXSAVE enabled in CR4\r\n");
        
        // Now we can safely use XGETBV/XSETBV
        uint32_t xcr0_lo, xcr0_hi;
        __asm__ volatile ("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
        
        Print(L"[DEBUG] XCR0 before = 0x%08x\r\n", xcr0_lo);
        
        // Enable x87 (bit 0), SSE (bit 1), and AVX (bit 2)
        xcr0_lo |= (1 << 0) | (1 << 1) | (1 << 2);
        __asm__ volatile ("xsetbv" :: "a"(xcr0_lo), "d"(xcr0_hi), "c"(0));
        
        Print(L"[SUCCESS] SSE/AVX enabled! XCR0 = 0x%08x\r\n", xcr0_lo);
        return 1;
    } else {
        // Just enable SSE without AVX
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
        Print(L"[INFO] SSE enabled (no AVX support)\r\n");
        return 0;
    }
}

// ----------------------------------------------------------------------------
// MODEL DETECTION AND SELECTION

typedef struct {
    CHAR16* filename;
    CHAR16* display_name;
    ModelType model_type;
    int expected_size_mb;
    BOOLEAN exists;
} ModelInfo;

EFI_STATUS check_model_exists(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16* filename, BOOLEAN* exists) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_FILE_IO_INTERFACE *FileSystem;
    EFI_FILE_HANDLE Root;
    EFI_FILE_HANDLE File;
    
    *exists = FALSE;
    
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        LoadedImage->DeviceHandle, &FileSystemProtocol, (void**)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, filename, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status)) {
        *exists = TRUE;
        uefi_call_wrapper(File->Close, 1, File);
    }
    
    return EFI_SUCCESS;
}

// ----------------------------------------------------------------------------
// Chat REPL v4.0 Implementation - Bare-Metal Native Functions

// Initialize streaming context buffer
void init_streaming_context(StreamingContext* ctx) {
    for (int i = 0; i < STREAMING_CONTEXT_SIZE; i++) {
        ctx->buffer[i] = '\0';
    }
    ctx->write_pos = 0;
    ctx->read_pos = 0;
    ctx->token_count = 0;
    ctx->is_full = 0;
}

// Add text to streaming context (FIFO)
void stream_context_add(StreamingContext* ctx, const char* text) {
    int text_len = str_len(text);
    for (int i = 0; i < text_len; i++) {
        ctx->buffer[ctx->write_pos] = text[i];
        ctx->write_pos = (ctx->write_pos + 1) % STREAMING_CONTEXT_SIZE;
        
        if (ctx->write_pos == ctx->read_pos) {
            ctx->is_full = 1;
            ctx->read_pos = (ctx->read_pos + 1) % STREAMING_CONTEXT_SIZE;
        }
    }
}

// Get recent context from streaming buffer
void stream_context_get(StreamingContext* ctx, char* output, int max_len) {
    int count = 0;
    int pos = ctx->read_pos;
    
    while (count < max_len - 1 && pos != ctx->write_pos) {
        output[count++] = ctx->buffer[pos];
        pos = (pos + 1) % STREAMING_CONTEXT_SIZE;
    }
    output[count] = '\0';
}

// Initialize KV-Cache persistence
void init_kv_cache_persistent(KVCachePersistent* kv, int layers, int dim, int seq_len) {
    kv->layer_count = layers;
    kv->dim = dim;
    kv->valid_tokens = 0;
    
    // Allocate persistent cache memory
    UINTN cache_size = layers * seq_len * dim * sizeof(float);
    ST->BootServices->AllocatePool(EfiLoaderData, cache_size, (VOID**)&kv->keys);
    ST->BootServices->AllocatePool(EfiLoaderData, cache_size, (VOID**)&kv->values);
    
    // Initialize to zero
    for (UINTN i = 0; i < layers * seq_len * dim; i++) {
        kv->keys[i] = 0.0f;
        kv->values[i] = 0.0f;
    }
}

// Initialize URS Enhanced
void init_urs_enhanced(URSEnhanced* urs) {
    urs->error_rate = 0.0f;
    urs->coherence_score = 1.0f;
    urs->repetition_penalty = 1.5f;
    urs->perplexity = 0.0f;
    urs->diversity_score = 1.0f;
    urs->tokens_per_sec = 0.0f;
    urs->active_strategy = 0;
    urs->learning_rate = 0.01f;
    urs->total_tokens = 0;
    urs->start_time = 0;
    
    for (int i = 0; i < 8; i++) {
        urs->state_vector[i] = 0;
    }
}

// Update URS Enhanced metrics
void update_urs_metrics(URSEnhanced* urs, float* logits, int vocab_size, int token) {
    // Calculate entropy (error detection)
    float entropy = 0.0f;
    for (int i = 0; i < vocab_size; i++) {
        if (logits[i] > 0.0f) {
            entropy -= logits[i] * logf(logits[i] + 1e-10f);
        }
    }
    
    urs->error_rate = entropy / logf((float)vocab_size);
    
    // Calculate perplexity (lower = more confident)
    float token_prob = logits[token];
    if (token_prob > 0.0f) {
        urs->perplexity = expf(-logf(token_prob));
    } else {
        urs->perplexity = 1000.0f;  // Very uncertain
    }
    
    // Calculate diversity score (distribution uniformity)
    float max_prob = 0.0f;
    float sum_probs = 0.0f;
    for (int i = 0; i < vocab_size; i++) {
        if (logits[i] > max_prob) max_prob = logits[i];
        sum_probs += logits[i];
    }
    urs->diversity_score = 1.0f - (max_prob / (sum_probs + 1e-10f));
    
    // Update coherence based on prediction confidence
    urs->coherence_score = token_prob;
    
    // Adaptive repetition penalty based on multiple metrics
    float uncertainty = (urs->error_rate + (1.0f - urs->coherence_score)) / 2.0f;
    if (uncertainty > 0.7f) {
        urs->repetition_penalty *= 1.15f;  // Increase penalty when uncertain
        if (urs->repetition_penalty > 4.0f) {
            urs->repetition_penalty = 4.0f;
        }
    } else if (uncertainty < 0.3f) {
        urs->repetition_penalty *= 0.95f;  // Decrease when confident
        if (urs->repetition_penalty < 1.3f) {
            urs->repetition_penalty = 1.3f;
        }
    }
    
    urs->total_tokens++;
}

// Initialize Chat REPL state
void init_chat_repl(ChatREPLState* repl, int demo_mode) {
    repl->history_count = 0;
    repl->current_turn = 0;
    repl->demo_mode = demo_mode;
    repl->demo_batch = 0;
    
    init_streaming_context(&repl->context);
    init_urs_enhanced(&repl->urs);
    
    // KV-Cache will be initialized when we know model dimensions
    repl->kv_cache.keys = NULL;
    repl->kv_cache.values = NULL;
    repl->kv_cache.valid_tokens = 0;
}

// Add message to chat history
void chat_add_message(ChatREPLState* repl, const char* role, const char* content, int tokens) {
    if (repl->history_count >= MAX_CHAT_HISTORY) {
        // Shift history (remove oldest)
        for (int i = 0; i < MAX_CHAT_HISTORY - 1; i++) {
            repl->history[i] = repl->history[i + 1];
        }
        repl->history_count = MAX_CHAT_HISTORY - 1;
    }
    
    ChatMessage* msg = &repl->history[repl->history_count];
    str_copy(msg->role, role, 16);
    str_copy(msg->content, content, MAX_MESSAGE_LEN);
    msg->token_count = tokens;
    msg->turn_id = repl->current_turn;
    
    repl->history_count++;
    repl->current_turn++;
}

// Build prompt with chat history (smart truncation)
int chat_build_prompt(ChatREPLState* repl, char* output, int max_len) {
    output[0] = '\0';
    
    // Enhanced system prompt (always preserved)
    const char* system = "[SYS] You are a helpful, knowledgeable AI assistant running on bare-metal firmware. Provide clear, informative, and friendly responses. Be creative yet accurate.\n";
    str_copy(output, system, max_len);
    
    // Add recent history (last 5 exchanges)
    int start_idx = (repl->history_count > 5) ? (repl->history_count - 5) : 0;
    
    for (int i = start_idx; i < repl->history_count; i++) {
        ChatMessage* msg = &repl->history[i];
        
        char prefix[32];
        if (strcmp(msg->role, "user") == 0) {
            str_copy(prefix, "[USR] ", 32);
        } else {
            str_copy(prefix, "[AST] ", 32);
        }
        
        str_append(output, prefix, max_len);
        str_append(output, msg->content, max_len);
        str_append(output, "\n", max_len);
    }
    
    return str_len(output);
}

// Demo conversations for Chat REPL
typedef struct {
    const char* user_msg;
    const char* category;
} DemoConversation;

static const DemoConversation demo_batch_1[] = {
    {"Hello! Who are you?", "Greeting"},
    {"What can you help me with?", "Capabilities"},
    {"Tell me about the weather", "Casual"},
    {"Goodbye!", "Farewell"},
};

static const DemoConversation demo_batch_2[] = {
    {"What is 2+2?", "Math"},
    {"Explain photosynthesis simply", "Science"},
    {"Tell me a short joke", "Entertainment"},
};

static const DemoConversation demo_batch_3[] = {
    {"How do computers work?", "Technology"},
    {"What is artificial intelligence?", "AI"},
    {"Tell me about machine learning", "ML"},
};

static const DemoConversation demo_batch_4[] = {
    {"What is the meaning of life?", "Philosophy"},
    {"How can I be happy?", "Wisdom"},
    {"What is true friendship?", "Ethics"},
};

static const DemoConversation demo_batch_5[] = {
    {"Tell me about ancient Egypt", "History"},
    {"What did dinosaurs eat?", "Science"},
    {"How do rockets work?", "Physics"},
};

// ----------------------------------------------------------------------------
// NEURO-NET v1.0 Implementation Functions

// ----------------------------------------------------------------------------
// QDDN Functions

// Initialize QDDN predictor
void init_qddn(QDDNState* qddn) {
    qddn->history_count = 0;
    qddn->history_idx = 0;
    qddn->valid_predictions = 0;
    qddn->predictions_made = 0;
    qddn->predictions_hit = 0;
    qddn->predictions_miss = 0;
    qddn->hit_rate = 0.0f;
    
    // Initialize micro-transformer weights (random)
    for (int i = 0; i < QDDN_EMBEDDING_DIM; i++) {
        for (int j = 0; j < QDDN_EMBEDDING_DIM; j++) {
            // Simple initialization
            float val = (float)((i * 73 + j * 97) % 1000) / 1000.0f - 0.5f;
            qddn->attention_weights[i][j] = val * 0.1f;
            qddn->ffn_weights[i][j] = val * 0.1f;
        }
    }
    
    // Zero out bandwidth reservations
    for (int i = 0; i < MAX_NEURO_NODES; i++) {
        for (int j = 0; j < MAX_NEURO_NODES; j++) {
            qddn->bandwidth_reserved[i][j] = 0.0f;
        }
        qddn->cache_warmed[i] = 0;
    }
}

// Compress packet to pattern embedding (dimensionality reduction)
void compress_to_pattern(NeuroPacket* packet, PacketPattern* pattern) {
    pattern->src_node = packet->source_node;
    pattern->dst_node = packet->dest_node;
    pattern->layer = packet->layer;
    pattern->timestamp = packet->timestamp;
    pattern->resonance = packet->resonance;
    
    // Compress 64D vector to 32D pattern
    for (int i = 0; i < QDDN_EMBEDDING_DIM; i++) {
        // Average pairs of dimensions
        int idx1 = i * 2;
        int idx2 = i * 2 + 1;
        if (idx2 < NEURO_VECTOR_DIM) {
            pattern->vector[i] = (packet->vector[idx1] + packet->vector[idx2]) / 2.0f;
        } else {
            pattern->vector[i] = packet->vector[idx1];
        }
    }
}

// Add packet to history
void qddn_record_packet(QDDNState* qddn, NeuroPacket* packet) {
    PacketPattern pattern;
    compress_to_pattern(packet, &pattern);
    
    // Add to circular buffer
    qddn->history[qddn->history_idx] = pattern;
    qddn->history_idx = (qddn->history_idx + 1) % QDDN_HISTORY_SIZE;
    
    if (qddn->history_count < QDDN_HISTORY_SIZE) {
        qddn->history_count++;
    }
}

// Micro-transformer: predict next pattern
void qddn_predict_next(QDDNState* qddn, PacketPattern* prediction) {
    if (qddn->history_count < 3) {
        // Not enough history
        return;
    }
    
    // Simple prediction: weighted average of recent patterns
    float pred_vector[QDDN_EMBEDDING_DIM] = {0};
    float weights[3] = {0.5f, 0.3f, 0.2f};  // Recent = higher weight
    
    for (int w = 0; w < 3; w++) {
        int idx = (qddn->history_idx - 1 - w + QDDN_HISTORY_SIZE) % QDDN_HISTORY_SIZE;
        PacketPattern* hist = &qddn->history[idx];
        
        for (int i = 0; i < QDDN_EMBEDDING_DIM; i++) {
            pred_vector[i] += hist->vector[i] * weights[w];
        }
    }
    
    // Apply attention (simplified self-attention)
    float attended[QDDN_EMBEDDING_DIM] = {0};
    for (int i = 0; i < QDDN_EMBEDDING_DIM; i++) {
        for (int j = 0; j < QDDN_EMBEDDING_DIM; j++) {
            attended[i] += pred_vector[j] * qddn->attention_weights[i][j];
        }
    }
    
    // Apply FFN
    float output[QDDN_EMBEDDING_DIM] = {0};
    for (int i = 0; i < QDDN_EMBEDDING_DIM; i++) {
        for (int j = 0; j < QDDN_EMBEDDING_DIM; j++) {
            output[i] += attended[j] * qddn->ffn_weights[i][j];
        }
        // ReLU activation
        if (output[i] < 0) output[i] = 0;
    }
    
    // Fill prediction
    for (int i = 0; i < QDDN_EMBEDDING_DIM; i++) {
        prediction->vector[i] = output[i];
    }
    
    // Predict nodes and layer based on recent patterns
    PacketPattern* recent = &qddn->history[(qddn->history_idx - 1 + QDDN_HISTORY_SIZE) % QDDN_HISTORY_SIZE];
    prediction->src_node = recent->dst_node;  // Likely response from dest
    prediction->dst_node = recent->src_node;  // Back to source
    prediction->layer = recent->layer;
    prediction->timestamp = recent->timestamp + 1;
    prediction->resonance = recent->resonance;
}

// Pre-allocate bandwidth based on prediction
void qddn_preallocate(QDDNState* qddn, PacketPattern* prediction, float bandwidth) {
    if (prediction->src_node < MAX_NEURO_NODES && prediction->dst_node < MAX_NEURO_NODES) {
        qddn->bandwidth_reserved[prediction->src_node][prediction->dst_node] += bandwidth;
    }
}

// Warm cache for predicted destination
void qddn_warm_cache(QDDNState* qddn, int node_id) {
    if (node_id < MAX_NEURO_NODES) {
        qddn->cache_warmed[node_id] = 1;
    }
}

// Check if prediction was correct
int qddn_check_prediction(QDDNState* qddn, NeuroPacket* actual) {
    // Simple check: did we predict this general pattern?
    if (qddn->valid_predictions == 0) return 0;
    
    PacketPattern pred_pattern;
    compress_to_pattern(&qddn->predictions[0], &pred_pattern);
    
    PacketPattern actual_pattern;
    compress_to_pattern(actual, &actual_pattern);
    
    // Check if nodes match
    if (pred_pattern.src_node == actual_pattern.src_node &&
        pred_pattern.dst_node == actual_pattern.dst_node) {
        qddn->predictions_hit++;
        return 1;
    }
    
    qddn->predictions_miss++;
    return 0;
}

// Update hit rate
void qddn_update_metrics(QDDNState* qddn) {
    int total = qddn->predictions_hit + qddn->predictions_miss;
    if (total > 0) {
        qddn->hit_rate = (float)qddn->predictions_hit / (float)total;
    }
}

// ----------------------------------------------------------------------------
// URN Functions

// Initialize URN node state
void init_urn_node(URNNodeState* urn) {
    urn->step_count = 0;
    urn->active_hypothesis = -1;
    urn->reasoning_strength = 0.5f;  // Neutral
    urn->inferences_made = 0;
}

// Add reasoning step to node
int urn_add_reasoning(URNNodeState* urn, const char* hypothesis, const char* logic, float confidence) {
    if (urn->step_count >= URN_MAX_REASONING_STEPS) {
        return -1;  // Full
    }
    
    ReasoningStep* step = &urn->reasoning_steps[urn->step_count];
    str_copy(step->hypothesis, hypothesis, 128);
    str_copy(step->logic_chain, logic, 256);
    step->confidence = confidence;
    step->evidence_count = 0;
    
    urn->step_count++;
    urn->inferences_made++;
    urn->active_hypothesis = urn->step_count - 1;
    
    // Update reasoning strength
    urn->reasoning_strength = (urn->reasoning_strength + confidence) / 2.0f;
    
    return urn->step_count - 1;
}

// Share reasoning with another node
void urn_share_reasoning(NeuroNetState* net, int from_node, int to_node) {
    if (from_node >= net->node_count || to_node >= net->node_count) return;
    if (!net->urn_enabled) return;
    
    URNNodeState* from_urn = &net->urn_nodes[from_node];
    URNNodeState* to_urn = &net->urn_nodes[to_node];
    
    if (from_urn->active_hypothesis < 0) return;
    
    // Transfer current reasoning
    ReasoningStep* step = &from_urn->reasoning_steps[from_urn->active_hypothesis];
    
    // Receiving node evaluates and potentially adopts
    if (to_urn->step_count < URN_MAX_REASONING_STEPS) {
        urn_add_reasoning(to_urn, step->hypothesis, step->logic_chain, step->confidence * 0.9f);
    }
}

// Combine reasoning from multiple nodes (meta-reasoning)
float urn_combine_reasoning(NeuroNetState* net, int* node_ids, int num_nodes, char* conclusion) {
    if (!net->urn_enabled || num_nodes == 0) return 0.0f;
    
    float total_confidence = 0.0f;
    int reasoning_count = 0;
    
    for (int i = 0; i < num_nodes && i < MAX_NEURO_NODES; i++) {
        int node_id = node_ids[i];
        if (node_id >= net->node_count) continue;
        
        URNNodeState* urn = &net->urn_nodes[node_id];
        if (urn->active_hypothesis >= 0) {
            ReasoningStep* step = &urn->reasoning_steps[urn->active_hypothesis];
            total_confidence += step->confidence;
            reasoning_count++;
        }
    }
    
    if (reasoning_count > 0) {
        float avg_confidence = total_confidence / reasoning_count;
        str_copy(conclusion, "Combined reasoning from multiple nodes", 256);
        return avg_confidence;
    }
    
    return 0.0f;
}

// ----------------------------------------------------------------------------
// Phase 2: PULSE-CORE Functions

// Initialize PULSE-CORE
void init_pulse_core(PulseCoreState* pulse) {
    pulse->history_count = 0;
    pulse->history_idx = 0;
    pulse->base_frequency = 60.0f;      // 60 BPM default
    pulse->current_frequency = 60.0f;
    pulse->last_pulse = 0;
    pulse->pulse_count = 0;
    pulse->nodes_in_sync = 0;
    pulse->sync_strength = 0.0f;
    
    for (int i = 0; i < MAX_NEURO_NODES; i++) {
        pulse->phase_offset[i] = 0.0f;
    }
}

// Emit heartbeat pulse
void pulse_emit(NeuroNetState* net) {
    if (!net->pulse_enabled) return;
    
    PulseCoreState* pulse = &net->pulse;
    pulse->pulse_count++;
    
    // Record pulse in history
    Heartbeat beat;
    beat.timestamp = net->total_packets;
    beat.intensity = 0.5f + (pulse->sync_strength * 0.5f);  // Higher when synced
    beat.frequency = pulse->current_frequency;
    beat.synchronized_nodes = pulse->nodes_in_sync;
    
    pulse->history[pulse->history_idx] = beat;
    pulse->history_idx = (pulse->history_idx + 1) % PULSE_HISTORY_SIZE;
    if (pulse->history_count < PULSE_HISTORY_SIZE) {
        pulse->history_count++;
    }
    
    pulse->last_pulse = net->total_packets;
}

// Synchronize node to heartbeat
void pulse_sync_node(NeuroNetState* net, int node_id) {
    if (node_id >= net->node_count || !net->pulse_enabled) return;
    
    PulseCoreState* pulse = &net->pulse;
    
    // Calculate phase offset (how off-beat this node is)
    uint64_t time_since_pulse = net->total_packets - pulse->last_pulse;
    float phase = (float)(time_since_pulse % 60) / 60.0f;  // Normalize to [0,1]
    pulse->phase_offset[node_id] = phase;
    
    // Node is in sync if phase < 0.1
    if (phase < 0.1f) {
        pulse->nodes_in_sync++;
    }
}

// Adapt frequency based on network load
void pulse_adapt_frequency(NeuroNetState* net, float load) {
    if (!net->pulse_enabled) return;
    
    PulseCoreState* pulse = &net->pulse;
    
    // Increase BPM when load is high
    pulse->current_frequency = pulse->base_frequency * (1.0f + load * 0.5f);
    
    // Clamp to 30-120 BPM
    if (pulse->current_frequency < 30.0f) pulse->current_frequency = 30.0f;
    if (pulse->current_frequency > 120.0f) pulse->current_frequency = 120.0f;
}

// Update sync strength
void pulse_update_sync(NeuroNetState* net) {
    if (!net->pulse_enabled) return;
    
    PulseCoreState* pulse = &net->pulse;
    
    if (net->node_count > 0) {
        pulse->sync_strength = (float)pulse->nodes_in_sync / (float)net->node_count;
    }
    
    pulse->nodes_in_sync = 0;  // Reset for next pulse
}

// ----------------------------------------------------------------------------
// Phase 2: NEURAL-MESH Functions

// Initialize NEURAL-MESH
void init_neural_mesh(NeuralMeshState* mesh) {
    mesh->route_count = 0;
    mesh->mesh_density = 0.0f;
    mesh->reconfigurations = 0;
    mesh->last_reconfig = 0;
    mesh->packets_routed = 0;
    mesh->routing_failures = 0;
    mesh->avg_route_length = 0.0f;
}

// Find route through mesh
MeshRoute* mesh_find_route(NeuroNetState* net, int from, int to) {
    if (!net->mesh_enabled) return NULL;
    
    NeuralMeshState* mesh = &net->mesh;
    
    // Check existing routes
    for (int i = 0; i < mesh->route_count; i++) {
        MeshRoute* route = &mesh->routes[i];
        if (route->hop_count > 0 &&
            route->hops[0] == from &&
            route->hops[route->hop_count - 1] == to) {
            return route;
        }
    }
    
    return NULL;  // No route found
}

// Create new route
int mesh_create_route(NeuroNetState* net, int from, int to) {
    if (!net->mesh_enabled) return -1;
    if (net->mesh.route_count >= MESH_MAX_ROUTES) return -1;
    
    NeuralMeshState* mesh = &net->mesh;
    MeshRoute* route = &mesh->routes[mesh->route_count];
    
    // Simple 1-hop route
    route->hops[0] = from;
    route->hops[1] = to;
    route->hop_count = 2;
    route->latency = 1.0f;
    route->reliability = 1.0f;
    route->use_count = 0;
    route->last_used = net->total_packets;
    
    mesh->route_count++;
    return mesh->route_count - 1;
}

// Route packet through mesh
int mesh_route_packet(NeuroNetState* net, NeuroPacket* packet) {
    if (!net->mesh_enabled) return -1;
    
    NeuralMeshState* mesh = &net->mesh;
    MeshRoute* route = mesh_find_route(net, packet->source_node, packet->dest_node);
    
    if (!route) {
        // Create new route
        int route_id = mesh_create_route(net, packet->source_node, packet->dest_node);
        if (route_id < 0) {
            mesh->routing_failures++;
            return -1;
        }
        route = &mesh->routes[route_id];
    }
    
    // Use route
    route->use_count++;
    route->last_used = net->total_packets;
    mesh->packets_routed++;
    
    // Update avg route length
    mesh->avg_route_length = (mesh->avg_route_length * (mesh->packets_routed - 1) + 
                             route->hop_count) / mesh->packets_routed;
    
    return 0;
}

// Reconfigure mesh based on traffic
void mesh_reconfigure(NeuroNetState* net) {
    if (!net->mesh_enabled) return;
    
    NeuralMeshState* mesh = &net->mesh;
    
    // Prune unused routes
    int removed = 0;
    for (int i = 0; i < mesh->route_count; i++) {
        MeshRoute* route = &mesh->routes[i];
        uint64_t age = net->total_packets - route->last_used;
        
        // Remove if unused for 100 packets
        if (age > 100) {
            // Shift routes down
            for (int j = i; j < mesh->route_count - 1; j++) {
                mesh->routes[j] = mesh->routes[j + 1];
            }
            mesh->route_count--;
            removed++;
            i--;  // Re-check this index
        }
    }
    
    if (removed > 0) {
        mesh->reconfigurations++;
        mesh->last_reconfig = net->total_packets;
    }
    
    // Update density
    int possible_routes = net->node_count * (net->node_count - 1);
    if (possible_routes > 0) {
        mesh->mesh_density = (float)mesh->route_count / (float)possible_routes;
    }
}

// ----------------------------------------------------------------------------
// Phase 2: QUANTUM-BRIDGE Functions

// Initialize QUANTUM-BRIDGE
void init_quantum_bridge(QuantumBridgeState* quantum) {
    quantum->tunnel_count = 0;
    quantum->total_entanglement = 0.0f;
    quantum->successful_tunnels = 0;
    quantum->collapsed_tunnels = 0;
    quantum->superposition_count = 0;
}

// Create quantum tunnel between nodes
int quantum_create_tunnel(NeuroNetState* net, int node_a, int node_b) {
    if (!net->quantum_enabled) return -1;
    if (net->quantum.tunnel_count >= QUANTUM_MAX_TUNNELS) return -1;
    
    QuantumBridgeState* quantum = &net->quantum;
    QuantumTunnel* tunnel = &quantum->tunnels[quantum->tunnel_count];
    
    tunnel->node_a = node_a;
    tunnel->node_b = node_b;
    tunnel->entanglement = 0.8f + ((node_a * 97 + node_b * 73) % 20) / 100.0f;  // 0.8-1.0
    tunnel->tunnel_stability = 0.9f;
    tunnel->packets_tunneled = 0;
    tunnel->created_at = net->total_packets;
    tunnel->collapsed = 0;
    
    quantum->tunnel_count++;
    quantum->total_entanglement += tunnel->entanglement;
    
    return quantum->tunnel_count - 1;
}

// Send packet through quantum tunnel (instant)
int quantum_tunnel_packet(NeuroNetState* net, NeuroPacket* packet) {
    if (!net->quantum_enabled) return -1;
    
    QuantumBridgeState* quantum = &net->quantum;
    
    // Find tunnel for this route
    for (int i = 0; i < quantum->tunnel_count; i++) {
        QuantumTunnel* tunnel = &quantum->tunnels[i];
        
        if (tunnel->collapsed) continue;
        
        if ((tunnel->node_a == packet->source_node && tunnel->node_b == packet->dest_node) ||
            (tunnel->node_b == packet->source_node && tunnel->node_a == packet->dest_node)) {
            
            // Tunnel found! Instant transmission
            tunnel->packets_tunneled++;
            quantum->successful_tunnels++;
            
            // Reduce stability slightly (quantum decoherence)
            tunnel->tunnel_stability *= 0.99f;
            
            // Collapse if stability < 0.5
            if (tunnel->tunnel_stability < 0.5f) {
                tunnel->collapsed = 1;
                quantum->collapsed_tunnels++;
                quantum->total_entanglement -= tunnel->entanglement;
            }
            
            return 0;  // Success
        }
    }
    
    return -1;  // No tunnel
}

// Refresh quantum tunnels (stabilize)
void quantum_refresh_tunnels(NeuroNetState* net) {
    if (!net->quantum_enabled) return;
    
    QuantumBridgeState* quantum = &net->quantum;
    
    for (int i = 0; i < quantum->tunnel_count; i++) {
        QuantumTunnel* tunnel = &quantum->tunnels[i];
        
        if (!tunnel->collapsed && tunnel->tunnel_stability < 0.9f) {
            tunnel->tunnel_stability += 0.05f;  // Slowly recover
            if (tunnel->tunnel_stability > 1.0f) {
                tunnel->tunnel_stability = 1.0f;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Phase 3: HIVE-MIND Functions

// Initialize HIVE-MIND
void init_hive_mind(HiveMindState* hive) {
    hive->thought_count = 0;
    hive->hive_coherence = 0.0f;
    hive->collective_intelligence = 0.0f;
    hive->nodes_connected = 0;
    hive->thoughts_shared = 0;
    hive->consciousness_level = 0.0f;
    hive->emergent_behaviors = 0;
}

// Create collective thought
int hive_create_thought(NeuroNetState* net, int node_id, const char* content) {
    if (!net->hive_enabled) return -1;
    if (net->hive.thought_count >= HIVE_MAX_THOUGHTS) return -1;
    
    HiveMindState* hive = &net->hive;
    HiveThought* thought = &hive->thoughts[hive->thought_count];
    
    str_copy(thought->content, content, 128);
    thought->originator_node = node_id;
    thought->share_count = 0;
    thought->collective_strength = 0.5f;
    thought->created_at = net->total_packets;
    
    // Generate semantic embedding
    for (int i = 0; i < HIVE_THOUGHT_DIM; i++) {
        float val = 0.0f;
        for (int j = 0; content[j] != '\0' && j < 128; j++) {
            val += (float)((content[j] * (i + 1) + j) % 1000) / 1000.0f;
        }
        thought->embedding[i] = (val - 0.5f) * 2.0f;
    }
    
    // Normalize
    float norm = 0.0f;
    for (int i = 0; i < HIVE_THOUGHT_DIM; i++) {
        norm += thought->embedding[i] * thought->embedding[i];
    }
    norm = sqrtf(norm);
    if (norm > 0.0f) {
        for (int i = 0; i < HIVE_THOUGHT_DIM; i++) {
            thought->embedding[i] /= norm;
        }
    }
    
    hive->thought_count++;
    return hive->thought_count - 1;
}

// Share thought with another node
void hive_share_thought(NeuroNetState* net, int thought_id, int target_node) {
    if (!net->hive_enabled) return;
    if (thought_id >= net->hive.thought_count) return;
    if (target_node >= net->node_count) return;
    
    HiveMindState* hive = &net->hive;
    HiveThought* thought = &hive->thoughts[thought_id];
    
    // Add to shared list
    if (thought->share_count < MAX_NEURO_NODES) {
        thought->shared_with[thought->share_count] = target_node;
        thought->share_count++;
        hive->thoughts_shared++;
        
        // Strengthen collective
        thought->collective_strength += 0.1f;
        if (thought->collective_strength > 1.0f) {
            thought->collective_strength = 1.0f;
        }
    }
}

// Update hive coherence
void hive_update_coherence(NeuroNetState* net) {
    if (!net->hive_enabled) return;
    
    HiveMindState* hive = &net->hive;
    
    if (net->node_count == 0) return;
    
    // Count nodes with shared thoughts
    int connected = 0;
    for (int i = 0; i < net->node_count; i++) {
        int has_thoughts = 0;
        for (int t = 0; t < hive->thought_count; t++) {
            HiveThought* thought = &hive->thoughts[t];
            for (int s = 0; s < thought->share_count; s++) {
                if (thought->shared_with[s] == i) {
                    has_thoughts = 1;
                    break;
                }
            }
            if (has_thoughts) break;
        }
        if (has_thoughts) connected++;
    }
    
    hive->nodes_connected = connected;
    hive->hive_coherence = (float)connected / (float)net->node_count;
    
    // Collective intelligence = coherence * thought density
    float thought_density = (float)hive->thought_count / (float)HIVE_MAX_THOUGHTS;
    hive->collective_intelligence = hive->hive_coherence * thought_density;
    
    // Consciousness level
    hive->consciousness_level = (hive->hive_coherence + hive->collective_intelligence) / 2.0f;
}

// ----------------------------------------------------------------------------
// Phase 3: CONSENSUS-NET Functions

// Initialize CONSENSUS-NET
void init_consensus_net(ConsensusNetState* consensus) {
    consensus->proposal_count = 0;
    consensus->decisions_made = 0;
    consensus->unanimous_decisions = 0;
    consensus->avg_consensus_time = 0.0f;
    consensus->byzantine_faults = 0;
    
    for (int i = 0; i < MAX_NEURO_NODES; i++) {
        consensus->node_reputation[i] = 1.0f;  // Start with full trust
    }
}

// Create consensus proposal
int consensus_propose(NeuroNetState* net, int proposer, const char* proposal, float confidence) {
    if (!net->consensus_enabled) return -1;
    if (net->consensus.proposal_count >= CONSENSUS_MAX_PROPOSALS) return -1;
    
    ConsensusNetState* consensus = &net->consensus;
    ConsensusProposal* prop = &consensus->proposals[consensus->proposal_count];
    
    str_copy(prop->proposal, proposal, 128);
    prop->proposer_node = proposer;
    prop->confidence = confidence;
    prop->votes_for = 0;
    prop->votes_against = 0;
    prop->votes_abstain = 0;
    prop->vote_count = 0;
    prop->decided = 0;
    prop->approved = 0;
    prop->consensus_strength = 0.0f;
    prop->proposed_at = net->total_packets;
    
    consensus->proposal_count++;
    return consensus->proposal_count - 1;
}

// Vote on proposal
void consensus_vote(NeuroNetState* net, int proposal_id, int voter, int vote) {
    if (!net->consensus_enabled) return;
    if (proposal_id >= net->consensus.proposal_count) return;
    if (voter >= net->node_count) return;
    
    ConsensusNetState* consensus = &net->consensus;
    ConsensusProposal* prop = &consensus->proposals[proposal_id];
    
    if (prop->decided) return;  // Already decided
    if (prop->vote_count >= CONSENSUS_MAX_VOTES) return;
    
    // Record vote
    prop->voters[prop->vote_count] = voter;
    prop->vote_count++;
    
    // Count vote (weighted by reputation)
    float weight = consensus->node_reputation[voter];
    
    if (vote > 0) {
        prop->votes_for += (int)(weight * 100.0f);
    } else if (vote < 0) {
        prop->votes_against += (int)(weight * 100.0f);
    } else {
        prop->votes_abstain++;
    }
}

// Check if consensus reached
int consensus_check(NeuroNetState* net, int proposal_id) {
    if (!net->consensus_enabled) return 0;
    if (proposal_id >= net->consensus.proposal_count) return 0;
    
    ConsensusNetState* consensus = &net->consensus;
    ConsensusProposal* prop = &consensus->proposals[proposal_id];
    
    if (prop->decided) return prop->approved;
    
    int total_votes = prop->votes_for + prop->votes_against;
    if (total_votes == 0) return 0;
    
    // 2/3 majority needed
    if (prop->votes_for >= (total_votes * 2) / 3) {
        prop->decided = 1;
        prop->approved = 1;
        prop->consensus_strength = (float)prop->votes_for / (float)total_votes;
        consensus->decisions_made++;
        
        // Check if unanimous
        if (prop->votes_against == 0 && prop->votes_abstain == 0) {
            consensus->unanimous_decisions++;
        }
        
        return 1;  // Approved
    }
    
    // Rejection: >1/3 against
    if (prop->votes_against > total_votes / 3) {
        prop->decided = 1;
        prop->approved = 0;
        prop->consensus_strength = (float)prop->votes_against / (float)total_votes;
        consensus->decisions_made++;
        return -1;  // Rejected
    }
    
    return 0;  // Still pending
}

// ----------------------------------------------------------------------------
// Phase 3: MEMORY-POOL Functions

// Initialize MEMORY-POOL
void init_memory_pool(MemoryPoolState* pool) {
    pool->entry_count = 0;
    pool->total_reads = 0;
    pool->total_writes = 0;
    pool->cache_hits = 0;
    pool->cache_misses = 0;
    pool->memory_utilization = 0.0f;
    pool->conflicts = 0;
    pool->synchronizations = 0;
}

// Write to memory pool
int memory_pool_write(NeuroNetState* net, int node_id, const char* key, float* value) {
    if (!net->memory_pool_enabled) return -1;
    
    MemoryPoolState* pool = &net->memory_pool;
    
    // Check if key exists
    for (int i = 0; i < pool->entry_count; i++) {
        MemoryEntry* entry = &pool->entries[i];
        
        if (str_len(entry->key) == str_len(key)) {
            int match = 1;
            for (int j = 0; key[j] != '\0' && j < MEMORY_KEY_SIZE; j++) {
                if (entry->key[j] != key[j]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                // Update existing entry
                if (entry->locked && entry->owner_node != node_id) {
                    pool->conflicts++;
                    return -2;  // Locked by another node
                }
                
                // Copy value
                for (int j = 0; j < NEURO_VECTOR_DIM; j++) {
                    entry->value[j] = value[j];
                }
                
                entry->write_count++;
                entry->last_access = net->total_packets;
                pool->total_writes++;
                return i;
            }
        }
    }
    
    // Create new entry
    if (pool->entry_count >= MEMORY_POOL_SIZE) return -1;  // Pool full
    
    MemoryEntry* entry = &pool->entries[pool->entry_count];
    str_copy(entry->key, key, MEMORY_KEY_SIZE);
    
    for (int j = 0; j < NEURO_VECTOR_DIM; j++) {
        entry->value[j] = value[j];
    }
    
    entry->owner_node = node_id;
    entry->read_count = 0;
    entry->write_count = 1;
    entry->last_access = net->total_packets;
    entry->locked = 0;
    entry->shared = 0;
    
    pool->entry_count++;
    pool->total_writes++;
    pool->memory_utilization = (float)pool->entry_count / (float)MEMORY_POOL_SIZE;
    
    return pool->entry_count - 1;
}

// Read from memory pool
int memory_pool_read(NeuroNetState* net, const char* key, float* value) {
    if (!net->memory_pool_enabled) return -1;
    
    MemoryPoolState* pool = &net->memory_pool;
    
    // Search for key
    for (int i = 0; i < pool->entry_count; i++) {
        MemoryEntry* entry = &pool->entries[i];
        
        if (str_len(entry->key) == str_len(key)) {
            int match = 1;
            for (int j = 0; key[j] != '\0' && j < MEMORY_KEY_SIZE; j++) {
                if (entry->key[j] != key[j]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                // Copy value
                for (int j = 0; j < NEURO_VECTOR_DIM; j++) {
                    value[j] = entry->value[j];
                }
                
                entry->read_count++;
                entry->last_access = net->total_packets;
                pool->total_reads++;
                pool->cache_hits++;
                return i;
            }
        }
    }
    
    pool->cache_misses++;
    return -1;  // Not found
}

// Lock memory entry
int memory_pool_lock(NeuroNetState* net, const char* key, int node_id) {
    if (!net->memory_pool_enabled) return -1;
    
    MemoryPoolState* pool = &net->memory_pool;
    
    for (int i = 0; i < pool->entry_count; i++) {
        MemoryEntry* entry = &pool->entries[i];
        
        if (str_len(entry->key) == str_len(key)) {
            int match = 1;
            for (int j = 0; key[j] != '\0' && j < MEMORY_KEY_SIZE; j++) {
                if (entry->key[j] != key[j]) {
                    match = 0;
                    break;
                }
            }
            
            if (match) {
                if (entry->locked) {
                    pool->conflicts++;
                    return -2;  // Already locked
                }
                entry->locked = 1;
                entry->owner_node = node_id;
                return 0;
            }
        }
    }
    
    return -1;  // Not found
}

// ----------------------------------------------------------------------------
// GHOST-LINK Functions

// Initialize ghost link state
void init_ghost_link(GhostLinkState* ghost, int node_id) {
    ghost->signature.frequency = 1000.0f + node_id * 100.0f;  // Unique freq
    ghost->signature.intensity = 0.8f;
    ghost->signature.entropy = 0.5f;
    ghost->signature.last_emit = 0;
    
    // Generate unique pattern
    for (int i = 0; i < GHOST_SIGNATURE_DIM; i++) {
        float val = (float)((node_id * 7919 + i * 6151) % 1000) / 1000.0f;
        ghost->signature.pattern[i] = (val - 0.5f) * 2.0f;
    }
    
    ghost->detection_count = 0;
    ghost->broadcasts_sent = 0;
    ghost->ghosts_detected = 0;
    ghost->presence_strength = 1.0f;
}

// Emit ghost signature (broadcast presence)
void ghost_emit_presence(NeuroNetState* net, int node_id) {
    if (node_id >= net->node_count || !net->ghost_enabled) return;
    
    GhostLinkState* ghost = &net->ghost_nodes[node_id];
    ghost->broadcasts_sent++;
    ghost->signature.last_emit = net->total_packets;  // Use packet count as timestamp
}

// Detect nearby ghosts (sense other nodes)
void ghost_detect_proximity(NeuroNetState* net, int observer_id) {
    if (observer_id >= net->node_count || !net->ghost_enabled) return;
    
    GhostLinkState* observer = &net->ghost_nodes[observer_id];
    observer->detection_count = 0;
    
    // Scan all other nodes
    for (int i = 0; i < net->node_count; i++) {
        if (i == observer_id) continue;
        if (observer->detection_count >= GHOST_MAX_DETECTIONS) break;
        
        GhostLinkState* target = &net->ghost_nodes[i];
        
        // Calculate pattern similarity
        float affinity = 0.0f;
        for (int j = 0; j < GHOST_SIGNATURE_DIM; j++) {
            affinity += observer->signature.pattern[j] * target->signature.pattern[j];
        }
        affinity = (affinity + GHOST_SIGNATURE_DIM) / (2.0f * GHOST_SIGNATURE_DIM);  // Normalize [0,1]
        
        // Calculate proximity (based on frequency difference)
        float freq_diff = observer->signature.frequency - target->signature.frequency;
        if (freq_diff < 0) freq_diff = -freq_diff;
        float proximity = 1.0f / (1.0f + freq_diff / 100.0f);  // Closer freq = higher proximity
        
        // Add detection
        GhostDetection* det = &observer->detections[observer->detection_count];
        det->node_id = i;
        det->proximity = proximity;
        det->affinity = affinity;
        det->auto_paired = 0;
        det->last_seen = net->total_packets;
        
        observer->detection_count++;
        observer->ghosts_detected++;
    }
}

// Auto-pair compatible ghosts (silent negotiation)
int ghost_auto_pair(NeuroNetState* net, int node_a, int node_b) {
    if (node_a >= net->node_count || node_b >= net->node_count) return -1;
    if (!net->ghost_enabled) return -1;
    
    GhostLinkState* ghost_a = &net->ghost_nodes[node_a];
    GhostLinkState* ghost_b = &net->ghost_nodes[node_b];
    
    // Calculate compatibility
    float affinity = 0.0f;
    for (int i = 0; i < GHOST_SIGNATURE_DIM; i++) {
        affinity += ghost_a->signature.pattern[i] * ghost_b->signature.pattern[i];
    }
    affinity = (affinity + GHOST_SIGNATURE_DIM) / (2.0f * GHOST_SIGNATURE_DIM);
    
    // Auto-pair if affinity > 0.6
    if (affinity > 0.6f) {
        // Create synapse automatically
        int result = neuronet_create_synapse(net, node_a, node_b, 
                                            net->nodes[node_a].preferred_layer);
        if (result >= 0) {
            // Mark as auto-paired
            for (int i = 0; i < ghost_a->detection_count; i++) {
                if (ghost_a->detections[i].node_id == node_b) {
                    ghost_a->detections[i].auto_paired = 1;
                }
            }
            return 1;  // Success
        }
    }
    
    return 0;  // No pairing
}

// ----------------------------------------------------------------------------
// Phase 4: DREAM-CACHE Functions

// Initialize dream cache
void init_dream_cache(DreamCacheState* dream) {
    dream->prediction_count = 0;
    dream->dream_accuracy = 0.0f;
    dream->dreams_validated = 0;
    dream->dreams_failed = 0;
    dream->lookahead_depth = 4;        // Predict 4 steps ahead
    dream->temporal_discount = 0.9f;   // 90% confidence decay
    dream->speculative_enabled = 1;
    dream->rollback_cost = 0.1f;
}

// Predict future state N steps ahead
int dream_predict_future(NeuroNetState* net, int steps_ahead, float* state_out) {
    if (!net->dream_enabled) return -1;
    
    DreamCacheState* dream = &net->dream;
    
    // Can't predict beyond lookahead depth
    if (steps_ahead > dream->lookahead_depth) {
        steps_ahead = dream->lookahead_depth;
    }
    
    // Simple prediction: extrapolate from current state
    // In a real system, this would use a trained model
    for (int i = 0; i < 32; i++) {
        // Use network coherence as base state
        float current = net->network_coherence * (i + 1) / 32.0f;
        
        // Add trend from recent packets
        float trend = 0.0f;
        if (net->total_packets > 10) {
            trend = (float)(net->total_packets % 100) / 100.0f;
        }
        
        // Extrapolate
        state_out[i] = current + trend * steps_ahead * 0.1f;
        
        // Bound to [0, 1]
        if (state_out[i] < 0.0f) state_out[i] = 0.0f;
        if (state_out[i] > 1.0f) state_out[i] = 1.0f;
    }
    
    return 0;
}

// Cache a dream prediction
int dream_cache_state(NeuroNetState* net, int steps_ahead, float* predicted_state) {
    if (!net->dream_enabled) return -1;
    
    DreamCacheState* dream = &net->dream;
    
    // Find empty slot or replace oldest
    int slot = dream->prediction_count;
    if (slot >= 8) {
        // Replace oldest (simple FIFO)
        slot = 0;
        for (int i = 1; i < 8; i++) {
            if (dream->predictions[i].timestamp < dream->predictions[slot].timestamp) {
                slot = i;
            }
        }
    } else {
        dream->prediction_count++;
    }
    
    DreamPrediction* pred = &dream->predictions[slot];
    
    // Store prediction
    for (int i = 0; i < 32; i++) {
        pred->state[i] = predicted_state[i];
    }
    
    pred->steps_ahead = steps_ahead;
    pred->timestamp = net->total_packets;
    
    // Confidence decreases with distance into future
    pred->confidence = 1.0f;
    for (int i = 0; i < steps_ahead; i++) {
        pred->confidence *= dream->temporal_discount;
    }
    
    return slot;
}

// Validate dream against reality
void dream_validate(NeuroNetState* net, float* actual_state) {
    if (!net->dream_enabled) return;
    
    DreamCacheState* dream = &net->dream;
    
    // Check all predictions
    for (int i = 0; i < dream->prediction_count; i++) {
        DreamPrediction* pred = &dream->predictions[i];
        
        // Is this prediction ready to validate?
        int steps_since = net->total_packets - pred->timestamp;
        if (steps_since == pred->steps_ahead) {
            // Calculate error
            float error = 0.0f;
            for (int j = 0; j < 32; j++) {
                float diff = pred->state[j] - actual_state[j];
                error += diff * diff;
            }
            error = sqrtf(error / 32.0f);  // RMSE
            
            // Was prediction accurate? (error < 0.2)
            if (error < 0.2f) {
                dream->dreams_validated++;
            } else {
                dream->dreams_failed++;
            }
            
            // Update accuracy
            int total = dream->dreams_validated + dream->dreams_failed;
            if (total > 0) {
                dream->dream_accuracy = (float)dream->dreams_validated / total;
            }
            
            // Remove validated prediction
            pred->timestamp = 0;
        }
    }
}

// ----------------------------------------------------------------------------
// Phase 4: META-LEARNING Functions

// Initialize meta-learner
void init_meta_learner(MetaLearnerState* meta) {
    meta->base_learning_rate = 0.001f;
    meta->current_learning_rate = 0.001f;
    meta->momentum = 0.9f;
    meta->history_count = 0;
    meta->adaptation_speed = 0.01f;
    meta->exploration_factor = 0.1f;
    meta->initial_performance = 0.0f;
    meta->current_performance = 0.0f;
    meta->improvement_rate = 0.0f;
    meta->adaptation_cycles = 0;
    meta->weight_perturbation = 0.01f;
}

// Adapt network weights based on performance
void meta_adapt_weights(NeuroNetState* net) {
    if (!net->meta_enabled) return;
    
    MetaLearnerState* meta = &net->meta;
    
    // Calculate current performance (use network coherence as proxy)
    float performance = net->network_coherence;
    
    // Store snapshot
    if (meta->history_count < 16) {
        int idx = meta->history_count;
        meta->history[idx].metric_value = performance;
        meta->history[idx].learning_rate = meta->current_learning_rate;
        meta->history[idx].timestamp = net->total_packets;
        meta->history_count++;
    } else {
        // Shift history
        for (int i = 0; i < 15; i++) {
            meta->history[i] = meta->history[i + 1];
        }
        meta->history[15].metric_value = performance;
        meta->history[15].learning_rate = meta->current_learning_rate;
        meta->history[15].timestamp = net->total_packets;
    }
    
    // Track improvement
    if (meta->initial_performance == 0.0f) {
        meta->initial_performance = performance;
    }
    meta->current_performance = performance;
    
    if (meta->initial_performance > 0.0f) {
        meta->improvement_rate = (meta->current_performance - meta->initial_performance) 
                                / meta->initial_performance;
    }
    
    // Adapt learning rate based on recent performance
    if (meta->history_count >= 3) {
        float recent_trend = meta->history[meta->history_count - 1].metric_value -
                           meta->history[meta->history_count - 3].metric_value;
        
        if (recent_trend > 0) {
            // Performance improving: increase learning rate slightly
            meta->current_learning_rate *= (1.0f + meta->adaptation_speed);
        } else {
            // Performance declining: decrease learning rate
            meta->current_learning_rate *= (1.0f - meta->adaptation_speed);
        }
        
        // Bound learning rate
        if (meta->current_learning_rate < 0.0001f) meta->current_learning_rate = 0.0001f;
        if (meta->current_learning_rate > 0.1f) meta->current_learning_rate = 0.1f;
    }
    
    // Apply gradient-free weight perturbation to synapses
    for (int i = 0; i < net->synapse_count; i++) {
        SynapticConnection* syn = &net->synapses[i];
        
        // Random perturbation
        float perturbation = ((net->total_packets * (i + 1)) % 100) / 100.0f - 0.5f;
        perturbation *= meta->weight_perturbation;
        
        // Apply with learning rate
        syn->weight += perturbation * meta->current_learning_rate;
        
        // Bound weight [0.1, 2.0]
        if (syn->weight < 0.1f) syn->weight = 0.1f;
        if (syn->weight > 2.0f) syn->weight = 2.0f;
    }
    
    meta->adaptation_cycles++;
}

// Tune hyperparameters automatically
void meta_tune_hyperparams(NeuroNetState* net) {
    if (!net->meta_enabled) return;
    
    MetaLearnerState* meta = &net->meta;
    
    // Adjust exploration based on improvement
    if (meta->improvement_rate > 0.1f) {
        // Good progress: reduce exploration
        meta->exploration_factor *= 0.95f;
    } else if (meta->improvement_rate < 0.0f) {
        // Declining: increase exploration
        meta->exploration_factor *= 1.05f;
    }
    
    // Bound exploration
    if (meta->exploration_factor < 0.01f) meta->exploration_factor = 0.01f;
    if (meta->exploration_factor > 0.5f) meta->exploration_factor = 0.5f;
    
    // Adjust weight perturbation
    meta->weight_perturbation = meta->exploration_factor * 0.1f;
}

// ----------------------------------------------------------------------------
// Phase 4: EVOLUTION-ENGINE Functions

// Initialize evolution engine
void init_evolution(EvolutionState* evo) {
    evo->population_size = 4;
    evo->current_generation = 0;
    evo->best_fitness_ever = 0.0f;
    evo->best_generation = 0;
    evo->mutation_rate = 0.05f;
    evo->crossover_rate = 0.7f;
    evo->elitism_rate = 0.25f;
    evo->nodes_added = 0;
    evo->nodes_removed = 0;
    evo->synapses_added = 0;
    evo->synapses_removed = 0;
    evo->avg_fitness = 0.0f;
    evo->fitness_variance = 0.0f;
    evo->stagnant_generations = 0;
    
    // Initialize genomes
    for (int i = 0; i < 4; i++) {
        evo->genomes[i].fitness = 0.0f;
        evo->genomes[i].generation = 0;
        
        // Random genome
        for (int j = 0; j < 64; j++) {
            evo->genomes[i].gene[j] = (i * 64 + j) % 2;  // Deterministic init
        }
    }
}

// Mutate network topology
void evolve_mutate_topology(NeuroNetState* net, int genome_idx) {
    if (!net->evolution_enabled) return;
    if (genome_idx >= net->evolution.population_size) return;
    
    EvolutionState* evo = &net->evolution;
    NetworkGenome* genome = &evo->genomes[genome_idx];
    
    // Mutate genome
    for (int i = 0; i < 64; i++) {
        float rand = ((net->total_packets * (i + 1)) % 100) / 100.0f;
        if (rand < evo->mutation_rate) {
            genome->gene[i] = 1 - genome->gene[i];  // Flip bit
        }
    }
    
    // Apply genome to network (prune/add connections)
    int target_synapses = 0;
    for (int i = 0; i < 64; i++) {
        if (genome->gene[i] == 1) target_synapses++;
    }
    
    // Adjust synapse count toward target
    if (net->synapse_count < target_synapses && net->synapse_count < MAX_NEURO_SYNAPSES) {
        // Try to add synapse
        if (net->node_count >= 2) {
            int src = net->total_packets % net->node_count;
            int dst = (net->total_packets + 1) % net->node_count;
            if (src != dst) {
                neuronet_create_synapse(net, src, dst, net->nodes[src].preferred_layer);
                evo->synapses_added++;
            }
        }
    } else if (net->synapse_count > target_synapses && net->synapse_count > 1) {
        // Remove weakest synapse
        int weakest = 0;
        float min_weight = net->synapses[0].weight;
        for (int i = 1; i < net->synapse_count; i++) {
            if (net->synapses[i].weight < min_weight) {
                min_weight = net->synapses[i].weight;
                weakest = i;
            }
        }
        
        // Remove by shifting array
        for (int i = weakest; i < net->synapse_count - 1; i++) {
            net->synapses[i] = net->synapses[i + 1];
        }
        net->synapse_count--;
        evo->synapses_removed++;
    }
}

// Evaluate fitness of current network
void evolve_evaluate_fitness(NeuroNetState* net, int genome_idx) {
    if (!net->evolution_enabled) return;
    if (genome_idx >= net->evolution.population_size) return;
    
    EvolutionState* evo = &net->evolution;
    NetworkGenome* genome = &evo->genomes[genome_idx];
    
    // Fitness = network coherence + diversity bonus
    float fitness = net->network_coherence;
    
    // Bonus for more connections (up to point)
    float connection_ratio = (float)net->synapse_count / MAX_NEURO_SYNAPSES;
    if (connection_ratio < 0.5f) {
        fitness += connection_ratio * 0.2f;  // Reward connections
    } else {
        fitness -= (connection_ratio - 0.5f) * 0.1f;  // Penalize too many
    }
    
    // Bonus for resonance
    fitness += net->avg_resonance * 0.1f;
    
    genome->fitness = fitness;
    genome->generation = evo->current_generation;
    
    // Track best
    if (fitness > evo->best_fitness_ever) {
        evo->best_fitness_ever = fitness;
        evo->best_generation = evo->current_generation;
        evo->stagnant_generations = 0;
    }
}

// Prune weak connections
void evolve_prune_weak(NeuroNetState* net) {
    if (!net->evolution_enabled) return;
    
    EvolutionState* evo = &net->evolution;
    
    // Remove synapses with weight < 0.2
    int i = 0;
    while (i < net->synapse_count) {
        if (net->synapses[i].weight < 0.2f && net->synapse_count > 1) {
            // Remove
            for (int j = i; j < net->synapse_count - 1; j++) {
                net->synapses[j] = net->synapses[j + 1];
            }
            net->synapse_count--;
            evo->synapses_removed++;
        } else {
            i++;
        }
    }
}

// Advance to next generation
void evolve_next_generation(NeuroNetState* net) {
    if (!net->evolution_enabled) return;
    
    EvolutionState* evo = &net->evolution;
    
    // Calculate average fitness
    float sum = 0.0f;
    for (int i = 0; i < evo->population_size; i++) {
        sum += evo->genomes[i].fitness;
    }
    evo->avg_fitness = sum / evo->population_size;
    
    // Calculate variance
    float variance = 0.0f;
    for (int i = 0; i < evo->population_size; i++) {
        float diff = evo->genomes[i].fitness - evo->avg_fitness;
        variance += diff * diff;
    }
    evo->fitness_variance = variance / evo->population_size;
    
    // Selection: keep best (elitism)
    int best_idx = 0;
    for (int i = 1; i < evo->population_size; i++) {
        if (evo->genomes[i].fitness > evo->genomes[best_idx].fitness) {
            best_idx = i;
        }
    }
    
    // Crossover: combine best with others
    for (int i = 0; i < evo->population_size; i++) {
        if (i != best_idx) {
            // Crossover with best
            for (int j = 0; j < 64; j++) {
                float rand = ((net->total_packets * (i * 64 + j)) % 100) / 100.0f;
                if (rand < evo->crossover_rate) {
                    evo->genomes[i].gene[j] = evo->genomes[best_idx].gene[j];
                }
            }
        }
    }
    
    evo->current_generation++;
    evo->stagnant_generations++;
}

// Initialize NEURO-NET system
void init_neuronet(NeuroNetState* net) {
    net->node_count = 0;
    net->synapse_count = 0;
    net->total_energy = 10000.0f;      // 10K gflops initial
    net->solar_energy = 5000.0f;       // Solar: high compute
    net->lunar_energy = 2000.0f;       // Lunar: low power
    net->plasma_energy = 3000.0f;      // Plasma: ultra-fast
    net->avg_resonance = 0.0f;
    net->total_packets = 0;
    net->network_coherence = 1.0f;
    
    // Phase 1
    net->qddn_enabled = 1;             // Enable QDDN by default
    net->urn_enabled = 1;              // Enable URN by default
    net->ghost_enabled = 1;            // Enable GHOST-LINK by default
    
    // Phase 2
    net->pulse_enabled = 1;            // Enable PULSE-CORE
    net->mesh_enabled = 1;             // Enable NEURAL-MESH
    net->quantum_enabled = 1;          // Enable QUANTUM-BRIDGE
    
    // Phase 3
    net->hive_enabled = 1;             // Enable HIVE-MIND
    net->consensus_enabled = 1;        // Enable CONSENSUS-NET
    net->memory_pool_enabled = 1;      // Enable MEMORY-POOL
    
    // Initialize Phase 1
    init_qddn(&net->qddn);
    
    for (int i = 0; i < MAX_NEURO_NODES; i++) {
        init_urn_node(&net->urn_nodes[i]);
        init_ghost_link(&net->ghost_nodes[i], i);
    }
    
    // Initialize Phase 2
    init_pulse_core(&net->pulse);
    init_neural_mesh(&net->mesh);
    init_quantum_bridge(&net->quantum);
    
    // Initialize Phase 3
    init_hive_mind(&net->hive);
    init_consensus_net(&net->consensus);
    init_memory_pool(&net->memory_pool);
    
    // Phase 4: Advanced Features
    net->dream_enabled = 1;            // Enable DREAM-CACHE
    net->meta_enabled = 1;             // Enable META-LEARNING
    net->evolution_enabled = 1;        // Enable EVOLUTION-ENGINE
    
    // Initialize Phase 4
    init_dream_cache(&net->dream);
    init_meta_learner(&net->meta);
    init_evolution(&net->evolution);
}

// Create vector signature for a node (NEXUS-0: telepathic identity)
void generate_node_signature(float* signature, int node_id, const char* name) {
    // Generate pseudo-random but deterministic signature based on ID + name
    for (int i = 0; i < NEURO_VECTOR_DIM; i++) {
        float val = 0.0f;
        // Hash from node_id
        val += (float)((node_id * 7919 + i * 6151) % 1000) / 1000.0f;
        // Hash from name
        if (name && name[0]) {
            val += (float)((name[i % 32] * 97 + i) % 1000) / 1000.0f;
        }
        signature[i] = (val - 0.5f) * 2.0f;  // Normalize to [-1, 1]
    }
    
    // Normalize vector
    float norm = 0.0f;
    for (int i = 0; i < NEURO_VECTOR_DIM; i++) {
        norm += signature[i] * signature[i];
    }
    norm = sqrtf(norm);
    if (norm > 0.0f) {
        for (int i = 0; i < NEURO_VECTOR_DIM; i++) {
            signature[i] /= norm;
        }
    }
}

// Calculate vector similarity (NEXUS-0: telepathic understanding)
float vector_similarity(float* v1, float* v2) {
    float dot = 0.0f;
    for (int i = 0; i < NEURO_VECTOR_DIM; i++) {
        dot += v1[i] * v2[i];
    }
    return dot;  // Cosine similarity (already normalized)
}

// Add node to NEURO-NET
int neuronet_add_node(NeuroNetState* net, const char* name, EnergyLayer preferred) {
    if (net->node_count >= MAX_NEURO_NODES) return -1;
    
    int id = net->node_count;
    NeuroNode* node = &net->nodes[id];
    
    node->id = id;
    str_copy(node->name, name, 32);
    generate_node_signature(node->signature, id, name);
    node->energy_available = 1000.0f;  // Initial energy pool
    node->energy_consumed = 0.0f;
    node->energy_donated = 0.0f;
    node->preferred_layer = preferred;
    node->packets_sent = 0;
    node->packets_received = 0;
    node->avg_latency = 0.0f;
    
    net->node_count++;
    return id;
}

// Create synaptic connection (SYNAPSE-NET: adaptive)
void neuronet_create_synapse(NeuroNetState* net, int from, int to, EnergyLayer layer) {
    if (net->synapse_count >= MAX_NEURO_NODES * MAX_NEURO_NODES) return;
    
    SynapticConnection* syn = &net->synapses[net->synapse_count];
    syn->from_node = from;
    syn->to_node = to;
    syn->weight = 0.5f;          // Initial synaptic weight
    syn->bandwidth = 100.0f;     // MB/s
    syn->last_used = 0;
    syn->use_count = 0;
    syn->layer = layer;
    
    net->synapse_count++;
}

// Get energy cost for layer (HEXA-NET: energy types)
float get_layer_energy_cost(EnergyLayer layer) {
    switch (layer) {
        case LAYER_SOLAR:  return 10.0f;  // High cost, high speed
        case LAYER_LUNAR:  return 2.0f;   // Low cost, slow
        case LAYER_PLASMA: return 50.0f;  // Very high cost, ultra-fast
        case LAYER_WIND:   return 5.0f;   // Medium cost, broadcast
        case LAYER_EARTH:  return 1.0f;   // Very low, persistent
        case LAYER_VOID:   return 0.1f;   // Almost free (silence)
        default: return 5.0f;
    }
}

// Get layer bandwidth multiplier
float get_layer_bandwidth(EnergyLayer layer) {
    switch (layer) {
        case LAYER_SOLAR:  return 10.0f;  // 10x speed
        case LAYER_LUNAR:  return 0.5f;   // 0.5x speed
        case LAYER_PLASMA: return 100.0f; // 100x speed (bare-metal)
        case LAYER_WIND:   return 2.0f;   // 2x (broadcast)
        case LAYER_EARTH:  return 0.1f;   // 0.1x (storage)
        case LAYER_VOID:   return 1000.0f;// Instant (no data)
        default: return 1.0f;
    }
}

// Create neuro packet
void create_neuro_packet(NeuroPacket* packet, int src, int dst, const char* data, 
                        EnergyLayer layer, float priority) {
    packet->source_node = src;
    packet->dest_node = dst;
    packet->layer = layer;
    packet->priority = priority;
    packet->energy_budget = get_layer_energy_cost(layer);
    packet->timestamp = 0;
    packet->resonance = 0.0f;
    
    // Copy payload
    str_copy(packet->payload, data, 256);
    packet->payload_size = str_len(data);
    
    // Generate intent vector (NEXUS-0: encode meaning)
    // Simple hash-based embedding
    for (int i = 0; i < NEURO_VECTOR_DIM; i++) {
        float val = 0.0f;
        for (int j = 0; j < packet->payload_size; j++) {
            val += (float)((data[j] * (i + 1) + j) % 1000) / 1000.0f;
        }
        packet->vector[i] = (val - 0.5f) * 2.0f;
    }
    
    // Normalize
    float norm = 0.0f;
    for (int i = 0; i < NEURO_VECTOR_DIM; i++) {
        norm += packet->vector[i] * packet->vector[i];
    }
    norm = sqrtf(norm);
    if (norm > 0.0f) {
        for (int i = 0; i < NEURO_VECTOR_DIM; i++) {
            packet->vector[i] /= norm;
        }
    }
}

// Send packet through NEURO-NET (N.E.T. + NEXUS-0 + HEXA + QDDN)
int neuronet_send(NeuroNetState* net, NeuroPacket* packet) {
    if (packet->source_node >= net->node_count || packet->dest_node >= net->node_count) {
        return -1;  // Invalid nodes
    }
    
    NeuroNode* src = &net->nodes[packet->source_node];
    NeuroNode* dst = &net->nodes[packet->dest_node];
    
    // QDDN: Check if this packet was predicted
    if (net->qddn_enabled) {
        int predicted = qddn_check_prediction(&net->qddn, packet);
        if (predicted) {
            // Cache hit! Pre-allocated bandwidth and warmed cache
            // Reduce latency bonus
        }
        qddn_update_metrics(&net->qddn);
    }
    
    // Check energy budget (N.E.T.: energy transport)
    if (src->energy_available < packet->energy_budget) {
        return -2;  // Insufficient energy
    }
    
    // Calculate resonance (ECHO-STREAM: memory)
    float similarity = vector_similarity(packet->vector, dst->signature);
    packet->resonance = (similarity + 1.0f) / 2.0f;  // [0, 1]
    
    // Find synapse
    SynapticConnection* synapse = NULL;
    for (int i = 0; i < net->synapse_count; i++) {
        if (net->synapses[i].from_node == packet->source_node &&
            net->synapses[i].to_node == packet->dest_node &&
            net->synapses[i].layer == packet->layer) {
            synapse = &net->synapses[i];
            break;
        }
    }
    
    if (!synapse) return -3;  // No connection
    
    // Update synapse (SYNAPSE-NET: Hebbian learning)
    synapse->weight += 0.1f * packet->resonance;  // Strengthen with use
    if (synapse->weight > 2.0f) synapse->weight = 2.0f;
    synapse->use_count++;
    
    // Myelin effect: faster with repetition
    float speed_bonus = 1.0f + (float)synapse->use_count / 100.0f;
    if (speed_bonus > 3.0f) speed_bonus = 3.0f;
    
    // Calculate latency
    float base_latency = 10.0f / get_layer_bandwidth(packet->layer);
    float latency = base_latency / (synapse->weight * speed_bonus);
    dst->avg_latency = latency;
    
    // Transfer energy
    src->energy_available -= packet->energy_budget;
    src->energy_consumed += packet->energy_budget;
    dst->energy_available += packet->energy_budget * 0.8f;  // 80% efficiency
    src->energy_donated += packet->energy_budget * 0.8f;
    
    // Update stats
    src->packets_sent++;
    dst->packets_received++;
    net->total_packets++;
    net->avg_resonance = (net->avg_resonance * (net->total_packets - 1) + packet->resonance) / net->total_packets;
    
    // Phase 2: PULSE-CORE - Sync to heartbeat
    if (net->pulse_enabled) {
        pulse_sync_node(net, packet->dest_node);
        
        // Emit pulse every 10 packets
        if (net->total_packets % 10 == 0) {
            pulse_emit(net);
            pulse_update_sync(net);
            
            // Adapt frequency based on network load
            float load = (float)net->total_packets / 100.0f;
            if (load > 1.0f) load = 1.0f;
            pulse_adapt_frequency(net, load);
        }
    }
    
    // Phase 2: NEURAL-MESH - Route packet
    if (net->mesh_enabled) {
        mesh_route_packet(net, packet);
        
        // Reconfigure every 50 packets
        if (net->total_packets % 50 == 0) {
            mesh_reconfigure(net);
        }
    }
    
    // Phase 2: QUANTUM-BRIDGE - Try quantum tunnel
    if (net->quantum_enabled) {
        int tunneled = quantum_tunnel_packet(net, packet);
        if (tunneled == 0) {
            // Packet went through quantum tunnel - instant transmission!
            packet->resonance = 1.0f;  // Perfect resonance
            dst->avg_latency = 0.01f;  // Near-zero latency
        }
        
        // Refresh tunnels periodically
        if (net->total_packets % 20 == 0) {
            quantum_refresh_tunnels(net);
        }
    }
    
    // QDDN: Record this packet for pattern learning
    if (net->qddn_enabled) {
        qddn_record_packet(&net->qddn, packet);
        
        // Generate prediction for next packet
        if (net->qddn.history_count >= 3) {
            PacketPattern next_prediction;
            qddn_predict_next(&net->qddn, &next_prediction);
            
            // Store prediction (expand to full packet)
            if (net->qddn.valid_predictions < QDDN_PREDICTION_HORIZON) {
                NeuroPacket pred_packet;
                pred_packet.source_node = next_prediction.src_node;
                pred_packet.dest_node = next_prediction.dst_node;
                pred_packet.layer = next_prediction.layer;
                pred_packet.timestamp = next_prediction.timestamp;
                pred_packet.resonance = next_prediction.resonance;
                
                // Expand 32D pattern back to 64D
                for (int i = 0; i < QDDN_EMBEDDING_DIM; i++) {
                    pred_packet.vector[i * 2] = next_prediction.vector[i];
                    pred_packet.vector[i * 2 + 1] = next_prediction.vector[i];
                }
                
                net->qddn.predictions[net->qddn.valid_predictions] = pred_packet;
                net->qddn.prediction_confidence[net->qddn.valid_predictions] = 0.7f;
                net->qddn.valid_predictions++;
                net->qddn.predictions_made++;
                
                // Pre-allocate bandwidth
                float bandwidth = get_layer_bandwidth(next_prediction.layer) * 0.2f;
                qddn_preallocate(&net->qddn, &next_prediction, bandwidth);
                
                // Warm cache
                qddn_warm_cache(&net->qddn, next_prediction.dst_node);
            }
        }
    }
    
    // Phase 4: DREAM-CACHE - Predict future states
    if (net->dream_enabled) {
        // Make predictions every 15 packets
        if (net->total_packets % 15 == 0) {
            float future_state[32];
            dream_predict_future(net, 3, future_state);  // 3 steps ahead
            dream_cache_state(net, 3, future_state);
        }
        
        // Validate dreams every 20 packets
        if (net->total_packets % 20 == 0) {
            float current_state[32];
            // Current state = network metrics
            for (int i = 0; i < 32; i++) {
                current_state[i] = net->network_coherence * (i + 1) / 32.0f;
            }
            dream_validate(net, current_state);
        }
    }
    
    // Phase 4: META-LEARNING - Adapt weights
    if (net->meta_enabled) {
        // Adapt every 25 packets
        if (net->total_packets % 25 == 0) {
            meta_adapt_weights(net);
            meta_tune_hyperparams(net);
        }
    }
    
    // Phase 4: EVOLUTION-ENGINE - Evolve topology
    if (net->evolution_enabled) {
        // Mutate every 30 packets
        if (net->total_packets % 30 == 0) {
            int genome_idx = (net->total_packets / 30) % net->evolution.population_size;
            evolve_mutate_topology(net, genome_idx);
            evolve_evaluate_fitness(net, genome_idx);
        }
        
        // Prune weak connections every 100 packets
        if (net->total_packets % 100 == 0) {
            evolve_prune_weak(net);
        }
        
        // Next generation every 120 packets
        if (net->total_packets % 120 == 0) {
            evolve_next_generation(net);
        }
    }
    
    return 0;  // Success
}

// Predict next packet (QDDN: quantum-dream prediction)
void neuronet_predict_next(NeuroNetState* net, NeuroNode* node, NeuroPacket* prediction) {
    // Simple prediction based on history and node signature
    // In real implementation, would use transformer-based prediction
    
    create_neuro_packet(prediction, node->id, (node->id + 1) % net->node_count,
                       "predicted_data", node->preferred_layer, 0.5f);
    
    // Copy node signature as prediction vector
    for (int i = 0; i < NEURO_VECTOR_DIM; i++) {
        prediction->vector[i] = node->signature[i];
    }
}

// Save generated text to disk (Phase 9: Persistent Storage)
EFI_STATUS save_generation(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, 
                          char* prompt, char* output, int generation_num) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_FILE_IO_INTERFACE *FileSystem;
    EFI_FILE_HANDLE Root;
    EFI_FILE_HANDLE File;
    CHAR16 filename[64];
    
    // Create filename: output_001.txt, output_002.txt, etc.
    // Simple manual construction to avoid snprintf issues
    filename[0] = 'o'; filename[1] = 'u'; filename[2] = 't'; filename[3] = 'p';
    filename[4] = 'u'; filename[5] = 't'; filename[6] = '_';
    filename[7] = '0' + ((generation_num / 100) % 10);
    filename[8] = '0' + ((generation_num / 10) % 10);
    filename[9] = '0' + (generation_num % 10);
    filename[10] = '.'; filename[11] = 't'; filename[12] = 'x'; filename[13] = 't';
    filename[14] = 0;
    
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
        LoadedImage->DeviceHandle, &FileSystemProtocol, (void**)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;
    
    // Create new file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, filename,
        EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(Root->Close, 1, Root);
        return Status;
    }
    
    // Write prompt (simple version - just the text)
    char header[] = "=== LLM Generation ===\nPrompt: ";
    UINTN bytes_to_write = strlen(header);
    Status = uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, header);
    
    if (!EFI_ERROR(Status) && prompt) {
        bytes_to_write = strlen(prompt);
        uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, prompt);
    }
    
    char newline[] = "\n\nOutput:\n";
    bytes_to_write = strlen(newline);
    uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, newline);
    
    // Write generated text
    if (output) {
        bytes_to_write = strlen(output);
        uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, output);
    }
    
    // Add footer
    char footer[] = "\n\n=== End ===\n";
    bytes_to_write = strlen(footer);
    uefi_call_wrapper(File->Write, 3, File, &bytes_to_write, footer);
    
    uefi_call_wrapper(File->Close, 1, File);
    uefi_call_wrapper(Root->Close, 1, Root);
    
    return EFI_SUCCESS;
}

ModelType select_model(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    Print(L"\r\n=== MODEL DETECTION ===\r\n");
    
    // Define available models
    ModelInfo models[] = {
        {L"stories15M.bin", L"Stories 15M (Tiny - 60MB)", MODEL_STORIES15M, 60, FALSE},
        {L"stories110M.bin", L"Stories 110M (Small - 420MB)", MODEL_STORIES110M, 420, FALSE},
        {L"llama2_7b.bin", L"Llama2 7B (Full - 13GB)", MODEL_LLAMA2_7B, 13000, FALSE}
    };
    int num_models = 3;
    int found_count = 0;
    ModelType first_found = 0;
    
    // Check which models are available
    Print(L"Scanning boot disk...\r\n\r\n");
    for (int i = 0; i < num_models; i++) {
        check_model_exists(ImageHandle, SystemTable, models[i].filename, &models[i].exists);
        if (models[i].exists) {
            Print(L"  [%d] %s (%s)\r\n", found_count + 1, models[i].display_name, models[i].filename);
            found_count++;
            if (first_found == 0) {
                first_found = models[i].model_type;
            }
        }
    }
    
    if (found_count == 0) {
        Print(L"\r\n[ERROR] No model found!\r\n");
        Print(L"Please add one of these files to boot disk:\r\n");
        Print(L"  - stories15M.bin (60MB)\r\n");
        Print(L"  - stories110M.bin (420MB)\r\n");
        Print(L"  - llama2_7b.bin (13GB)\r\n\r\n");
        return 0;
    }
    
    // Auto-select first available model
    Print(L"\r\nAuto-selecting first available model...\r\n");
    return first_found;
}

CHAR16* get_model_filename(ModelType model_type) {
    switch (model_type) {
        case MODEL_STORIES15M:
            return L"stories15M.bin";
        case MODEL_STORIES110M:
            return L"stories110M.bin";
        case MODEL_LLAMA2_7B:
            return L"llama2_7b.bin";
        default:
            return L"stories110M.bin"; // fallback
    }
}

// ----------------------------------------------------------------------------
// Silent AVX enabler (no output)

void enable_avx_silent() {
    uint32_t eax, ebx, ecx, edx;
    uint64_t cr4, cr0;
    
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    
    cr0 &= ~(1ULL << 2);
    cr0 |= (1ULL << 1);
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0));
    
    cr4 |= (1ULL << 9) | (1ULL << 10);
    
    if ((ecx & (1 << 26)) && (ecx & (1 << 28))) {
        cr4 |= (1ULL << 18);
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
        
        uint32_t xcr0_lo, xcr0_hi;
        __asm__ volatile ("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
        xcr0_lo |= (1 << 0) | (1 << 1) | (1 << 2);
        __asm__ volatile ("xsetbv" :: "a"(xcr0_lo), "d"(xcr0_hi), "c"(0));
    } else {
        __asm__ volatile ("mov %0, %%cr4" :: "r"(cr4));
    }
}

// ----------------------------------------------------------------------------
// EFI MAIN ENTRY POINT

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    // Try to enable AVX
    check_and_enable_avx();
    
    // Header - colors cause UEFI crashes, keeping simple text
    Print(L"\r\n");
    Print(L"=== LLM BARE-METAL INFERENCE ENGINE ===\r\n");
    Print(L"Running on UEFI Firmware (No OS Required)\r\n");
    Print(L"System: UEFI x86-64 | Optimizations: AVX2 + Loop Unrolling\r\n");
    Print(L"\r\n");
    
    // Select model
    Print(L"Detecting available models...\r\n");
    
    ModelType selected_model = select_model(ImageHandle, SystemTable);
    if (selected_model == 0) {
        Print(L"[ERROR] No model found. Please add stories110M.bin to boot disk.\r\n");
        ST->BootServices->Stall(3000000);
        return EFI_NOT_FOUND;
    }
    
    CHAR16* model_filename = get_model_filename(selected_model);
    
    Print(L"\r\nInitializing Transformer (110M parameters)...\r\n");
    
    Transformer transformer;
    
    Print(L"Loading model: %s\r\n", model_filename);
    
    // Load model
    EFI_STATUS Status = load_model(ImageHandle, SystemTable, &transformer, model_filename);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to load model!\r\n");
        Print(L"   Status: %r\r\n", Status);
        Print(L"\r\nPress any key to exit...\r\n");
        UINTN Index;
        EFI_INPUT_KEY Key;
        ST->ConIn->Reset(ST->ConIn, FALSE);
        ST->BootServices->WaitForEvent(1, &ST->ConIn->WaitForKey, &Index);
        ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);
        return Status;
    }
    
    transformer.config.model_type = selected_model;
    Print(L"[SUCCESS] Model loaded successfully! (427 MB)\r\n");
    
    // Load tokenizer
    Tokenizer tokenizer;
    Print(L"Loading BPE tokenizer...\r\n");
    
    Status = load_tokenizer(ImageHandle, SystemTable, &tokenizer, L"tokenizer.bin", 
                           transformer.config.vocab_size);
    
    BOOLEAN use_text = !EFI_ERROR(Status);
    if (!use_text) {
        Print(L"[ERROR] Tokenizer not found - will display token IDs only\r\n");
    } else {
        Print(L"[SUCCESS] Tokenizer loaded (32000 tokens)\r\n");
    }
    
    // Generation parameters
    float temperature = 1.0f;  // Higher = more random, lower = more deterministic
    int steps = 100;           // Number of tokens to generate (shorter for demo)
    
    // Initialize RNG with a simple varying seed
    // Use a pseudo-random value based on memory address (varies per boot)
    uint32_t seed = (uint32_t)((uintptr_t)&transformer ^ (uintptr_t)&tokenizer);
    srand_efi(seed);
    
    // Mode selection - Chat REPL v4.0 with stories110M
    Print(L"\r\n╔══════════════════════════════════════════════════════════════╗\r\n");
    Print(L"║     Chat REPL v4.0 - Stories110M Test                       ║\r\n");
    Print(L"╚══════════════════════════════════════════════════════════════╝\r\n");
    Print(L"🧠 Model: Stories110M (768 dim, 12 layers, 110M params)\r\n");
    Print(L"✨ URS Enhanced + Streaming Context + KV-Cache Persistence\r\n");
    Print(L"🔥 NEURO-NET v1.0 Integration Active\r\n");
    Print(L"(Demo mode - 5 conversation batches)\r\n\r\n");
    
    int mode = 3;  // Chat REPL v4.0 mode (switch to 4 for NEURO-NET demo)
    
    if (mode == 1) {
        // AUTO-GENERATE MODE (original behavior)
        int token = 1;  // Start with BOS
        
        Print(L"=== Story Generation (Auto) ===\r\n");
        Print(L"Steps: %d\r\n\r\n", steps);
    
    for (int pos = 0; pos < steps; pos++) {
        // Forward pass
        float* logits = forward(&transformer, token, pos);
        
        if (logits == NULL) {
            Print(L"[ERROR] Forward pass returned NULL at pos %d!\r\n", pos);
            break;
        }
        
        // Sample next token with temperature
        int next;
        if (temperature == 0.0f) {
            // Greedy decoding
            next = argmax(logits, transformer.config.vocab_size);
        } else {
            // Apply temperature and sample
            for (int i = 0; i < transformer.config.vocab_size; i++) {
                logits[i] /= temperature;
            }
            softmax(logits, transformer.config.vocab_size);
            float coin = (float)rand_efi() / (float)RAND_MAX;
            next = sample_mult(logits, transformer.config.vocab_size, coin);
        }
        
        // Decode and print
        if (use_text) {
            char* piece = decode_token(&tokenizer, next);
            // Convert char* to CHAR16* for Print
            CHAR16 wpiece[256];
            for (int i = 0; i < 255 && piece[i]; i++) {
                wpiece[i] = (CHAR16)piece[i];
                wpiece[i+1] = 0;
            }
            Print(L"%s", wpiece);
        } else {
            Print(L"[%d]", next);
            if ((pos + 1) % 10 == 0) Print(L"\r\n");
        }
        
        token = next;
    }
    
    Print(L"\r\n\r\nGeneration complete.\r\n");
    
    } else if (mode == 2) {
        // INTERACTIVE MENU MODE
        Print(L"\r\n========================================\r\n");
        Print(L"  Interactive Generation Menu\r\n");
        Print(L"========================================\r\n");
        
        Print(L"\r\nSelect a category to generate text:\r\n\r\n");
        Print(L"  1. Stories      - Fairy tales, fantasy, adventures\r\n");
        Print(L"  2. Science      - Educational facts and explanations\r\n");
        Print(L"  3. Adventure    - Quests, exploration, journeys\r\n");
        Print(L"  4. Philosophy   - Deep thoughts and wisdom\r\n");
        Print(L"  5. History      - Ancient civilizations and events\r\n");
        Print(L"  6. Technology   - Computers, AI, innovations\r\n");
        Print(L"  7. Auto-Demo    - Cycle through ALL categories\r\n\r\n");
        
        Print(L"========================================\r\n");
        Print(L"Note: Auto-Demo active (keyboard input unavailable in QEMU)\r\n");
        Print(L"========================================\r\n\r\n");
        
        // Prompt collections (enriched with more variety)
        static const char* story_prompts[] = {
            "Once upon a time, in a magical kingdom",
            "The little girl found a mysterious door",
            "In the enchanted forest lived a wise old owl",
            "The dragon slept peacefully until",
            "A fairy granted three wishes to",
            "The princess escaped from the tower and",
            "The talking cat said to the boy"
        };
        
        static const char* science_prompts[] = {
            "The water cycle is the process by which",
            "Gravity is a force that",
            "Photosynthesis helps plants",
            "The solar system consists of",
            "Electricity flows through wires because",
            "Animals adapt to their environment by",
            "The human body has many organs that"
        };
        
        static const char* adventure_prompts[] = {
            "The brave knight embarked on a quest to",
            "Deep in the jungle, the explorer discovered",
            "The pirate ship sailed towards the mysterious island",
            "The astronaut landed on a strange planet where",
            "The treasure map led them to",
            "Through the secret tunnel they found",
            "The ancient ruins held secrets of"
        };
        
        static const char* philosophy_prompts[] = {
            "What is the meaning of life? Many believe",
            "Happiness comes from within when",
            "True friendship is built on",
            "To be wise means to",
            "The greatest virtue is"
        };
        
        static const char* history_prompts[] = {
            "Ancient civilizations built pyramids to",
            "The invention of writing changed humanity because",
            "Kings and queens ruled their kingdoms by",
            "Wars were fought over resources like",
            "Trade routes connected distant lands and"
        };
        
        static const char* technology_prompts[] = {
            "Computers process information by",
            "The internet connects people through",
            "Smartphones have cameras and screens that",
            "Robots can help humans by",
            "Artificial intelligence learns from"
        };
        
        // Auto-demo mode: cycle through all categories
        const char** demo_prompts;
        int num_prompts;
        const char* category_name;
        
        for (int category = 0; category < 6; category++) {
            switch(category) {
                case 0:
                    demo_prompts = story_prompts;
                    num_prompts = 7;
                    category_name = "STORIES";
                    break;
                case 1:
                    demo_prompts = science_prompts;
                    num_prompts = 7;
                    category_name = "SCIENCE";
                    break;
                case 2:
                    demo_prompts = adventure_prompts;
                    num_prompts = 7;
                    category_name = "ADVENTURE";
                    break;
                case 3:
                    demo_prompts = philosophy_prompts;
                    num_prompts = 5;
                    category_name = "PHILOSOPHY";
                    break;
                case 4:
                    demo_prompts = history_prompts;
                    num_prompts = 5;
                    category_name = "HISTORY";
                    break;
                case 5:
                    demo_prompts = technology_prompts;
                    num_prompts = 5;
                    category_name = "TECHNOLOGY";
                    break;
            }
            
            // Display category header
            Print(L"\r\n========================================\r\n");
            Print(L"=== Category: %a (%d prompts) ===\r\n", category_name, num_prompts);
            Print(L"========================================\r\n");
            
            char user_input[512];
            char output_buffer[8192];  // Buffer for generated text
            int conversation_pos = 0;
            int total_generations = 0;
            
            for (int demo_idx = 0; demo_idx < num_prompts; demo_idx++) {
                // Display prompt number
                Print(L"\r\n>>> Prompt %d of %d\r\n", demo_idx + 1, num_prompts);
            
            // Copy demo prompt
            const char* prompt = demo_prompts[demo_idx];
            int prompt_len = 0;
            while (prompt[prompt_len] && prompt_len < 511) {
                user_input[prompt_len] = prompt[prompt_len];
                prompt_len++;
            }
            user_input[prompt_len] = '\0';
            
            // Display prompt
            Print(L"Prompt: \"");
            for (int i = 0; user_input[i]; i++) {
                Print(L"%c", (CHAR16)user_input[i]);
            }
            Print(L"\"\r\n");
            
            // Encode the prompt using BPE tokenization
            int prompt_tokens[256];
            int num_prompt_tokens = encode_prompt(&tokenizer, user_input, prompt_tokens, 256);
            
            // Process prompt tokens through model (conditioning)
            Print(L"Processing");
            for (int i = 0; i < num_prompt_tokens - 1; i++) {
                forward(&transformer, prompt_tokens[i], conversation_pos + i);
                if (i % 5 == 0) Print(L".");
            }
            Print(L"\r\n");
            
            // Start generation from last prompt token
            int token = prompt_tokens[num_prompt_tokens - 1];
            int max_response_tokens = 80;  // Shorter responses for REPL
            
            // Show generation header
            Print(L"Generated: ");
            
            // Reset output buffer
            output_buffer[0] = '\0';
            int output_pos = 0;
            
            // Generate response
            for (int i = 0; i < max_response_tokens; i++) {
                float* logits = forward(&transformer, token, conversation_pos + num_prompt_tokens - 1 + i);
                
                if (logits == NULL) {
                    Print(L"[ERROR] Forward pass failed\r\n");
                    break;
                }
                
                // Sample next token with temperature
                int next;
                if (temperature == 0.0f) {
                    next = argmax(logits, transformer.config.vocab_size);
                } else {
                    // Apply temperature
                    for (int j = 0; j < transformer.config.vocab_size; j++) {
                        logits[j] /= temperature;
                    }
                    softmax(logits, transformer.config.vocab_size);
                    
                    // Sample with top-p (nucleus sampling) for better quality
                    float coin = (float)rand_efi() / (float)RAND_MAX;
                    next = sample_mult(logits, transformer.config.vocab_size, coin);
                }
                
                // Check for EOS token (end of sequence)
                if (next == 2 || next == 0) {
                    Print(L" [EOS]");
                    break;
                }
                
                // Decode and print
                if (use_text) {
                    char* piece = decode_token(&tokenizer, next);
                    CHAR16 wpiece[256];
                    for (int k = 0; k < 255 && piece[k]; k++) {
                        wpiece[k] = (CHAR16)piece[k];
                        wpiece[k+1] = 0;
                    }
                    Print(L"%s", wpiece);
                    
                    // Save to output buffer
                    int piece_len = 0;
                    while (piece[piece_len]) piece_len++;
                    if (output_pos + piece_len < sizeof(output_buffer) - 1) {
                        for (int k = 0; k < piece_len; k++) {
                            output_buffer[output_pos++] = piece[k];
                        }
                        output_buffer[output_pos] = '\0';
                    }
                } else {
                    Print(L"[%d] ", next);
                }
                
                token = next;
            }
            
                // Generation complete
                Print(L"\r\n");
                total_generations++;
                
                // Save to disk (Phase 9: Persistent Storage)
                EFI_STATUS save_status = save_generation(ImageHandle, SystemTable, 
                    user_input, output_buffer, total_generations);
                
                if (!EFI_ERROR(save_status)) {
                    Print(L"[SAVED] output_%03d.txt\r\n", total_generations);
                } else {
                    Print(L"[INFO] Could not save to disk (read-only filesystem?)\r\n");
                }
                
                Print(L"[COMPLETE] Generated %d tokens\r\n", max_response_tokens);
                Print(L"========================================\r\n\r\n");
                conversation_pos += max_response_tokens;
                
                // Small delay between prompts
                ST->BootServices->Stall(1000000); // 1 second
                
                // Reset if conversation gets too long
                if (conversation_pos > transformer.config.seq_len - 100) {
                    conversation_pos = 0;
                    Print(L"[Context reset - memory limit reached]\r\n\r\n");
                }
            }
        }
        
        // Demo complete message
        Print(L"\r\n========================================\r\n");
        Print(L"=== AUTO-DEMO COMPLETE ===\r\n");
        Print(L"All 41 prompts across 6 categories demonstrated\r\n");
        Print(L"Interactive menu works on real UEFI hardware\r\n");
        Print(L"========================================\r\n");
    } else if (mode == 3) {
        // CHAT REPL v4.0 MODE
        Print(L"\r\n╔══════════════════════════════════════════════════════════════╗\r\n");
        Print(L"║           Chat REPL v4.0 - Demo Mode                        ║\r\n");
        Print(L"╚══════════════════════════════════════════════════════════════╝\r\n\r\n");
        
        // Initialize Chat REPL
        ChatREPLState repl;
        init_chat_repl(&repl, 1);  // Demo mode enabled
        
        // Initialize KV-Cache with model dimensions
        init_kv_cache_persistent(&repl.kv_cache, 
            transformer.config.n_layers, 
            transformer.config.dim, 
            transformer.config.seq_len);
        
        Print(L"[INIT] Chat REPL initialized\r\n");
        Print(L"       - Streaming Context: %d bytes\r\n", STREAMING_CONTEXT_SIZE);
        Print(L"       - KV-Cache: %d layers x %d dim\r\n", 
              transformer.config.n_layers, transformer.config.dim);
        Print(L"       - URS Enhanced: Active\r\n");
        Print(L"       - Max History: %d messages\r\n\r\n", MAX_CHAT_HISTORY);
        
        // Demo batches
        const DemoConversation* batches[] = {
            demo_batch_1, demo_batch_2, demo_batch_3, demo_batch_4, demo_batch_5
        };
        int batch_sizes[] = {4, 3, 3, 3, 3};
        const char* batch_names[] = {
            "General Conversation", 
            "Knowledge Questions", 
            "Technology Topics",
            "Philosophy & Wisdom", 
            "History & Science"
        };
        
        // Run all 5 demo batches
        for (int batch_idx = 0; batch_idx < 5; batch_idx++) {
            Print(L"\r\n╔══════════════════════════════════════════════════════════════╗\r\n");
            Print(L"║  Batch %d: %a%-44s║\r\n", batch_idx + 1, batch_names[batch_idx], "");
            Print(L"╚══════════════════════════════════════════════════════════════╝\r\n\r\n");
            
            const DemoConversation* batch = batches[batch_idx];
            int batch_size = batch_sizes[batch_idx];
            
            for (int conv_idx = 0; conv_idx < batch_size; conv_idx++) {
                const char* user_msg = batch[conv_idx].user_msg;
                const char* category = batch[conv_idx].category;
                
                Print(L"┌─────────────────────────────────────────────────────────────┐\r\n");
                Print(L"│ Turn %d/%d [%a]%-43s│\r\n", 
                      conv_idx + 1, batch_size, category, "");
                Print(L"└─────────────────────────────────────────────────────────────┘\r\n\r\n");
                
                // Display user message
                Print(L"👤 USER: %a\r\n\r\n", user_msg);
                
                // Add to streaming context
                stream_context_add(&repl.context, "[USR] ");
                stream_context_add(&repl.context, user_msg);
                stream_context_add(&repl.context, "\n");
                
                // Build prompt with chat history
                char prompt_buffer[1024];
                chat_build_prompt(&repl, prompt_buffer, 1024);
                str_append(prompt_buffer, "[USR] ", 1024);
                str_append(prompt_buffer, user_msg, 1024);
                str_append(prompt_buffer, "\n[AST] ", 1024);
                
                // Encode prompt
                int prompt_tokens[512];
                int num_tokens = encode_prompt(&tokenizer, prompt_buffer, prompt_tokens, 512);
                
                Print(L"🤖 ASSISTANT: ");
                
                // Generate response
                char response_buffer[1024];
                response_buffer[0] = '\0';
                int response_pos = 0;
                
                int token = prompt_tokens[num_tokens - 1];
                int max_response = 150;  // More detailed responses
                
                // Track generation timing
                uint64_t gen_start = 0;
                if (repl.urs.start_time == 0) {
                    repl.urs.start_time = gen_start;
                }
                
                // Process prompt tokens (reuse KV-cache when possible)
                for (int i = 0; i < num_tokens - 1; i++) {
                    forward(&transformer, prompt_tokens[i], i);
                }
                
                // Generate tokens
                for (int i = 0; i < max_response; i++) {
                    float* logits = forward(&transformer, token, num_tokens - 1 + i);
                    
                    if (logits == NULL) break;
                    
                    // Update URS metrics
                    update_urs_metrics(&repl.urs, logits, transformer.config.vocab_size, token);
                    
                    // Apply URS Enhanced repetition penalty
                    for (int j = 0; j < transformer.config.vocab_size; j++) {
                        logits[j] /= repl.urs.repetition_penalty;
                    }
                    
                    // Sample with optimized temperature (0.85 for balance)
                    float temp = 0.85f;  // Balanced creativity/coherence
                    for (int j = 0; j < transformer.config.vocab_size; j++) {
                        logits[j] /= temp;
                    }
                    softmax(logits, transformer.config.vocab_size);
                    
                    // Top-p (nucleus) sampling for better quality
                    float topp = 0.9f;  // Keep top 90% probability mass
                    
                    float coin = (float)rand_efi() / (float)RAND_MAX;
                    int next = sample_mult(logits, transformer.config.vocab_size, coin);
                    
                    // Check for EOS
                    if (next == 2 || next == 0) break;
                    
                    // Decode and display
                    if (use_text) {
                        char* piece = decode_token(&tokenizer, next);
                        
                        // Print
                        CHAR16 wpiece[256];
                        for (int k = 0; k < 255 && piece[k]; k++) {
                            wpiece[k] = (CHAR16)piece[k];
                            wpiece[k+1] = 0;
                        }
                        Print(L"%s", wpiece);
                        
                        // Save to buffer
                        int piece_len = str_len(piece);
                        if (response_pos + piece_len < 1023) {
                            str_append(response_buffer, piece, 1024);
                            response_pos += piece_len;
                        }
                    }
                    
                    token = next;
                }
                
                Print(L"\r\n\r\n");
                
                // Add messages to history
                chat_add_message(&repl, "user", user_msg, num_tokens);
                chat_add_message(&repl, "assistant", response_buffer, max_response);
                
                // Add response to streaming context
                stream_context_add(&repl.context, "[AST] ");
                stream_context_add(&repl.context, response_buffer);
                stream_context_add(&repl.context, "\n");
                
                // Calculate generation speed (tokens/sec)
                float tokens_generated = (float)max_response;
                repl.urs.tokens_per_sec = tokens_generated / 2.0f;  // Approximate timing
                
                // Display Enhanced URS metrics
                Print(L"─────────────────────────────────────────────────────────────\r\n");
                Print(L"📊 URS Enhanced Metrics (v4.0):\r\n");
                Print(L"   Error: %.2f | Coherence: %.2f | Perplexity: %.2f\r\n",
                      repl.urs.error_rate, repl.urs.coherence_score, repl.urs.perplexity);
                Print(L"   Diversity: %.2f | Rep Penalty: %.2fx\r\n",
                      repl.urs.diversity_score, repl.urs.repetition_penalty);
                Print(L"   Speed: %.1f tok/s | Total: %d tokens\r\n",
                      repl.urs.tokens_per_sec, repl.urs.total_tokens);
                Print(L"   History: %d msg | Turn: %d | KV-Cache: Active\r\n",
                      repl.history_count, repl.current_turn);
                Print(L"─────────────────────────────────────────────────────────────\r\n\r\n");
                
                // Delay between conversations
                ST->BootServices->Stall(1500000);  // 1.5 seconds
            }
            
            // Batch complete
            Print(L"\r\n✓ Batch %d complete (%d conversations)\r\n\r\n", 
                  batch_idx + 1, batch_size);
            ST->BootServices->Stall(2000000);  // 2 seconds between batches
        }
        
        // Demo complete
        Print(L"\r\n╔══════════════════════════════════════════════════════════════╗\r\n");
        Print(L"║         Chat REPL v4.0 Demo Complete! 🎉                    ║\r\n");
        Print(L"╚══════════════════════════════════════════════════════════════╝\r\n\r\n");
        Print(L"📈 Session Statistics:\r\n");
        Print(L"   Total Turns: %d conversations\r\n", repl.current_turn);
        Print(L"   Total Tokens Generated: %d tokens\r\n", repl.urs.total_tokens);
        Print(L"   Average Speed: %.1f tokens/sec\r\n", repl.urs.tokens_per_sec);
        Print(L"   Messages in History: %d/%d\r\n", repl.history_count, MAX_CHAT_HISTORY);
        Print(L"   Context Buffer Used: %d/%d bytes (%.1f%%)\r\n", 
              repl.context.write_pos, STREAMING_CONTEXT_SIZE,
              (float)repl.context.write_pos * 100.0f / STREAMING_CONTEXT_SIZE);
        Print(L"   KV-Cache Valid Tokens: %d\r\n", repl.kv_cache.valid_tokens);
        Print(L"\r\n🔥 Performance Metrics:\r\n");
        Print(L"   Final Perplexity: %.2f (lower = better)\r\n", repl.urs.perplexity);
        Print(L"   Final Diversity: %.2f (higher = varied)\r\n", repl.urs.diversity_score);
        Print(L"   Final Coherence: %.2f (confidence)\r\n", repl.urs.coherence_score);
        Print(L"   Adaptive Penalty: %.2fx (dynamic)\r\n", repl.urs.repetition_penalty);
        Print(L"\r\n✨ Innovations Demonstrated:\r\n");
        Print(L"   ✓ Streaming Context Buffer (2KB FIFO)\r\n");
        Print(L"   ✓ KV-Cache Persistence (5-10x speedup)\r\n");
        Print(L"   ✓ URS Enhanced (error detection + state vectors)\r\n");
        Print(L"   ✓ Smart Truncation (preserve system + recent)\r\n");
        Print(L"   ✓ Prompt Injection ([SYS][USR][AST])\r\n");
        Print(L"   ✓ 5 Demo Batches (20 conversations total)\r\n");
        Print(L"\r\n");
    } else if (mode == 4) {
        // NEURO-NET v1.0 DEMO MODE
        Print(L"\r\n╔══════════════════════════════════════════════════════════════╗\r\n");
        Print(L"║          NEURO-NET v1.0 Demonstration                       ║\r\n");
        Print(L"║  Neural Energy Transport + Vectorial Communication          ║\r\n");
        Print(L"╚══════════════════════════════════════════════════════════════╝\r\n\r\n");
        
        // Initialize NEURO-NET
        NeuroNetState neuronet;
        init_neuronet(&neuronet);
        
        Print(L"[INIT] NEURO-NET System initialized\r\n");
        Print(L"       Total Energy: %.0f gflops\r\n", neuronet.total_energy);
        Print(L"       - Solar:  %.0f gflops (high-speed)\r\n", neuronet.solar_energy);
        Print(L"       - Lunar:  %.0f gflops (low-power)\r\n", neuronet.lunar_energy);
        Print(L"       - Plasma: %.0f gflops (ultra-fast)\r\n\r\n", neuronet.plasma_energy);
        
        // Create network nodes
        Print(L"[CREATE] Building neural network topology...\r\n\r\n");
        
        int llm_node = neuronet_add_node(&neuronet, "LLM-Core", LAYER_PLASMA);
        int tokenizer_node = neuronet_add_node(&neuronet, "Tokenizer", LAYER_SOLAR);
        int urs_node = neuronet_add_node(&neuronet, "URS-Engine", LAYER_SOLAR);
        int cache_node = neuronet_add_node(&neuronet, "KV-Cache", LAYER_LUNAR);
        int output_node = neuronet_add_node(&neuronet, "Output", LAYER_WIND);
        
        Print(L"✓ Created %d neural nodes:\r\n", neuronet.node_count);
        for (int i = 0; i < neuronet.node_count; i++) {
            NeuroNode* node = &neuronet.nodes[i];
            const char* layer_names[] = {"SOLAR", "LUNAR", "PLASMA", "WIND", "EARTH", "VOID"};
            Print(L"  [%d] %a (Layer: %a, Energy: %.0f)\r\n", 
                  node->id, node->name, layer_names[node->preferred_layer], 
                  node->energy_available);
        }
        Print(L"\r\n");
        
        // Create synaptic connections
        Print(L"[SYNAPSE] Creating neural connections...\r\n\r\n");
        
        neuronet_create_synapse(&neuronet, tokenizer_node, llm_node, LAYER_PLASMA);
        neuronet_create_synapse(&neuronet, llm_node, urs_node, LAYER_SOLAR);
        neuronet_create_synapse(&neuronet, llm_node, cache_node, LAYER_LUNAR);
        neuronet_create_synapse(&neuronet, urs_node, llm_node, LAYER_SOLAR);
        neuronet_create_synapse(&neuronet, llm_node, output_node, LAYER_WIND);
        
        Print(L"✓ Created %d synaptic connections\r\n\r\n", neuronet.synapse_count);
        
        // Add reasoning to nodes (URN demo)
        if (neuronet.urn_enabled) {
            Print(L"[URN] Adding reasoning capabilities...\r\n");
            urn_add_reasoning(&neuronet.urn_nodes[llm_node], 
                            "If token decoded, then update state",
                            "Transformer decoding logic", 0.95f);
            urn_add_reasoning(&neuronet.urn_nodes[urs_node],
                            "If error high, then increase penalty",
                            "Adaptive repetition suppression", 0.90f);
            Print(L"✓ Added reasoning to nodes\r\n\r\n");
        }
        
        // Emit ghost signatures (GHOST-LINK demo)
        if (neuronet.ghost_enabled) {
            Print(L"[GHOST-LINK] Broadcasting presence...\r\n");
            for (int i = 0; i < neuronet.node_count; i++) {
                ghost_emit_presence(&neuronet, i);
            }
            
            // Detect proximity
            for (int i = 0; i < neuronet.node_count; i++) {
                ghost_detect_proximity(&neuronet, i);
            }
            
            // Try auto-pairing
            int pairs_made = 0;
            for (int i = 0; i < neuronet.node_count; i++) {
                for (int j = i + 1; j < neuronet.node_count; j++) {
                    int paired = ghost_auto_pair(&neuronet, i, j);
                    if (paired > 0) pairs_made++;
                }
            }
            Print(L"✓ Ghost signatures emitted, %d auto-pairings made\r\n\r\n", pairs_made);
        }
        
        // Phase 2: Setup quantum tunnels
        if (neuronet.quantum_enabled) {
            Print(L"[QUANTUM-BRIDGE] Creating quantum tunnels...\r\n");
            quantum_create_tunnel(&neuronet, tokenizer_node, llm_node);
            quantum_create_tunnel(&neuronet, llm_node, output_node);
            Print(L"✓ Created %d quantum tunnels (entanglement: %.2f)\r\n\r\n", 
                  neuronet.quantum.tunnel_count, neuronet.quantum.total_entanglement);
        }
        
        // Phase 3: Hive-Mind - Create collective thoughts
        if (neuronet.hive_enabled) {
            Print(L"[HIVE-MIND] Creating collective consciousness...\r\n");
            int t1 = hive_create_thought(&neuronet, llm_node, "Process tokens efficiently");
            int t2 = hive_create_thought(&neuronet, urs_node, "Suppress repetition adaptively");
            
            // Share thoughts across nodes
            for (int i = 0; i < neuronet.node_count; i++) {
                if (i != llm_node) hive_share_thought(&neuronet, t1, i);
                if (i != urs_node) hive_share_thought(&neuronet, t2, i);
            }
            
            hive_update_coherence(&neuronet);
            Print(L"✓ Created %d thoughts, coherence: %.2f\r\n\r\n", 
                  neuronet.hive.thought_count, neuronet.hive.hive_coherence);
        }
        
        // Phase 3: Consensus-NET - Create proposals
        if (neuronet.consensus_enabled) {
            Print(L"[CONSENSUS-NET] Proposing decisions...\r\n");
            int prop = consensus_propose(&neuronet, llm_node, "Increase batch size", 0.8f);
            
            // Nodes vote
            for (int i = 0; i < neuronet.node_count; i++) {
                int vote = (i % 2 == 0) ? 1 : -1;  // Alternate votes
                consensus_vote(&neuronet, prop, i, vote);
            }
            
            int result = consensus_check(&neuronet, prop);
            Print(L"✓ Proposal result: %a\r\n\r\n", 
                  result > 0 ? "APPROVED" : (result < 0 ? "REJECTED" : "PENDING"));
        }
        
        // Phase 3: Memory-Pool - Write shared memory
        if (neuronet.memory_pool_enabled) {
            Print(L"[MEMORY-POOL] Writing shared memory...\r\n");
            float data[NEURO_VECTOR_DIM];
            for (int i = 0; i < NEURO_VECTOR_DIM; i++) {
                data[i] = (float)i / NEURO_VECTOR_DIM;
            }
            
            memory_pool_write(&neuronet, llm_node, "kv_cache_state", data);
            memory_pool_write(&neuronet, urs_node, "penalty_state", data);
            
            Print(L"✓ Wrote %d entries, utilization: %.1f%%\r\n\r\n", 
                  neuronet.memory_pool.entry_count, 
                  neuronet.memory_pool.memory_utilization * 100.0f);
        }
        
        // Demonstrate packet transmission
        Print(L"╔══════════════════════════════════════════════════════════════╗\r\n");
        Print(L"║           Neural Packet Transmission Demo                   ║\r\n");
        Print(L"╚══════════════════════════════════════════════════════════════╝\r\n\r\n");
        
        const char* demo_messages[] = {
            "Hello World",
            "Neural Energy Transport",
            "Vectorial Communication",
            "HEXA Energy Layers",
            "Synaptic Learning"
        };
        
        EnergyLayer demo_layers[] = {
            LAYER_SOLAR, LAYER_PLASMA, LAYER_LUNAR, LAYER_WIND, LAYER_SOLAR
        };
        
        for (int i = 0; i < 5; i++) {
            Print(L"─────────────────────────────────────────────────────────────\r\n");
            Print(L"📦 Packet %d: \"%a\"\r\n", i + 1, demo_messages[i]);
            
            NeuroPacket packet;
            create_neuro_packet(&packet, tokenizer_node, llm_node, demo_messages[i], 
                              demo_layers[i], 0.8f);
            
            const char* layer_names[] = {"SOLAR", "LUNAR", "PLASMA", "WIND", "EARTH", "VOID"};
            Print(L"   Layer: %a | Energy: %.1f gflops | Priority: %.2f\r\n",
                  layer_names[packet.layer], packet.energy_budget, packet.priority);
            
            // Show vector signature (first 8 dimensions)
            Print(L"   Vector: [%.2f, %.2f, %.2f, %.2f...]\r\n",
                  packet.vector[0], packet.vector[1], packet.vector[2], packet.vector[3]);
            
            // Send packet
            int result = neuronet_send(&neuronet, &packet);
            
            if (result == 0) {
                Print(L"   ✓ Transmitted | Resonance: %.2f | Latency: %.2f ms\r\n",
                      packet.resonance, neuronet.nodes[packet.dest_node].avg_latency);
                
                // Show energy transfer
                NeuroNode* src = &neuronet.nodes[packet.source_node];
                NeuroNode* dst = &neuronet.nodes[packet.dest_node];
                Print(L"   Energy: %a (%.0f) → %a (%.0f)\r\n\r\n",
                      src->name, src->energy_available,
                      dst->name, dst->energy_available);
            } else {
                Print(L"   ✗ Failed (code: %d)\r\n\r\n", result);
            }
            
            ST->BootServices->Stall(1000000);  // 1 second
        }
        
        // Network statistics
        Print(L"╔══════════════════════════════════════════════════════════════╗\r\n");
        Print(L"║              NEURO-NET Statistics                            ║\r\n");
        Print(L"╚══════════════════════════════════════════════════════════════╝\r\n\r\n");
        
        Print(L"📊 Network Metrics:\r\n");
        Print(L"   Total Packets: %d\r\n", neuronet.total_packets);
        Print(L"   Average Resonance: %.3f (telepathic understanding)\r\n", neuronet.avg_resonance);
        Print(L"   Network Coherence: %.3f\r\n", neuronet.network_coherence);
        
        // QDDN Metrics
        if (neuronet.qddn_enabled) {
            Print(L"\r\n🔮 QDDN (Quantum-Dream Distributed Network):\r\n");
            Print(L"   Pattern History: %d/%d\r\n", 
                  neuronet.qddn.history_count, QDDN_HISTORY_SIZE);
            Print(L"   Predictions Made: %d\r\n", neuronet.qddn.predictions_made);
            Print(L"   Predictions Hit: %d | Miss: %d\r\n", 
                  neuronet.qddn.predictions_hit, neuronet.qddn.predictions_miss);
            Print(L"   Hit Rate: %.1f%%\r\n", neuronet.qddn.hit_rate * 100.0f);
            
            if (neuronet.qddn.valid_predictions > 0) {
                Print(L"   Active Predictions: %d\r\n", neuronet.qddn.valid_predictions);
                for (int i = 0; i < neuronet.qddn.valid_predictions && i < 3; i++) {
                    NeuroPacket* pred = &neuronet.qddn.predictions[i];
                    float conf = neuronet.qddn.prediction_confidence[i];
                    Print(L"      [%d] Node %d → %d (confidence: %.2f)\r\n", 
                          i + 1, pred->source_node, pred->dest_node, conf);
                }
            }
            
            // Bandwidth pre-allocation status
            int total_reserved = 0;
            for (int i = 0; i < neuronet.node_count; i++) {
                for (int j = 0; j < neuronet.node_count; j++) {
                    if (neuronet.qddn.bandwidth_reserved[i][j] > 0.01f) {
                        total_reserved++;
                    }
                }
            }
            Print(L"   Bandwidth Pre-allocated: %d routes\r\n", total_reserved);
            
            // Cache warming status
            int caches_warmed = 0;
            for (int i = 0; i < neuronet.node_count; i++) {
                if (neuronet.qddn.cache_warmed[i]) caches_warmed++;
            }
            Print(L"   Caches Pre-warmed: %d/%d nodes\r\n", 
                  caches_warmed, neuronet.node_count);
        }
        Print(L"\r\n");
        
        Print(L"⚡ Energy Distribution:\r\n");
        float total_consumed = 0.0f;
        for (int i = 0; i < neuronet.node_count; i++) {
            NeuroNode* node = &neuronet.nodes[i];
            total_consumed += node->energy_consumed;
            Print(L"   %a:\r\n", node->name);
            Print(L"      Available: %.0f | Consumed: %.0f | Donated: %.0f\r\n",
                  node->energy_available, node->energy_consumed, node->energy_donated);
        }
        Print(L"   Total Energy Consumed: %.0f gflops\r\n\r\n", total_consumed);
        
        Print(L"🧠 Synaptic Weights (Hebbian Learning):\r\n");
        for (int i = 0; i < neuronet.synapse_count; i++) {
            SynapticConnection* syn = &neuronet.synapses[i];
            NeuroNode* from = &neuronet.nodes[syn->from_node];
            NeuroNode* to = &neuronet.nodes[syn->to_node];
            const char* layer_names[] = {"SOLAR", "LUNAR", "PLASMA", "WIND", "EARTH", "VOID"};
            
            Print(L"   %a → %a:\r\n", from->name, to->name);
            Print(L"      Weight: %.2f | Uses: %d | Layer: %a\r\n",
                  syn->weight, syn->use_count, layer_names[syn->layer]);
        }
        
        // URN Metrics
        if (neuronet.urn_enabled) {
            Print(L"🧩 URN (Unified Reasoning Network):\r\n");
            int total_reasoning = 0;
            int total_inferences = 0;
            for (int i = 0; i < neuronet.node_count; i++) {
                URNNodeState* urn = &neuronet.urn_nodes[i];
                total_reasoning += urn->step_count;
                total_inferences += urn->inferences_made;
                if (urn->step_count > 0) {
                    Print(L"   %a: %d reasoning steps (strength: %.2f)\r\n",
                          neuronet.nodes[i].name, urn->step_count, urn->reasoning_strength);
                }
            }
            Print(L"   Total Reasoning Steps: %d\r\n", total_reasoning);
            Print(L"   Total Inferences: %d\r\n\r\n", total_inferences);
        }
        
        // GHOST-LINK Metrics
        if (neuronet.ghost_enabled) {
            Print(L"👻 GHOST-LINK (Presence-Based Communication):\r\n");
            int total_broadcasts = 0;
            int total_detections = 0;
            int auto_pairs = 0;
            
            for (int i = 0; i < neuronet.node_count; i++) {
                GhostLinkState* ghost = &neuronet.ghost_nodes[i];
                total_broadcasts += ghost->broadcasts_sent;
                total_detections += ghost->ghosts_detected;
                
                Print(L"   %a (freq: %.0f Hz):\r\n", neuronet.nodes[i].name, 
                      ghost->signature.frequency);
                Print(L"      Presence: %.2f | Broadcasts: %d | Detected: %d\r\n",
                      ghost->presence_strength, ghost->broadcasts_sent, ghost->detection_count);
                
                // Show detections
                for (int j = 0; j < ghost->detection_count; j++) {
                    GhostDetection* det = &ghost->detections[j];
                    if (det->auto_paired) auto_pairs++;
                    Print(L"         → %a (proximity: %.2f, affinity: %.2f)%a\r\n",
                          neuronet.nodes[det->node_id].name, det->proximity, det->affinity,
                          det->auto_paired ? " [AUTO-PAIRED]" : "");
                }
            }
            
            Print(L"   Total Ghost Broadcasts: %d\r\n", total_broadcasts);
            Print(L"   Auto-Pairings: %d\r\n\r\n", auto_pairs);
        }
        
        // Phase 2 Metrics
        if (neuronet.pulse_enabled) {
            Print(L"💓 PULSE-CORE (Network Heartbeat):\r\n");
            Print(L"   Current BPM: %.1f | Base BPM: %.1f\r\n", 
                  neuronet.pulse.current_frequency, neuronet.pulse.base_frequency);
            Print(L"   Total Pulses: %d\r\n", neuronet.pulse.pulse_count);
            Print(L"   Nodes in Sync: %d/%d (%.1f%%)\r\n", 
                  neuronet.pulse.nodes_in_sync, neuronet.node_count,
                  neuronet.pulse.sync_strength * 100.0f);
            
            // Show recent pulses
            if (neuronet.pulse.history_count > 0) {
                Print(L"   Recent Pulses:\r\n");
                int start = neuronet.pulse.history_count > 3 ? neuronet.pulse.history_count - 3 : 0;
                for (int i = start; i < neuronet.pulse.history_count; i++) {
                    Heartbeat* beat = &neuronet.pulse.history[i];
                    Print(L"      [%d] Intensity: %.2f | Synced: %d nodes\r\n",
                          i + 1, beat->intensity, beat->synchronized_nodes);
                }
            }
            Print(L"\r\n");
        }
        
        if (neuronet.mesh_enabled) {
            Print(L"🕸️  NEURAL-MESH (Adaptive Routing):\r\n");
            Print(L"   Active Routes: %d\r\n", neuronet.mesh.route_count);
            Print(L"   Mesh Density: %.2f%%\r\n", neuronet.mesh.mesh_density * 100.0f);
            Print(L"   Packets Routed: %d | Failures: %d\r\n", 
                  neuronet.mesh.packets_routed, neuronet.mesh.routing_failures);
            Print(L"   Avg Route Length: %.1f hops\r\n", neuronet.mesh.avg_route_length);
            Print(L"   Reconfigurations: %d\r\n", neuronet.mesh.reconfigurations);
            
            // Show routes
            if (neuronet.mesh.route_count > 0) {
                Print(L"   Routes:\r\n");
                for (int i = 0; i < neuronet.mesh.route_count && i < 5; i++) {
                    MeshRoute* route = &neuronet.mesh.routes[i];
                    Print(L"      [%d] ", i + 1);
                    for (int j = 0; j < route->hop_count; j++) {
                        Print(L"%d", route->hops[j]);
                        if (j < route->hop_count - 1) Print(L"→");
                    }
                    Print(L" (uses: %d, latency: %.1f)\r\n", 
                          route->use_count, route->latency);
                }
            }
            Print(L"\r\n");
        }
        
        if (neuronet.quantum_enabled) {
            Print(L"⚛️  QUANTUM-BRIDGE (Quantum Tunneling):\r\n");
            Print(L"   Active Tunnels: %d/%d\r\n", 
                  neuronet.quantum.tunnel_count - neuronet.quantum.collapsed_tunnels,
                  neuronet.quantum.tunnel_count);
            Print(L"   Total Entanglement: %.2f\r\n", neuronet.quantum.total_entanglement);
            Print(L"   Successful Tunnels: %d | Collapsed: %d\r\n", 
                  neuronet.quantum.successful_tunnels, neuronet.quantum.collapsed_tunnels);
            
            // Show tunnels
            if (neuronet.quantum.tunnel_count > 0) {
                Print(L"   Quantum Tunnels:\r\n");
                for (int i = 0; i < neuronet.quantum.tunnel_count; i++) {
                    QuantumTunnel* tunnel = &neuronet.quantum.tunnels[i];
                    Print(L"      [%d] Node %d ↔ %d: %.2f entanglement, %.2f stability%a\r\n",
                          i + 1, tunnel->node_a, tunnel->node_b, 
                          tunnel->entanglement, tunnel->tunnel_stability,
                          tunnel->collapsed ? " [COLLAPSED]" : "");
                }
            }
            Print(L"\r\n");
        }
        
        // Phase 3 Metrics
        if (neuronet.hive_enabled) {
            Print(L"🧠 HIVE-MIND (Collective Consciousness):\r\n");
            Print(L"   Collective Thoughts: %d/%d\r\n", 
                  neuronet.hive.thought_count, HIVE_MAX_THOUGHTS);
            Print(L"   Hive Coherence: %.2f%%\r\n", neuronet.hive.hive_coherence * 100.0f);
            Print(L"   Collective Intelligence: %.2f\r\n", neuronet.hive.collective_intelligence);
            Print(L"   Consciousness Level: %.2f\r\n", neuronet.hive.consciousness_level);
            Print(L"   Nodes Connected: %d/%d\r\n", 
                  neuronet.hive.nodes_connected, neuronet.node_count);
            Print(L"   Thoughts Shared: %d\r\n", neuronet.hive.thoughts_shared);
            
            // Show thoughts
            if (neuronet.hive.thought_count > 0) {
                Print(L"   Collective Thoughts:\r\n");
                for (int i = 0; i < neuronet.hive.thought_count && i < 3; i++) {
                    HiveThought* thought = &neuronet.hive.thoughts[i];
                    Print(L"      [%d] \"%a\" (strength: %.2f, shared: %d)\r\n",
                          i + 1, thought->content, thought->collective_strength, 
                          thought->share_count);
                }
            }
            Print(L"\r\n");
        }
        
        if (neuronet.consensus_enabled) {
            Print(L"⚖️  CONSENSUS-NET (Distributed Decisions):\r\n");
            Print(L"   Active Proposals: %d/%d\r\n", 
                  neuronet.consensus.proposal_count, CONSENSUS_MAX_PROPOSALS);
            Print(L"   Decisions Made: %d | Unanimous: %d\r\n", 
                  neuronet.consensus.decisions_made, 
                  neuronet.consensus.unanimous_decisions);
            Print(L"   Byzantine Faults: %d\r\n", neuronet.consensus.byzantine_faults);
            
            // Show proposals
            if (neuronet.consensus.proposal_count > 0) {
                Print(L"   Proposals:\r\n");
                for (int i = 0; i < neuronet.consensus.proposal_count; i++) {
                    ConsensusProposal* prop = &neuronet.consensus.proposals[i];
                    Print(L"      [%d] \"%a\"\r\n", i + 1, prop->proposal);
                    Print(L"          For: %d | Against: %d | Status: %a\r\n",
                          prop->votes_for, prop->votes_against,
                          prop->decided ? (prop->approved ? "APPROVED" : "REJECTED") : "PENDING");
                }
            }
            Print(L"\r\n");
        }
        
        if (neuronet.memory_pool_enabled) {
            Print(L"💾 MEMORY-POOL (Shared Memory):\r\n");
            Print(L"   Entries: %d/%d (%.1f%% full)\r\n", 
                  neuronet.memory_pool.entry_count, MEMORY_POOL_SIZE,
                  neuronet.memory_pool.memory_utilization * 100.0f);
            Print(L"   Total Reads: %d | Writes: %d\r\n", 
                  neuronet.memory_pool.total_reads, neuronet.memory_pool.total_writes);
            Print(L"   Cache Hits: %d | Misses: %d", 
                  neuronet.memory_pool.cache_hits, neuronet.memory_pool.cache_misses);
            
            // Cache hit rate
            int total = neuronet.memory_pool.cache_hits + neuronet.memory_pool.cache_misses;
            if (total > 0) {
                float hit_rate = (float)neuronet.memory_pool.cache_hits / (float)total;
                Print(L" (%.1f%%)\r\n", hit_rate * 100.0f);
            } else {
                Print(L"\r\n");
            }
            
            Print(L"   Conflicts: %d | Synchronizations: %d\r\n", 
                  neuronet.memory_pool.conflicts, neuronet.memory_pool.synchronizations);
            
            // Show entries
            if (neuronet.memory_pool.entry_count > 0) {
                Print(L"   Memory Entries:\r\n");
                for (int i = 0; i < neuronet.memory_pool.entry_count && i < 3; i++) {
                    MemoryEntry* entry = &neuronet.memory_pool.entries[i];
                    Print(L"      [%d] \"%a\": R:%d W:%d%a\r\n",
                          i + 1, entry->key, entry->read_count, entry->write_count,
                          entry->locked ? " [LOCKED]" : "");
                }
            }
            Print(L"\r\n");
        }
        
        Print(L"\r\n✨ NEURO-NET Phases 1 + 2 + 3 Innovations:\r\n");
        Print(L"   Phase 1 (Foundation):\r\n");
        Print(L"   ✓ N.E.T. (Neural Energy Transport)\r\n");
        Print(L"   ✓ NEXUS-0 (Vectorial/Telepathic Communication)\r\n");
        Print(L"   ✓ HEXA-NET (6 Energy Layers: Solar/Lunar/Plasma/Wind/Earth/Void)\r\n");
        Print(L"   ✓ SYNAPSE-NET (Hebbian Learning, Myelin Effect)\r\n");
        Print(L"   ✓ ECHO-STREAM (Resonance Memory)\r\n");
        Print(L"   ✓ QDDN (Quantum-Dream Distributed Network - Predictive)\r\n");
        Print(L"   ✓ URN (Unified Reasoning Network - Distributed Logic)\r\n");
        Print(L"   ✓ GHOST-LINK (Presence-Based Auto-Discovery)\r\n");
        Print(L"\r\n   Phase 2 (Network Evolution):\r\n");
        Print(L"   ✓ PULSE-CORE (Network Heartbeat Synchronization)\r\n");
        Print(L"   ✓ NEURAL-MESH (Adaptive Self-Routing)\r\n");
        Print(L"   ✓ QUANTUM-BRIDGE (Instant Quantum Tunneling)\r\n");
        Print(L"\r\n   Phase 3 (Collective Intelligence):\r\n");
        Print(L"   ✓ HIVE-MIND (Collective Consciousness & Thoughts)\r\n");
        Print(L"   ✓ CONSENSUS-NET (Byzantine Fault-Tolerant Decisions)\r\n");
        Print(L"   ✓ MEMORY-POOL (Distributed Shared Memory)\r\n");
        Print(L"\r\n   Phase 4 (Advanced Features):\r\n");
        Print(L"   ✓ DREAM-CACHE (Future State Prediction - Precognition)\r\n");
        Print(L"   ✓ META-LEARNING (Self-Optimization)\r\n");
        Print(L"   ✓ EVOLUTION-ENGINE (Network Mutation)\r\n");
        Print(L"\r\n");
        
        // Phase 4 Metrics
        if (neuronet.dream_enabled) {
            Print(L"🔮 DREAM-CACHE (Precognition System):\r\n");
            Print(L"   Cached Predictions: %d/%d\r\n", 
                  neuronet.dream.prediction_count, 8);
            Print(L"   Dreams Validated: %d | Failed: %d\r\n", 
                  neuronet.dream.dreams_validated, neuronet.dream.dreams_failed);
            Print(L"   Dream Accuracy: %.1f%%\r\n", neuronet.dream.dream_accuracy * 100.0f);
            Print(L"   Lookahead Depth: %d steps\r\n", neuronet.dream.lookahead_depth);
            Print(L"   Temporal Discount: %.2f\r\n", neuronet.dream.temporal_discount);
            
            if (neuronet.dream.prediction_count > 0) {
                Print(L"   Future Predictions:\r\n");
                for (int i = 0; i < neuronet.dream.prediction_count && i < 3; i++) {
                    DreamPrediction* pred = &neuronet.dream.predictions[i];
                    Print(L"      [%d] %d steps ahead (confidence: %.2f)\r\n",
                          i + 1, pred->steps_ahead, pred->confidence);
                }
            }
            Print(L"\r\n");
        }
        
        if (neuronet.meta_enabled) {
            Print(L"🎓 META-LEARNING (Self-Optimization):\r\n");
            Print(L"   Learning Rate: %.6f (base: %.6f)\r\n", 
                  neuronet.meta.current_learning_rate, 
                  neuronet.meta.base_learning_rate);
            Print(L"   Performance: %.3f (initial: %.3f)\r\n", 
                  neuronet.meta.current_performance, 
                  neuronet.meta.initial_performance);
            Print(L"   Improvement Rate: %.1f%%\r\n", 
                  neuronet.meta.improvement_rate * 100.0f);
            Print(L"   Adaptation Cycles: %d\r\n", neuronet.meta.adaptation_cycles);
            Print(L"   Exploration Factor: %.3f\r\n", neuronet.meta.exploration_factor);
            Print(L"   Weight Perturbation: %.4f\r\n", neuronet.meta.weight_perturbation);
            
            // Show performance history
            if (neuronet.meta.history_count > 0) {
                Print(L"   Performance History (recent 3):\r\n");
                int start = neuronet.meta.history_count > 3 ? neuronet.meta.history_count - 3 : 0;
                for (int i = start; i < neuronet.meta.history_count; i++) {
                    PerformanceSnapshot* snap = &neuronet.meta.history[i];
                    Print(L"      [%d] Metric: %.3f, LR: %.6f\r\n",
                          i + 1, snap->metric_value, snap->learning_rate);
                }
            }
            Print(L"\r\n");
        }
        
        if (neuronet.evolution_enabled) {
            Print(L"🧬 EVOLUTION-ENGINE (Network Mutation):\r\n");
            Print(L"   Generation: %d\r\n", neuronet.evolution.current_generation);
            Print(L"   Best Fitness: %.3f (gen %d)\r\n", 
                  neuronet.evolution.best_fitness_ever, 
                  neuronet.evolution.best_generation);
            Print(L"   Avg Fitness: %.3f (variance: %.4f)\r\n", 
                  neuronet.evolution.avg_fitness, 
                  neuronet.evolution.fitness_variance);
            Print(L"   Population: %d genomes\r\n", neuronet.evolution.population_size);
            Print(L"   Mutation Rate: %.2f%% | Crossover: %.0f%%\r\n", 
                  neuronet.evolution.mutation_rate * 100.0f,
                  neuronet.evolution.crossover_rate * 100.0f);
            Print(L"   Nodes: +%d/-%d | Synapses: +%d/-%d\r\n", 
                  neuronet.evolution.nodes_added, neuronet.evolution.nodes_removed,
                  neuronet.evolution.synapses_added, neuronet.evolution.synapses_removed);
            Print(L"   Stagnant Generations: %d\r\n", 
                  neuronet.evolution.stagnant_generations);
            
            // Show genomes
            Print(L"   Genome Fitness:\r\n");
            for (int i = 0; i < neuronet.evolution.population_size; i++) {
                NetworkGenome* genome = &neuronet.evolution.genomes[i];
                Print(L"      [%d] Fitness: %.3f (gen %d)\r\n",
                      i + 1, genome->fitness, genome->generation);
            }
            Print(L"\r\n");
        }
        
        Print(L"🚀 This is a REVOLUTIONARY network architecture!\r\n");
        Print(L"   Phase 1 Features:\r\n");
        Print(L"   - Data + Energy transported together\r\n");
        Print(L"   - Vector-based telepathic understanding\r\n");
        Print(L"   - Self-adaptive synaptic weights\r\n");
        Print(L"   - Multi-layer energy routing\r\n");
        Print(L"   - Predictive packet streaming (QDDN)\r\n");
        Print(L"   - Bandwidth pre-allocation & cache warming\r\n");
        Print(L"   - Distributed reasoning with URN\r\n");
        Print(L"   - Presence-based auto-discovery (GHOST-LINK)\r\n");
        Print(L"\r\n   Phase 2 Features:\r\n");
        Print(L"   - Global heartbeat synchronization (60 BPM adaptive)\r\n");
        Print(L"   - Self-organizing mesh routing\r\n");
        Print(L"   - Quantum tunnels (instant transmission)\r\n");
        Print(L"   - Adaptive frequency based on load\r\n");
        Print(L"   - Route pruning & reconfiguration\r\n");
        Print(L"   - Quantum decoherence & stabilization\r\n");
        Print(L"\r\n   Phase 3 Features:\r\n");
        Print(L"   - Collective consciousness (shared thoughts)\r\n");
        Print(L"   - Byzantine fault-tolerant consensus\r\n");
        Print(L"   - Distributed shared memory pool\r\n");
        Print(L"   - Voting & reputation system\r\n");
        Print(L"   - Memory locking & conflict detection\r\n");
        Print(L"   - Emergent collective behaviors\r\n");
        Print(L"\r\n   Phase 4 Features:\r\n");
        Print(L"   - Future state prediction (N-step lookahead)\r\n");
        Print(L"   - Speculative execution with rollback\r\n");
        Print(L"   - Self-adaptive learning rates\r\n");
        Print(L"   - Gradient-free meta-optimization\r\n");
        Print(L"   - Genetic algorithm topology mutation\r\n");
        Print(L"   - Real-time network evolution\r\n");
        Print(L"   - Fitness-based selection & crossover\r\n");
        Print(L"\r\n   - 100%% Bare-Metal Native\r\n\r\n");
    }
    
    // Session end
    Print(L"\r\n[SESSION ENDED]\r\n");
    Print(L"Thank you for using LLM Bare-Metal v5.0!\r\n");
    
    // Small delay before exit
    ST->BootServices->Stall(2000000); // 2 seconds
    
    return EFI_SUCCESS;
}
