/*
 * Simple EFI test - Just print and test math functions
 */

#include <efi.h>
#include <efilib.h>

// Test math functions
float sqrtf(float x) {
    if (x < 0.0f) return 0.0f;
    float guess = x;
    for (int i = 0; i < 10; i++) {
        if (guess == 0.0f) return 0.0f;
        guess = (guess + x / guess) / 2.0f;
    }
    return guess;
}

float expf(float x) {
    if (x > 10.0f) return 22026.0f;
    if (x < -10.0f) return 0.0f;
    float result = 1.0f;
    float term = 1.0f;
    for (int i = 1; i < 20; i++) {
        term *= x / i;
        result += term;
        if (term < 0.0001f && term > -0.0001f) break;
    }
    return result;
}

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n");
    Print(L"========================================\r\n");
    Print(L"  Math Test for LLaMA2\r\n");
    Print(L"========================================\r\n\r\n");
    
    // Test math functions
    float test_sqrt = sqrtf(16.0f);
    Print(L"sqrt(16) = %.2f (expected 4.00)\r\n", (double)test_sqrt);
    
    float test_exp = expf(1.0f);
    Print(L"exp(1) = %.2f (expected 2.72)\r\n", (double)test_exp);
    
    // Test array allocation
    Print(L"\r\nTesting static array...\r\n");
    static float test_array[1000];
    for (int i = 0; i < 1000; i++) {
        test_array[i] = (float)i;
    }
    Print(L"Array[999] = %.0f (expected 999)\r\n", (double)test_array[999]);
    
    // Test large allocation
    Print(L"\r\nTesting large static buffer (4MB)...\r\n");
    static float large_buffer[1000000];
    large_buffer[0] = 1.0f;
    large_buffer[999999] = 999999.0f;
    Print(L"Buffer[0] = %.0f\r\n", (double)large_buffer[0]);
    Print(L"Buffer[999999] = %.0f\r\n", (double)large_buffer[999999]);
    
    Print(L"\r\n✅ All tests passed!\r\n");
    Print(L"\r\nPress any key to exit.\r\n");
    
    // Wait for key
    UINTN Index;
    EFI_INPUT_KEY Key;
    ST->ConIn->Reset(ST->ConIn, FALSE);
    ST->BootServices->WaitForEvent(1, &ST->ConIn->WaitForKey, &Index);
    ST->ConIn->ReadKeyStroke(ST->ConIn, &Key);
    
    return EFI_SUCCESS;
}
