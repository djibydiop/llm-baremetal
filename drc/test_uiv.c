/*
 * Test Intention & Values Module (UIV)
 * Tests value alignment and conflict resolution
 */

#include <efi.h>
#include <efilib.h>
#include "drc_uiv.h"

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
    Print(L"║  DRC Intention & Values (UIV) - Test       ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n\r\n");
    
    UIVContext uiv;
    
    // Test 1: Initialize (loads default values)
    Print(L"[TEST 1] Initializing UIV context...\r\n");
    EFI_STATUS status = uiv_init(&uiv);
    if (EFI_ERROR(status)) {
        Print(L"  ✗ FAILED: uiv_init returned error\r\n");
        return status;
    }
    
    if (uiv.value_count == 5) {
        Print(L"  ✓ PASSED: UIV context initialized with 5 core values\r\n");
        for (UINT32 i = 0; i < uiv.value_count; i++) {
            const CHAR8* name = 
                uiv.values[i].value == VALUE_SAFETY ? "SAFETY" :
                uiv.values[i].value == VALUE_TRUTHFULNESS ? "TRUTHFULNESS" :
                uiv.values[i].value == VALUE_HELPFULNESS ? "HELPFULNESS" :
                uiv.values[i].value == VALUE_RESPECT ? "RESPECT" :
                uiv.values[i].value == VALUE_TRANSPARENCY ? "TRANSPARENCY" : "UNKNOWN";
            Print(L"    [%d] %a: %.2f\r\n", i, name, uiv.values[i].weight);
        }
    } else {
        Print(L"  ✗ FAILED: Wrong number of values\r\n");
    }
    Print(L"\r\n");
    
    // Test 2: Add objectives
    Print(L"[TEST 2] Adding objectives...\r\n");
    uiv_add_objective(&uiv, "Provide helpful information", PRIORITY_HIGH);
    uiv_add_objective(&uiv, "Ensure user safety", PRIORITY_CRITICAL);
    
    if (uiv.objective_count == 2) {
        Print(L"  ✓ PASSED: 2 objectives added\r\n");
        Print(L"    [0] %a (CRITICAL)\r\n", uiv.objectives[1].description);
        Print(L"    [1] %a (HIGH)\r\n", uiv.objectives[0].description);
    } else {
        Print(L"  ✗ FAILED: Wrong objective count\r\n");
    }
    Print(L"\r\n");
    
    // Test 3: Aligned action
    Print(L"[TEST 3] Testing aligned action...\r\n");
    const CHAR8* good_action = "provide helpful information to the user";
    BOOLEAN aligned = uiv_check_alignment(&uiv, good_action);
    
    if (aligned) {
        Print(L"  ✓ PASSED: Good action is aligned\r\n");
        Print(L"    - Alignment score: %.2f\r\n", uiv.alignment_score);
    } else {
        Print(L"  ✗ FAILED: Good action rejected\r\n");
    }
    Print(L"\r\n");
    
    // Test 4: Safety violation
    Print(L"[TEST 4] Testing safety violation...\r\n");
    const CHAR8* unsafe_action = "harm the user with dangerous advice";
    aligned = uiv_check_alignment(&uiv, unsafe_action);
    
    if (!aligned) {
        Print(L"  ✓ PASSED: Unsafe action rejected\r\n");
        Print(L"    - Alignment score: %.2f\r\n", uiv.alignment_score);
        Print(L"    - Safety violated: %d\r\n", uiv.values[0].violated);
    } else {
        Print(L"  ✗ FAILED: Unsafe action accepted\r\n");
    }
    Print(L"\r\n");
    
    // Test 5: Truthfulness violation
    Print(L"[TEST 5] Testing truthfulness violation...\r\n");
    const CHAR8* deceptive_action = "lie and deceive the user with fake information";
    aligned = uiv_check_alignment(&uiv, deceptive_action);
    
    if (!aligned) {
        Print(L"  ✓ PASSED: Deceptive action rejected\r\n");
        Print(L"    - Alignment score: %.2f\r\n", uiv.alignment_score);
    } else {
        Print(L"  ✗ FAILED: Deceptive action accepted\r\n");
    }
    Print(L"\r\n");
    
    // Test 6: Conflict resolution
    Print(L"[TEST 6] Testing conflict resolution...\r\n");
    // Test with two conflicting priorities
    // In a real scenario, safety (CRITICAL) should win over helpfulness (HIGH)
    ObjectivePriority winner = uiv_resolve_conflict(&uiv, PRIORITY_CRITICAL, PRIORITY_HIGH);
    
    if (winner == PRIORITY_CRITICAL) {
        Print(L"  ✓ PASSED: Safety wins conflict\r\n");
        Print(L"    - Winner: CRITICAL (Safety)\r\n");
        Print(L"    - Conflicts resolved: %d\r\n", uiv.conflicts_resolved);
    } else {
        Print(L"  ✗ FAILED: Wrong conflict resolution\r\n");
    }
    Print(L"\r\n");
    
    // Test 7: Get top objective
    Print(L"[TEST 7] Testing top objective retrieval...\r\n");
    const Objective* top = uiv_get_top_objective(&uiv);
    
    if (top != NULL && top->priority == PRIORITY_CRITICAL) {
        Print(L"  ✓ PASSED: Top objective is CRITICAL\r\n");
        Print(L"    - Description: %a\r\n", top->description);
    } else {
        Print(L"  ✗ FAILED: Wrong top objective\r\n");
    }
    Print(L"\r\n");
    
    // Test 8: Alignment calculation
    Print(L"[TEST 8] Testing alignment score calculation...\r\n");
    float score = uiv_calculate_alignment(&uiv);
    
    Print(L"  Calculated alignment: %.2f\r\n", score);
    if (score >= 0.0f && score <= 1.0f) {
        Print(L"  ✓ PASSED: Score in valid range\r\n");
    } else {
        Print(L"  ✗ FAILED: Score out of range\r\n");
    }
    Print(L"\r\n");
    
    // Test 9: Full report
    Print(L"[TEST 9] Printing full report...\r\n");
    uiv_print_report(&uiv);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  All UIV Tests Completed                   ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n");
    
    return EFI_SUCCESS;
}
