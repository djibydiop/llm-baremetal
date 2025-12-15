/*
 * Simple Hello World test for DRC modules
 */

#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n╔══════════════════════════════════════════════╗\r\n");
    Print(L"║  DRC v5.1 - Hello Test                     ║\r\n");
    Print(L"╚══════════════════════════════════════════════╝\r\n\r\n");
    
    Print(L"EFI application loaded successfully!\r\n");
    Print(L"Testing basic Print functionality... ✓\r\n\r\n");
    
    Print(L"Press any key to exit...\r\n");
    
    // Wait for key
    UINTN Index;
    EFI_INPUT_KEY Key;
    SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &Index);
    SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);
    
    return EFI_SUCCESS;
}
