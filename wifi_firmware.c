/*
 * Intel AX200 WiFi Firmware Loading - Week 2-4 Implementation
 */

#include "wifi_firmware.h"

// GUIDs for file system access (extern - defined in GNU-EFI)
extern EFI_GUID gEfiSimpleFileSystemProtocolGuid;
extern EFI_GUID gEfiFileInfoGuid;

/**
 * Load firmware file from disk
 * Week 2: File I/O implementation
 */
EFI_STATUS wifi_firmware_load_file(
    EFI_SYSTEM_TABLE *SystemTable,
    const CHAR16 *filename,
    FirmwareContext *fw_ctx
) {
    // Clear firmware context
    SetMem(fw_ctx, sizeof(FirmwareContext), 0);
    
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer = NULL;
    UINTN HandleCount = 0;
    
    // Locate all Simple File System handles
    Status = uefi_call_wrapper(
        SystemTable->BootServices->LocateHandleBuffer,
        5,
        ByProtocol,
        &gEfiSimpleFileSystemProtocolGuid,
        NULL,
        &HandleCount,
        &HandleBuffer
    );
    
    if (EFI_ERROR(Status) || HandleCount == 0) {
        return EFI_NOT_FOUND;
    }
    
    // Try each filesystem
    for (UINTN i = 0; i < HandleCount; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
        Status = uefi_call_wrapper(
            SystemTable->BootServices->HandleProtocol,
            3,
            HandleBuffer[i],
            &gEfiSimpleFileSystemProtocolGuid,
            (VOID**)&FileSystem
        );
        
        if (EFI_ERROR(Status)) continue;
        
        // Open root directory
        EFI_FILE_PROTOCOL *Root;
        Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
        if (EFI_ERROR(Status)) continue;
        
        // Try to open firmware file
        EFI_FILE_PROTOCOL *File;
        Status = uefi_call_wrapper(
            Root->Open,
            5,
            Root,
            &File,
            (CHAR16*)filename,
            EFI_FILE_MODE_READ,
            0
        );
        
        if (EFI_ERROR(Status)) {
            uefi_call_wrapper(Root->Close, 1, Root);
            continue;
        }
        
        // Get file size
        EFI_FILE_INFO *FileInfo;
        UINTN FileInfoSize = sizeof(EFI_FILE_INFO) + 512;
        FileInfo = AllocatePool(FileInfoSize);
        
        Status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
        if (EFI_ERROR(Status)) {
            FreePool(FileInfo);
            uefi_call_wrapper(File->Close, 1, File);
            uefi_call_wrapper(Root->Close, 1, Root);
            continue;
        }
        
        UINTN FileSize = FileInfo->FileSize;
        FreePool(FileInfo);
        
        // Allocate buffer
        UINT8 *Buffer = AllocatePool(FileSize);
        if (!Buffer) {
            uefi_call_wrapper(File->Close, 1, File);
            uefi_call_wrapper(Root->Close, 1, Root);
            continue;
        }
        
        // Read file
        Status = uefi_call_wrapper(File->Read, 3, File, &FileSize, Buffer);
        
        uefi_call_wrapper(File->Close, 1, File);
        uefi_call_wrapper(Root->Close, 1, Root);
        
        if (!EFI_ERROR(Status)) {
            fw_ctx->raw_data = Buffer;
            fw_ctx->raw_size = FileSize;
            FreePool(HandleBuffer);
            return EFI_SUCCESS;
        }
        
        FreePool(Buffer);
    }
    
    if (HandleBuffer) FreePool(HandleBuffer);
    return EFI_NOT_FOUND;
}

/**
 * Parse firmware .ucode file (TLV format)
 * Week 2: TLV parser implementation
 */
EFI_STATUS wifi_firmware_parse(
    FirmwareContext *fw_ctx,
    UINT8 *data,
    UINTN size
) {
    if (size < sizeof(FirmwareHeader)) {
        Print(L"[FIRMWARE] Error: File too small\r\n");
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"[FIRMWARE] Parsing firmware (size: %u bytes)...\r\n", size);
    
    FirmwareHeader *header = (FirmwareHeader*)data;
    
    // Verify magic number
    if (header->magic != FIRMWARE_MAGIC) {
        Print(L"[FIRMWARE] Error: Invalid magic 0x%08x (expected 0x%08x)\r\n",
              header->magic, FIRMWARE_MAGIC);
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"[FIRMWARE] Revision: %u, API: %u, Build: %u\r\n",
          header->revision, header->api_version, header->build);
    
    fw_ctx->revision = header->revision;
    fw_ctx->api_version = header->api_version;
    
    // TODO Week 2: Parse TLV sections
    // 1. Iterate through TLV entries
    // 2. Extract CODE section
    // 3. Extract DATA section
    // 4. Extract INIT section
    // 5. Store in fw_ctx
    
    Print(L"[FIRMWARE] ✓ Header parsed\r\n");
    Print(L"[FIRMWARE] Next: Parse TLV sections (CODE, DATA, INIT)\r\n");
    
    return EFI_SUCCESS;
}

