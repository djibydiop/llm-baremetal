/*
 * Intel AX200 WiFi Firmware Loading
 * Week 2-4: Firmware parsing, upload, and initialization
 */

#ifndef WIFI_FIRMWARE_H
#define WIFI_FIRMWARE_H

#include <efi.h>
#include <efilib.h>
#include "wifi_ax200.h"

// Intel WiFi firmware format constants
#define FIRMWARE_MAGIC          0x0A4C5749  // "IWL\n"
#define FIRMWARE_MAX_SIZE       (512 * 1024) // 512 KB
#define FIRMWARE_SECTION_CODE   1
#define FIRMWARE_SECTION_DATA   2
#define FIRMWARE_SECTION_INIT   3

// TLV (Type-Length-Value) format
typedef struct {
    UINT32 type;
    UINT32 length;
    UINT8 data[];
} FirmwareTLV;

// Firmware file header
typedef struct {
    UINT32 magic;           // 0x0A4C5749 ("IWL\n")
    UINT32 revision;        // Firmware revision
    UINT32 api_version;     // API version
    UINT32 build;           // Build number
    UINT64 tlv_offset;      // Offset to TLV data
} FirmwareHeader;

// Firmware section
typedef struct {
    UINT32 type;           // Section type (CODE, DATA, INIT)
    UINT32 size;           // Section size in bytes
    UINT64 offset;         // Offset in firmware file
    VOID* data;            // Loaded section data
} FirmwareSection;

// Complete firmware context
typedef struct {
    BOOLEAN loaded;
    UINT32 revision;
    UINT32 api_version;
    
    // Sections
    FirmwareSection code_section;
    FirmwareSection data_section;
    FirmwareSection init_section;
    
    // Raw firmware data
    VOID* raw_data;
    UINTN raw_size;
} FirmwareContext;

// Function prototypes
EFI_STATUS wifi_firmware_load_file(
    EFI_SYSTEM_TABLE *SystemTable,
    const CHAR16 *filename,
    FirmwareContext *fw_ctx
);

EFI_STATUS wifi_firmware_parse(
    FirmwareContext *fw_ctx,
    UINT8 *data,
    UINTN size
);

EFI_STATUS wifi_firmware_upload(
    WiFiDevice *device,
    FirmwareContext *fw_ctx
);

EFI_STATUS wifi_firmware_start(
    WiFiDevice *device
);

EFI_STATUS wifi_firmware_wait_ready(
    WiFiDevice *device,
    UINT32 timeout_ms
);

void wifi_firmware_free(
    FirmwareContext *fw_ctx
);

#endif // WIFI_FIRMWARE_H
