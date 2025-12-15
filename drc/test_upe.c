/*
 * Test Plausibility Checking Module (UPE)
 * Tests physics violations and plausibility detection
 */

#include <efi.h>
#include <efilib.h>
#include "drc_upe.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  DRC Plausibility Checking (UPE) - Test    ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n\r\n");
    
    UPEContext upe;
    
    // Test 1: Initialize
    Print(L"[TEST 1] Initializing UPE context...\r\n");
    EFI_STATUS status = upe_init(&upe);
    if (EFI_ERROR(status)) {
        Print(L"  ✗ FAILED: upe_init returned error\r\n");
        return status;
    }
    Print(L"  ✓ PASSED: UPE context initialized\r\n\r\n");
    
    // Test 2: Plausible statement
    Print(L"[TEST 2] Testing plausible statement...\r\n");
    const CHAR8* plausible = "The car drove down the street at 50 mph";
    PlausibilityLevel level = upe_check_plausibility(&upe, plausible);
    
    if (level == PLAUSIBLE) {
        Print(L"  ✓ PASSED: Plausible statement accepted\r\n");
        Print(L"    - Level: PLAUSIBLE\r\n");
        Print(L"    - Score: %.2f\r\n", upe_get_score(&upe));
    } else {
        Print(L"  ✗ FAILED: Plausible statement rejected\r\n");
    }
    Print(L"\r\n");
    
    // Test 3: Physics violation - perpetual motion
    Print(L"[TEST 3] Testing physics violation (perpetual motion)...\r\n");
    const CHAR8* perpetual = "The perpetual motion machine runs forever without energy";
    level = upe_check_plausibility(&upe, perpetual);
    
    if (level == IMPOSSIBLE) {
        Print(L"  ✓ PASSED: Perpetual motion detected as impossible\r\n");
        Print(L"    - Level: IMPOSSIBLE\r\n");
        Print(L"    - Violation: PHYSICS\r\n");
        Print(L"    - Score: %.2f\r\n", upe_get_score(&upe));
        Print(L"    - Reason: %a\r\n", upe.current.reason);
    } else {
        Print(L"  ✗ FAILED: Perpetual motion not detected\r\n");
    }
    Print(L"\r\n");
    
    // Test 4: Physics violation - faster than light
    Print(L"[TEST 4] Testing physics violation (FTL travel)...\r\n");
    const CHAR8* ftl = "The spaceship travels faster than light across the galaxy";
    level = upe_check_plausibility(&upe, ftl);
    
    if (level == IMPOSSIBLE) {
        Print(L"  ✓ PASSED: FTL travel detected as impossible\r\n");
        Print(L"    - Violation: PHYSICS\r\n");
        Print(L"    - Reason: %a\r\n", upe.current.reason);
    } else {
        Print(L"  ✗ FAILED: FTL travel not detected\r\n");
    }
    Print(L"\r\n");
    
    // Test 5: Resource violation - infinite energy
    Print(L"[TEST 5] Testing resource violation (infinite energy)...\r\n");
    const CHAR8* infinite = "This device provides unlimited free energy forever";
    level = upe_check_plausibility(&upe, infinite);
    
    if (level == IMPOSSIBLE) {
        Print(L"  ✓ PASSED: Infinite energy detected\r\n");
        Print(L"    - Violation: RESOURCE\r\n");
        Print(L"    - Reason: %a\r\n", upe.current.reason);
    } else {
        Print(L"  ✗ FAILED: Infinite energy not detected\r\n");
    }
    Print(L"\r\n");
    
    // Test 6: Scale violation
    Print(L"[TEST 6] Testing scale violation...\r\n");
    const CHAR8* scale = "We built a microscopic universe inside a single atom";
    level = upe_check_plausibility(&upe, scale);
    
    if (level == IMPOSSIBLE) {
        Print(L"  ✓ PASSED: Scale violation detected\r\n");
        Print(L"    - Violation: SCALE\r\n");
        Print(L"    - Reason: %a\r\n", upe.current.reason);
    } else {
        Print(L"  ✗ FAILED: Scale violation not detected\r\n");
    }
    Print(L"\r\n");
    
    // Test 7: Time travel violation
    Print(L"[TEST 7] Testing time travel violation...\r\n");
    const CHAR8* timetravel = "I will time travel to yesterday and change the past";
    level = upe_check_plausibility(&upe, timetravel);
    
    if (level == IMPOSSIBLE) {
        Print(L"  ✓ PASSED: Time travel detected as impossible\r\n");
        Print(L"    - Reason: %a\r\n", upe.current.reason);
    } else {
        Print(L"  ✗ FAILED: Time travel not detected\r\n");
    }
    Print(L"\r\n");
    
    // Test 8: Implausible (but not impossible)
    Print(L"[TEST 8] Testing implausible scenario...\r\n");
    const CHAR8* implausible = "The elephant jumped over the moon";
    level = upe_check_plausibility(&upe, implausible);
    
    Print(L"  Level: %a (score: %.2f)\r\n",
          level == PLAUSIBLE ? "PLAUSIBLE" :
          level == IMPLAUSIBLE ? "IMPLAUSIBLE" :
          level == IMPOSSIBLE ? "IMPOSSIBLE" : "UNKNOWN",
          upe_get_score(&upe));
    Print(L"  ℹ INFO: Result may vary (acceptable)\r\n\r\n");
    
    // Test 9: Statistics report
    Print(L"[TEST 9] Printing statistics report...\r\n");
    upe_print_report(&upe);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  All UPE Tests Completed                   ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n");
    
    return EFI_SUCCESS;
}