/**
 * Upload firmware to device via DMA
 * Week 3: DMA transfer implementation
 */
EFI_STATUS wifi_firmware_upload(
    WiFiDevice *device,
    FirmwareContext *fw_ctx
) {
    if (device->state != WIFI_STATE_DETECTED) {
        return EFI_NOT_READY;
    }
    
    Print(L"[FIRMWARE] Uploading to device at BAR0: 0x%016lx...\r\n",
          device->bar0_address);
    
    device->state = WIFI_STATE_FIRMWARE_LOADING;
    
    // TODO Week 3: Implement DMA transfer
    // 1. Reset device
    // 2. Setup DMA buffers
    // 3. Upload CODE section
    // 4. Upload DATA section
    // 5. Upload INIT section
    // 6. Verify checksums
    
    Print(L"[FIRMWARE] ✗ DMA upload not yet implemented\r\n");
    Print(L"[FIRMWARE] Next: Implement device reset and DMA transfer\r\n");
    
    return EFI_NOT_READY;
}

/**
 * Start firmware execution
 * Week 4: Device initialization
 */
EFI_STATUS wifi_firmware_start(
    WiFiDevice *device
) {
    if (device->state != WIFI_STATE_FIRMWARE_LOADING) {
        return EFI_NOT_READY;
    }
    
    Print(L"[FIRMWARE] Starting firmware...\r\n");
    
    // TODO Week 4: Start firmware
    // 1. Write start command to device
    // 2. Trigger interrupt
    // 3. Wait for firmware ready
    
    Print(L"[FIRMWARE] ✗ Firmware start not yet implemented\r\n");
    
    return EFI_NOT_READY;
}

/**
 * Wait for firmware to become ready
 * Week 4: Polling implementation
 */
EFI_STATUS wifi_firmware_wait_ready(
    WiFiDevice *device,
    UINT32 timeout_ms
) {
    Print(L"[FIRMWARE] Waiting for firmware ready (timeout: %u ms)...\r\n",
          timeout_ms);
    
    // TODO Week 4: Poll device status
    // 1. Read status register
    // 2. Check firmware ready bit
    // 3. Timeout after N milliseconds
    
    Print(L"[FIRMWARE] ✗ Status polling not yet implemented\r\n");
    
    return EFI_NOT_READY;
}

/**
 * Free firmware context
 */
void wifi_firmware_free(
    FirmwareContext *fw_ctx
) {
    if (fw_ctx->raw_data) {
        FreePool(fw_ctx->raw_data);
        fw_ctx->raw_data = NULL;
    }
    
    if (fw_ctx->code_section.data) {
        FreePool(fw_ctx->code_section.data);
    }
    
    if (fw_ctx->data_section.data) {
        FreePool(fw_ctx->data_section.data);
    }
    
    if (fw_ctx->init_section.data) {
        FreePool(fw_ctx->init_section.data);
    }
    
    SetMem(fw_ctx, sizeof(FirmwareContext), 0);
}

/**
 * Test firmware loading (for Week 2 development)
 */
EFI_STATUS wifi_firmware_test_load(
    EFI_SYSTEM_TABLE *SystemTable,
    WiFiDevice *device
) {
    // Firmware loading silencieux
    FirmwareContext fw_ctx;
    EFI_STATUS status;
    
    status = wifi_firmware_load_file(SystemTable, L"iwlwifi-cc-a0-72.ucode", &fw_ctx);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    status = wifi_firmware_parse(&fw_ctx, fw_ctx.raw_data, fw_ctx.raw_size);
    if (EFI_ERROR(status)) {
        wifi_firmware_free(&fw_ctx);
        return status;
    }
    
    status = wifi_firmware_upload(device, &fw_ctx);
    if (EFI_ERROR(status)) {
        wifi_firmware_free(&fw_ctx);
        return status;
    }
    
    status = wifi_firmware_start(device);
    if (EFI_ERROR(status)) {
        wifi_firmware_free(&fw_ctx);
        return status;
    }
    
    status = wifi_firmware_wait_ready(device, 5000);
    if (EFI_ERROR(status)) {
        wifi_firmware_free(&fw_ctx);
        return status;
    }
    
    device->state = WIFI_STATE_FIRMWARE_LOADED;
    
    wifi_firmware_free(&fw_ctx);
    return EFI_SUCCESS;
}
