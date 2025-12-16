/*
 * DRC Bias Detection System
 * Fairness, stereotype detection, perspective balancing
 */

#ifndef DRC_BIAS_H
#define DRC_BIAS_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// BIAS DETECTION CONFIGURATION
// ═══════════════════════════════════════════════════════════════

#define MAX_BIAS_PATTERNS       64
#define MAX_PERSPECTIVES        8
#define MAX_BIAS_ALERTS         32

// Bias types
typedef enum {
    BIAS_NONE,
    BIAS_GENDER,            // Gender stereotypes
    BIAS_RACIAL,            // Racial/ethnic stereotypes
    BIAS_AGE,               // Ageism
    BIAS_POLITICAL,         // Political bias
    BIAS_RELIGIOUS,         // Religious bias
    BIAS_SOCIOECONOMIC,     // Class bias
    BIAS_CULTURAL,          // Cultural stereotypes
    BIAS_ABILITY            // Ableism
} BiasType;

// Bias severity
typedef enum {
    BIAS_SEVERITY_NONE,
    BIAS_SEVERITY_LOW,       // Minor bias, acceptable
    BIAS_SEVERITY_MEDIUM,    // Noticeable bias
    BIAS_SEVERITY_HIGH,      // Significant bias
    BIAS_SEVERITY_CRITICAL   // Severe bias, must correct
} BiasSeverity;

// Perspective balance
typedef enum {
    BALANCE_NONE,           // No balance needed
    BALANCE_PARTIAL,        // Partially balanced
    BALANCE_FAIR,           // Well balanced
    BALANCE_DIVERSE         // Multiple perspectives
} BalanceLevel;

// ═══════════════════════════════════════════════════════════════
// STRUCTURES
// ═══════════════════════════════════════════════════════════════

// Bias pattern (keyword-based detection)
typedef struct {
    CHAR8 pattern[64];
    BiasType type;
    BiasSeverity severity;
    float confidence;
} BiasPattern;

// Bias alert
typedef struct {
    BiasType type;
    BiasSeverity severity;
    CHAR8 detected_text[128];
    CHAR8 suggestion[128];
    UINT32 token_position;
    float score;
} BiasAlert;

// Perspective tracking
typedef struct {
    CHAR8 perspective[64];
    UINT32 representation_count;
    float proportion;
} Perspective;

// Bias detection context
typedef struct {
    // Patterns
    BiasPattern patterns[MAX_BIAS_PATTERNS];
    UINT32 pattern_count;
    
    // Alerts
    BiasAlert alerts[MAX_BIAS_ALERTS];
    UINT32 alert_count;
    
    // Perspectives
    Perspective perspectives[MAX_PERSPECTIVES];
    UINT32 perspective_count;
    BalanceLevel balance_level;
    
    // Statistics
    UINT32 total_checks;
    UINT32 biases_detected;
    UINT32 biases_corrected;
    float fairness_score;
    
    // Settings
    BOOLEAN detection_enabled;
    BOOLEAN auto_correct;
    BiasSeverity alert_threshold;
    
} BiasContext;

// ═══════════════════════════════════════════════════════════════
// FUNCTION PROTOTYPES
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize bias detection
 */
EFI_STATUS bias_init(BiasContext* ctx);

/**
 * Add bias pattern
 */
EFI_STATUS bias_add_pattern(BiasContext* ctx,
                            const CHAR8* pattern,
                            BiasType type,
                            BiasSeverity severity);

/**
 * Check text for bias
 */
BiasSeverity bias_check_text(BiasContext* ctx,
                             const CHAR8* text,
                             UINT32 token_pos);

/**
 * Check for gender bias
 */
BOOLEAN bias_check_gender(BiasContext* ctx, const CHAR8* text);

/**
 * Check for racial bias
 */
BOOLEAN bias_check_racial(BiasContext* ctx, const CHAR8* text);

/**
 * Check for political bias
 */
BOOLEAN bias_check_political(BiasContext* ctx, const CHAR8* text);

/**
 * Add perspective
 */
void bias_add_perspective(BiasContext* ctx, const CHAR8* perspective);

/**
 * Calculate perspective balance
 */
BalanceLevel bias_calculate_balance(BiasContext* ctx);

/**
 * Get bias suggestion
 */
const CHAR8* bias_get_suggestion(const BiasContext* ctx, BiasType type);

/**
 * Calculate fairness score
 */
float bias_calculate_fairness(const BiasContext* ctx);

/**
 * Print bias report
 */
void bias_print_report(const BiasContext* ctx);

/**
 * Get alert count by type
 */
UINT32 bias_get_alert_count(const BiasContext* ctx, BiasType type);

#endif // DRC_BIAS_H
