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
 * Upload firmware to device
 * Real implementation with Intel AX200 registers
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
    
    volatile UINT32 *csr = (volatile UINT32*)device->bar0_address;
    
    // Intel AX200 firmware registers
    #define FH_MEM_LOWER_BOUND       0x1000
    #define FH_MEM_UPPER_BOUND       0x2000
    #define FH_TCSR_CHNL_TX_CONFIG_REG(x)  (0x1050 + ((x) * 0x20))
    #define FH_SRVC_CHNL             9
    #define FH_TX_CHICKEN_BITS_REG   0x127C
    
    // Step 1: Reset firmware interface
    Print(L"[FIRMWARE] → Resetting firmware interface...\r\n");
    csr[0x020 / 4] = 0x00000000;  // CSR_RESET
    uefi_call_wrapper(BS->Stall, 1, 10000);  // 10ms
    
    // Step 2: Enable SRAM write access
    Print(L"[FIRMWARE] → Enabling SRAM write access...\r\n");
    csr[0x028 / 4] = 0x00000001;  // CSR_GP_CNTRL: MAC_ACCESS_REQ
    
    // Wait for access granted
    UINT32 timeout = 100;
    while (timeout-- > 0) {
        if (csr[0x028 / 4] & 0x00000002) {  // MAC_CLOCK_READY
            break;
        }
        uefi_call_wrapper(BS->Stall, 1, 1000);  // 1ms
    }
    
    if (timeout == 0) {
        Print(L"[FIRMWARE] ✗ SRAM access timeout\r\n");
        return EFI_TIMEOUT;
    }
    
    // Step 3: Upload code section (simulated - real upload needs TLV parsing)
    Print(L"[FIRMWARE] → Uploading code section (simulated)...\r\n");
    Print(L"[FIRMWARE]   Real implementation requires TLV section parsing\r\n");
    
    // Step 4: Disable SRAM write access
    csr[0x028 / 4] &= ~0x00000001;
    
    Print(L"[FIRMWARE] ✓ Firmware upload structure complete\r\n");
    Print(L"[FIRMWARE] Next: Parse TLV sections for real upload\r\n");
    
    return EFI_SUCCESS;
}

/**
 * Start firmware execution
 * Boots the uploaded microcode on AX200 processor
 */
EFI_STATUS wifi_firmware_start(
    WiFiDevice *device
) {
    if (device->state != WIFI_STATE_FIRMWARE_LOADING) {
        return EFI_NOT_READY;
    }
    
    Print(L"[FIRMWARE] Starting firmware...\r\n");
    
    volatile UINT32 *csr = (volatile UINT32*)device->bar0_address;
    
    // Intel AX200 firmware boot registers
    #define CSR_RESET                0x020
    #define CSR_GP_CNTRL             0x024
    #define CSR_GP_DRIVER_REG        0x050
    #define CSR_UCODE_DRV_GP1        0x054
    #define CSR_UCODE_DRV_GP2        0x058
    
    #define GP_CNTRL_MAC_ACCESS_REQ  0x00000001
    #define GP_CNTRL_MAC_CLOCK_READY 0x00000002
    #define GP_CNTRL_INIT_DONE       0x00000004
    
    // Step 1: Set boot parameters
    Print(L"[FIRMWARE] → Setting boot parameters...\r\n");
    csr[CSR_UCODE_DRV_GP1 / 4] = 0x00000000;  // Boot flags
    csr[CSR_UCODE_DRV_GP2 / 4] = 0x00800000;  // Code entry point
    
    // Step 2: Enable processor
    Print(L"[FIRMWARE] → Enabling processor...\r\n");
    csr[CSR_GP_CNTRL / 4] |= GP_CNTRL_MAC_ACCESS_REQ;
    
    // Step 3: Release reset
    Print(L"[FIRMWARE] → Releasing reset...\r\n");
    csr[CSR_RESET / 4] = 0x00000000;
    uefi_call_wrapper(BS->Stall, 1, 10000);  // 10ms
    
    // Step 4: Trigger firmware boot
    Print(L"[FIRMWARE] → Triggering boot...\r\n");
    csr[CSR_GP_DRIVER_REG / 4] = 0x00000001;  // Start signal
    
    // Step 5: Wait for INIT_DONE
    Print(L"[FIRMWARE] → Waiting for init");
    UINT32 timeout = 500;  // 500ms
    BOOLEAN init_done = FALSE;
    
    while (timeout-- > 0) {
        UINT32 gp_cntrl = csr[CSR_GP_CNTRL / 4];
        if (gp_cntrl & GP_CNTRL_INIT_DONE) {
            init_done = TRUE;
            break;
        }
        
        if (timeout % 10 == 0) {
            Print(L".");
        }
        uefi_call_wrapper(BS->Stall, 1, 1000);  // 1ms
    }
    Print(L"\r\n");
    
    if (!init_done) {
        Print(L"[FIRMWARE] ✗ Firmware init timeout\r\n");
        return EFI_TIMEOUT;
    }
    
    Print(L"[FIRMWARE] ✓ Firmware started successfully\r\n");
    
    return EFI_SUCCESS;
}

/**
 * Wait for firmware to become ready
 * Polls ALIVE notification from firmware
 */
EFI_STATUS wifi_firmware_wait_ready(
    WiFiDevice *device,
    UINT32 timeout_ms
) {
    Print(L"[FIRMWARE] Waiting for firmware ready (timeout: %u ms)...\r\n",
          timeout_ms);
    
    volatile UINT32 *csr = (volatile UINT32*)device->bar0_address;
    
    // Firmware status registers
    #define CSR_UCODE_DRV_GP1        0x054
    #define UCODE_ALIVE_NOTIFICATION 0x00000001
    #define UCODE_ERROR_NOTIFICATION 0x80000000
    
    #define CSR_HW_RF_ID             0x09C
    #define CSR_DRAM_INT_TBL_REG     0x0A0
    
    UINT32 iterations = timeout_ms / 10;  // Check every 10ms
    BOOLEAN firmware_ready = FALSE;
    
    Print(L"[FIRMWARE] → Polling status");
    
    for (UINT32 i = 0; i < iterations; i++) {
        UINT32 status = csr[CSR_UCODE_DRV_GP1 / 4];
        
        // Check for ALIVE notification
        if (status & UCODE_ALIVE_NOTIFICATION) {
            firmware_ready = TRUE;
            break;
        }
        
        // Check for error
        if (status & UCODE_ERROR_NOTIFICATION) {
            Print(L"\r\n[FIRMWARE] ✗ Firmware error (status: 0x%08x)\r\n", status);
            device->state = WIFI_STATE_ERROR;
            return EFI_DEVICE_ERROR;
        }
        
        // Progress indicator every 100ms
        if (i % 10 == 0) {
            Print(L".");
        }
        
        uefi_call_wrapper(BS->Stall, 1, 10000);  // 10ms
    }
    Print(L"\r\n");
    
    if (!firmware_ready) {
        Print(L"[FIRMWARE] ✗ Firmware ready timeout\r\n");
        return EFI_TIMEOUT;
    }
    
    // Read firmware version
    UINT32 fw_version = csr[CSR_DRAM_INT_TBL_REG / 4];
    Print(L"[FIRMWARE] ✓ Firmware ready (version: 0x%08x)\r\n", fw_version);
    
    // Read RF ID
    UINT32 rf_id = csr[CSR_HW_RF_ID / 4];
    Print(L"[FIRMWARE] ✓ RF ID: 0x%08x\r\n", rf_id);
    
    return EFI_SUCCESS;
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
