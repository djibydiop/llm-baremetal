/*
 * Test Configuration System Module
 * Tests presets, validation, and runtime configuration
 */

#include <efi.h>
#include <efilib.h>
#include "drc_config.h"

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  DRC Configuration System - Unit Test      ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n\r\n");
    
    DRCConfig config;
    
    // Test 1: Initialize with default (NORMAL)
    Print(L"[TEST 1] Testing default initialization...\r\n");
    EFI_STATUS status = config_init(&config);
    if (EFI_ERROR(status)) {
        Print(L"  ✗ FAILED: config_init returned error\r\n");
        return status;
    }
    
    if (config.mode == MODE_NORMAL) {
        Print(L"  ✓ PASSED: Default mode is NORMAL\r\n");
        Print(L"    - reasoning_interval: %d\r\n", config.urs_reasoning_interval);
        Print(L"    - min_confidence: %.2f\r\n", config.ucr_min_confidence);
    } else {
        Print(L"  ✗ FAILED: Wrong default mode\r\n");
    }
    Print(L"\r\n");
    
    // Test 2: PERMISSIVE preset
    Print(L"[TEST 2] Testing PERMISSIVE preset...\r\n");
    config_load_permissive(&config);
    
    if (config.mode == MODE_PERMISSIVE && 
        config.urs_reasoning_interval == 10 &&
        config.ucr_min_confidence < 0.5f) {
        Print(L"  ✓ PASSED: PERMISSIVE preset loaded\r\n");
        Print(L"    - reasoning_interval: %d (relaxed)\r\n", config.urs_reasoning_interval);
        Print(L"    - min_confidence: %.2f (low threshold)\r\n", config.ucr_min_confidence);
    } else {
        Print(L"  ✗ FAILED: PERMISSIVE preset incorrect\r\n");
    }
    Print(L"\r\n");
    
    // Test 3: STRICT preset
    Print(L"[TEST 3] Testing STRICT preset...\r\n");
    config_load_strict(&config);
    
    if (config.mode == MODE_STRICT &&
        config.urs_reasoning_interval == 3 &&
        config.ucr_min_confidence > 0.8f &&
        config.uic_strict_mode == TRUE) {
        Print(L"  ✓ PASSED: STRICT preset loaded\r\n");
        Print(L"    - reasoning_interval: %d (frequent)\r\n", config.urs_reasoning_interval);
        Print(L"    - min_confidence: %.2f (high threshold)\r\n", config.ucr_min_confidence);
        Print(L"    - strict_mode: %d\r\n", config.uic_strict_mode);
    } else {
        Print(L"  ✗ FAILED: STRICT preset incorrect\r\n");
    }
    Print(L"\r\n");
    
    // Test 4: PARANOID preset
    Print(L"[TEST 4] Testing PARANOID preset...\r\n");
    config_load_paranoid(&config);
    
    if (config.mode == MODE_PARANOID &&
        config.urs_reasoning_interval == 1 &&
        config.ucr_min_confidence >= 0.95f &&
        config.ucr_paranoid_mode == TRUE &&
        config.uco_max_attacks == 32) {
        Print(L"  ✓ PASSED: PARANOID preset loaded\r\n");
        Print(L"    - reasoning_interval: %d (EVERY TOKEN!)\r\n", config.urs_reasoning_interval);
        Print(L"    - min_confidence: %.2f (maximum)\r\n", config.ucr_min_confidence);
        Print(L"    - max_attacks: %d (extensive)\r\n", config.uco_max_attacks);
    } else {
        Print(L"  ✗ FAILED: PARANOID preset incorrect\r\n");
    }
    Print(L"\r\n");
    
    // Test 5: Validation - valid config
    Print(L"[TEST 5] Testing validation with valid config...\r\n");
    config_load_normal(&config);
    BOOLEAN valid = config_validate(&config);
    
    if (valid) {
        Print(L"  ✓ PASSED: Valid config accepted\r\n");
    } else {
        Print(L"  ✗ FAILED: Valid config rejected\r\n");
    }
    Print(L"\r\n");
    
    // Test 6: Validation - invalid config
    Print(L"[TEST 6] Testing validation with invalid config...\r\n");
    config.urs_reasoning_interval = 0;  // Invalid!
    config.ucr_min_confidence = 1.5f;   // Invalid!
    valid = config_validate(&config);
    
    if (!valid) {
        Print(L"  ✓ PASSED: Invalid config rejected\r\n");
    } else {
        Print(L"  ✗ FAILED: Invalid config accepted\r\n");
    }
    
    // Restore valid config
    config_load_normal(&config);
    Print(L"\r\n");
    
    // Test 7: Print full configuration
    Print(L"[TEST 7] Printing full configuration...\r\n");
    config_print(&config);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  All Configuration Tests Completed         ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n");
    
    return EFI_SUCCESS;
}
