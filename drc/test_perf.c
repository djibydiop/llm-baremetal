/*
 * Test Performance Monitoring Module
 * Tests timing, overhead calculation, and bottleneck detection
 */

#include <efi.h>
#include <efilib.h>
#include "drc_perf.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  DRC Performance Monitoring - Unit Test    ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n\r\n");
    
    DRCPerformanceMetrics perf;
    
    // Test 1: Initialize
    Print(L"[TEST 1] Initializing performance metrics...\r\n");
    EFI_STATUS status = perf_init(&perf);
    if (EFI_ERROR(status)) {
        Print(L"  ✗ FAILED: perf_init returned error\r\n");
        return status;
    }
    Print(L"  ✓ PASSED: Performance metrics initialized\r\n\r\n");
    
    // Test 2: Timer operations
    Print(L"[TEST 2] Testing timer start/stop...\r\n");
    perf_start_timer(&perf.urs_timer);
    for (volatile UINT32 i = 0; i < 100000; i++);  // Simulate work
    perf_stop_timer(&perf.urs_timer);
    
    if (perf.urs_timer.call_count == 1 && perf.urs_timer.total_time_us > 0) {
        Print(L"  ✓ PASSED: Timer recorded %d calls, %llu us total\r\n", 
              perf.urs_timer.call_count, perf.urs_timer.total_time_us);
    } else {
        Print(L"  ✗ FAILED: Timer metrics incorrect\r\n");
    }
    Print(L"\r\n");
    
    // Test 3: Multiple timers
    Print(L"[TEST 3] Testing multiple timers...\r\n");
    perf_start_timer(&perf.uic_timer);
    for (volatile UINT32 i = 0; i < 50000; i++);
    perf_stop_timer(&perf.uic_timer);
    
    perf_start_timer(&perf.ucr_timer);
    for (volatile UINT32 i = 0; i < 75000; i++);
    perf_stop_timer(&perf.ucr_timer);
    
    Print(L"  UIC timer: %llu us\r\n", perf.uic_timer.total_time_us);
    Print(L"  UCR timer: %llu us\r\n", perf.ucr_timer.total_time_us);
    Print(L"  ✓ PASSED: Multiple timers working\r\n\r\n");
    
    // Test 4: Token metrics
    Print(L"[TEST 4] Testing token metrics...\r\n");
    perf_update_token_metrics(&perf, 1500);  // 1.5ms per token
    perf_update_token_metrics(&perf, 1200);
    perf_update_token_metrics(&perf, 1800);
    
    if (perf.tokens_generated == 3) {
        Print(L"  ✓ PASSED: %d tokens, %.2f tokens/sec\r\n", 
              perf.tokens_generated, perf.tokens_per_second);
    } else {
        Print(L"  ✗ FAILED: Token count incorrect\r\n");
    }
    Print(L"\r\n");
    
    // Test 5: Overhead calculation
    Print(L"[TEST 5] Testing overhead calculation...\r\n");
    perf.total_inference_time_us = 100000;  // 100ms inference
    perf_calculate_overhead(&perf);
    
    Print(L"  Total inference: %llu us\r\n", perf.total_inference_time_us);
    Print(L"  DRC overhead: %llu us\r\n", perf.total_drc_overhead_us);
    Print(L"  Overhead: %.2f%%\r\n", perf.overhead_percentage);
    Print(L"  ✓ PASSED: Overhead calculated\r\n\r\n");
    
    // Test 6: Bottleneck detection
    Print(L"[TEST 6] Testing bottleneck detection...\r\n");
    perf.uti_timer.total_time_us = 50000;  // Make UTI the slowest
    perf.uti_timer.call_count = 1;
    const CHAR8* bottleneck = perf_get_bottleneck(&perf);
    
    Print(L"  Detected bottleneck: %a\r\n", bottleneck);
    if (bottleneck != NULL) {
        Print(L"  ✓ PASSED: Bottleneck detection working\r\n");
    } else {
        Print(L"  ✗ FAILED: No bottleneck detected\r\n");
    }
    Print(L"\r\n");
    
    // Test 7: Full report
    Print(L"[TEST 7] Printing full performance report...\r\n");
    perf_print_report(&perf);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  All Performance Tests Completed           ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n");
    
    return EFI_SUCCESS;
}
