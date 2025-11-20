/*
 * hello_efi.c - Simple "Hello World" for EFI
 * 
 * Purpose: Test if QEMU can boot our EFI binaries at all
 * If this works, the issue is with the LLM inference, not the boot process
 */

#include <efi.h>
#include <efilib.h>

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\n");
    Print(L"========================================\n");
    Print(L"   HELLO FROM BARE-METAL EFI!\n");
    Print(L"========================================\n");
    Print(L"\n");
    Print(L"This is a simple test to verify:\n");
    Print(L"  [x] UEFI boot works\n");
    Print(L"  [x] EFI binary loads correctly\n");
    Print(L"  [x] Console output works\n");
    Print(L"  [x] QEMU virtualization works\n");
    Print(L"\n");
    Print(L"If you can see this message, then:\n");
    Print(L"  * QEMU boot is successful!\n");
    Print(L"  * EFI environment is working!\n");
    Print(L"  * Ready for LLM inference!\n");
    Print(L"\n");
    Print(L"Next step: Run chatbot.efi for GPT-Nano\n");
    Print(L"\n");
    Print(L"Press any key to exit...\n");
    
    // Wait for keypress
    EFI_INPUT_KEY Key;
    SystemTable->ConIn->Reset(SystemTable->ConIn, FALSE);
    while (SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key) == EFI_NOT_READY);
    
    return EFI_SUCCESS;
}
