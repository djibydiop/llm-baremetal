/*
 * Simplified Performance Monitoring Test
 * Tests basic structure initialization without complex timing
 */

#include <efi.h>
#include <efilib.h>
#include "drc_perf.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  DRC Performance Module - Simple Test     ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n\r\n");
    
    DRCPerformanceMetrics perf;
    
    // Test 1: Initialize
    Print(L"[TEST 1] Initializing performance metrics...\r\n");
    EFI_STATUS status = perf_init(&perf);
    if (EFI_ERROR(status)) {
        Print(L"  ✗ FAILED\r\n");
        goto end;
    }
    Print(L"  ✓ PASSED\r\n\r\n");
    
    // Test 2: Check structure
    Print(L"[TEST 2] Checking structure initialized...\r\n");
    if (perf.urs_timer.call_count == 0 && 
        perf.uic_timer.call_count == 0 &&
        perf.tokens_generated == 0) {
        Print(L"  ✓ PASSED - All counters at zero\r\n");
    } else {
        Print(L"  ✗ FAILED - Unexpected initial values\r\n");
    }
    Print(L"\r\n");
    
    // Test 3: Start/stop timer
    Print(L"[TEST 3] Testing timer operations...\r\n");
    perf_start_timer(&perf.urs_timer);
    perf_stop_timer(&perf.urs_timer);
    
    if (perf.urs_timer.call_count == 1) {
        Print(L"  ✓ PASSED - Timer recorded 1 call\r\n");
    } else {
        Print(L"  ✗ FAILED - Call count: %d\r\n", perf.urs_timer.call_count);
    }
    Print(L"\r\n");
    
    // Test 4: Update token metrics
    Print(L"[TEST 4] Testing token metrics...\r\n");
    perf_update_token_metrics(&perf, 1000);
    perf_update_token_metrics(&perf, 1000);
    
    if (perf.tokens_generated == 2) {
        Print(L"  ✓ PASSED - 2 tokens recorded\r\n");
    } else {
        Print(L"  ✗ FAILED - Token count: %d\r\n", perf.tokens_generated);
    }
    Print(L"\r\n");
    
    Print(L"╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  Performance Module Tests Complete         ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n");
    
end:
    Print(L"\r\nPress any key to exit...\r\n");
    UINTN Index;
    EFI_INPUT_KEY Key;
    SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &Index);
    SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);
    
    return EFI_SUCCESS;
}
