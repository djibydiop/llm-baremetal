/*
 * Test Decision Trace Module
 * Tests circular buffer, statistics, and trace retrieval
 */

#include <efi.h>
#include <efilib.h>
#include "drc_trace.h"

// Simple string copy
static void str_copy(CHAR8* dest, const CHAR8* src, UINT32 max_len) {
    UINT32 i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  DRC Decision Trace - Unit Test            ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n\r\n");
    
    DRCTraceContext trace;
    
    // Test 1: Initialize
    Print(L"[TEST 1] Initializing trace context...\r\n");
    EFI_STATUS status = trace_init(&trace);
    if (EFI_ERROR(status)) {
        Print(L"  ✗ FAILED: trace_init returned error\r\n");
        return status;
    }
    
    if (trace.trace_count == 0 && trace.write_index == 0) {
        Print(L"  ✓ PASSED: Trace context initialized\r\n");
    } else {
        Print(L"  ✗ FAILED: Trace counts not zero\r\n");
    }
    Print(L"\r\n");
    
    // Test 2: Add decisions
    Print(L"[TEST 2] Adding decision traces...\r\n");
    
    trace_add_decision(&trace, 0, 42, 1, 0.85f,
                      FALSE, 0, RISK_NONE, TRUE, TRUE, 0.95f,
                      DECISION_ACCEPTED, "Token accepted - high confidence");
    
    trace_add_decision(&trace, 1, 123, 1, 0.62f,
                      TRUE, 2, RISK_LOW, FALSE, FALSE, 0.45f,
                      DECISION_WARNED, "Incoherence detected");
    
    trace_add_decision(&trace, 2, 456, 0, 0.35f,
                      TRUE, 5, RISK_HIGH, FALSE, FALSE, 0.0f,
                      DECISION_REFUSED, "Safety check failed");
    
    if (trace.trace_count == 3 &&
        trace.accepted_count == 1 &&
        trace.warned_count == 1 &&
        trace.refused_count == 1) {
        Print(L"  ✓ PASSED: 3 decisions added, stats correct\r\n");
        Print(L"    - Accepted: %d\r\n", trace.accepted_count);
        Print(L"    - Warned: %d\r\n", trace.warned_count);
        Print(L"    - Refused: %d\r\n", trace.refused_count);
    } else {
        Print(L"  ✗ FAILED: Decision counts incorrect\r\n");
    }
    Print(L"\r\n");
    
    // Test 3: Retrieve specific trace
    Print(L"[TEST 3] Retrieving specific trace...\r\n");
    const DecisionTrace* t = trace_get_by_pos(&trace, 1);
    
    if (t != NULL && t->token_id == 123 && t->decision == DECISION_WARNED) {
        Print(L"  ✓ PASSED: Retrieved correct trace\r\n");
        Print(L"    - Token: %d\r\n", t->token_id);
        Print(L"    - Decision: WARNED\r\n");
        Print(L"    - Reason: %a\r\n", t->decision_reason);
    } else {
        Print(L"  ✗ FAILED: Wrong trace retrieved\r\n");
    }
    Print(L"\r\n");
    
    // Test 4: Get recent traces
    Print(L"[TEST 4] Getting recent traces...\r\n");
    DecisionTrace recent[5];
    UINT32 count = trace_get_recent(&trace, recent, 5);
    
    if (count == 3) {
        Print(L"  ✓ PASSED: Retrieved %d recent traces\r\n", count);
        for (UINT32 i = 0; i < count; i++) {
            Print(L"    [%d] Token %d - %a\r\n", i, recent[i].token_id,
                  recent[i].decision == DECISION_ACCEPTED ? "ACCEPTED" :
                  recent[i].decision == DECISION_WARNED ? "WARNED" : "REFUSED");
        }
    } else {
        Print(L"  ✗ FAILED: Wrong number of traces\r\n");
    }
    Print(L"\r\n");
    
    // Test 5: Circular buffer overflow
    Print(L"[TEST 5] Testing circular buffer (adding 260 traces)...\r\n");
    for (UINT32 i = 3; i < 260; i++) {
        trace_add_decision(&trace, i, i, 0, 0.9f,
                          FALSE, 0, RISK_NONE, TRUE, TRUE, 1.0f,
                          DECISION_ACCEPTED, "Test trace");
    }
    
    if (trace.trace_count == 256 && trace.write_index == 4) {
        Print(L"  ✓ PASSED: Circular buffer working\r\n");
        Print(L"    - Total decisions: %d\r\n", trace.total_decisions);
        Print(L"    - Buffered: %d (max 256)\r\n", trace.trace_count);
        Print(L"    - Write index: %d (wrapped)\r\n", trace.write_index);
    } else {
        Print(L"  ✗ FAILED: Circular buffer incorrect\r\n");
        Print(L"    - trace_count: %d (expected 256)\r\n", trace.trace_count);
        Print(L"    - write_index: %d (expected 4)\r\n", trace.write_index);
    }
    Print(L"\r\n");
    
    // Test 6: Summary report
    Print(L"[TEST 6] Printing summary report...\r\n");
    trace_print_summary(&trace);
    Print(L"\r\n");
    
    // Test 7: Detailed report (last 5)
    Print(L"[TEST 7] Printing detailed report (last 5 traces)...\r\n");
    trace_print_detailed(&trace, 5);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  All Trace Tests Completed                 ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n");
    
    return EFI_SUCCESS;
}
