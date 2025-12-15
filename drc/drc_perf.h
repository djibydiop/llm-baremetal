/*
 * DRC Performance Monitoring System
 * Track overhead and identify bottlenecks
 */

#ifndef DRC_PERF_H
#define DRC_PERF_H

#include <efi.h>
#include <efilib.h>

// ═══════════════════════════════════════════════════════════════
// PERFORMANCE MONITORING
// ═══════════════════════════════════════════════════════════════

typedef struct {
    UINT64 start_time;
    UINT64 end_time;
    UINT64 duration_us;
    UINT32 call_count;
    UINT64 total_time_us;
    UINT64 min_time_us;
    UINT64 max_time_us;
} PerfTimer;

typedef struct {
    // Per-unit timers
    PerfTimer urs_timer;
    PerfTimer uic_timer;
    PerfTimer ucr_timer;
    PerfTimer uti_timer;
    PerfTimer uco_timer;
    PerfTimer ums_timer;
    PerfTimer verification_timer;
    PerfTimer total_timer;
    
    // Overhead metrics
    UINT64 total_inference_time_us;
    UINT64 total_drc_overhead_us;
    float overhead_percentage;
    
    // Token generation metrics
    UINT32 tokens_generated;
    float avg_time_per_token_us;
    float tokens_per_second;
    
    // Memory usage (estimated)
    UINT64 memory_used_bytes;
    
    // System metrics
    UINT64 system_start_time;
    UINT64 system_uptime_seconds;
} DRCPerformanceMetrics;

// ═══════════════════════════════════════════════════════════════
// PERFORMANCE FUNCTIONS
// ═══════════════════════════════════════════════════════════════

/**
 * Initialize performance monitoring
 */
EFI_STATUS perf_init(DRCPerformanceMetrics* perf);

/**
 * Start timer for a specific unit
 */
void perf_start_timer(PerfTimer* timer);

/**
 * Stop timer and update statistics
 */
void perf_stop_timer(PerfTimer* timer);

/**
 * Calculate overhead percentage
 */
float perf_calculate_overhead(DRCPerformanceMetrics* perf);

/**
 * Get current timestamp in microseconds
 */
UINT64 perf_get_timestamp_us(void);

/**
 * Update token metrics
 */
void perf_update_token_metrics(DRCPerformanceMetrics* perf, UINT64 inference_time_us);

/**
 * Print performance report
 */
void perf_print_report(DRCPerformanceMetrics* perf);

/**
 * Get bottleneck (slowest unit)
 */
const CHAR8* perf_get_bottleneck(DRCPerformanceMetrics* perf);

#endif // DRC_PERF_H
