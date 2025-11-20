#include <efi.h>
#include <efilib.h>

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    Print(L"\r\n");
    Print(L"========================================\r\n");
    Print(L"  MINIMAL EFI TEST - WORKING!\r\n");
    Print(L"========================================\r\n");
    Print(L"\r\n");
    Print(L"✅ EFI boot successful\r\n");
    Print(L"✅ Print() function working\r\n");
    Print(L"✅ UEFI environment initialized\r\n");
    Print(L"\r\n");
    Print(L"Press any key to test file system...\r\n");
    
    // Wait for key
    UINTN Index;
    SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &Index);
    
    // Try to open file system
    Print(L"\r\n[TEST] Opening file system...\r\n");
    
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_STATUS Status = SystemTable->BootServices->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (void**)&LoadedImage
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"❌ Failed to get loaded image protocol: %r\r\n", Status);
    } else {
        Print(L"✅ Loaded image protocol: OK\r\n");
    }
    
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    Status = SystemTable->BootServices->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (void**)&FileSystem
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"❌ Failed to get file system protocol: %r\r\n", Status);
    } else {
        Print(L"✅ File system protocol: OK\r\n");
        
        EFI_FILE *Root;
        Status = FileSystem->OpenVolume(FileSystem, &Root);
        if (EFI_ERROR(Status)) {
            Print(L"❌ Failed to open volume: %r\r\n", Status);
        } else {
            Print(L"✅ Volume opened: OK\r\n");
            
            // Try to open stories15M.bin
            EFI_FILE *ModelFile;
            Status = Root->Open(
                Root,
                &ModelFile,
                L"stories15M.bin",
                EFI_FILE_MODE_READ,
                0
            );
            
            if (EFI_ERROR(Status)) {
                Print(L"❌ Failed to open stories15M.bin: %r\r\n", Status);
            } else {
                Print(L"✅ stories15M.bin opened: OK\r\n");
                
                // Get file size
                EFI_FILE_INFO FileInfoBuffer;
                UINTN FileInfoSize = sizeof(FileInfoBuffer);
                Status = ModelFile->GetInfo(
                    ModelFile,
                    &gEfiFileInfoGuid,
                    &FileInfoSize,
                    &FileInfoBuffer
                );
                
                if (EFI_ERROR(Status)) {
                    Print(L"⚠️  Could not get file info: %r\r\n", Status);
                } else {
                    UINT64 size = FileInfoBuffer.FileSize;
                    Print(L"✅ File size: %lu bytes (%.2f MB)\r\n", 
                          size, (float)size / (1024.0f * 1024.0f));
                }
                
                ModelFile->Close(ModelFile);
            }
            
            Root->Close(Root);
        }
    }
    
    Print(L"\r\n========================================\r\n");
    Print(L"  TEST COMPLETE!\r\n");
    Print(L"========================================\r\n");
    Print(L"\r\nPress any key to exit...\r\n");
    
    SystemTable->BootServices->WaitForEvent(1, &SystemTable->ConIn->WaitForKey, &Index);
    
    return EFI_SUCCESS;
}
