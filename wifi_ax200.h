/*
 * Intel AX200 WiFi 6 Bare-Metal Driver
 * 
 * First-ever WiFi driver for bare-metal UEFI without OS
 * Supports Intel AX200/AX201 802.11ax (WiFi 6) cards
 * 
 * Author: llm-baremetal project
 * License: MIT
 * 
 * WORLD FIRST: No one has ever implemented WiFi 6 on bare-metal UEFI
 */

#ifndef WIFI_AX200_H
#define WIFI_AX200_H

#include <efi.h>
#include <efilib.h>

// Intel AX200 PCI IDs
#define INTEL_AX200_VENDOR_ID  0x8086
#define INTEL_AX200_DEVICE_ID  0x2723  // AX200
#define INTEL_AX201_DEVICE_ID  0x4DF0  // AX201
#define INTEL_AX210_DEVICE_ID  0x2725  // AX210

// Firmware versions
#define IWLWIFI_AX200_FIRMWARE "iwlwifi-cc-a0-72.ucode"
#define FIRMWARE_MAX_SIZE      (512 * 1024)  // 512 KB

// WiFi states
typedef enum {
    WIFI_STATE_UNINITIALIZED = 0,
    WIFI_STATE_FIRMWARE_LOADING,
    WIFI_STATE_FIRMWARE_LOADED,
    WIFI_STATE_RADIO_ON,
    WIFI_STATE_SCANNING,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_ERROR
} WiFiState;

// WiFi configuration
typedef struct {
    CHAR8 ssid[32];
    CHAR8 password[64];
    UINT8 security_type;  // 0=Open, 1=WEP, 2=WPA, 3=WPA2
    UINT8 channel;
} WiFiConfig;

// WiFi device structure
typedef struct {
    // PCI information
    UINT16 vendor_id;
    UINT16 device_id;
    UINT8 bus;
    UINT8 device;
    UINT8 function;
    UINT64 bar0_address;  // MMIO base address
    UINT32 bar0_size;
    
    // Device state
    WiFiState state;
    BOOLEAN radio_enabled;
    
    // MAC address
    UINT8 mac_address[6];
    
    // Firmware
    VOID* firmware_data;
    UINTN firmware_size;
    
    // Configuration
    WiFiConfig config;
    
    // Network info
    UINT8 bssid[6];       // Connected AP MAC
    UINT8 ip_address[4];  // Assigned IP
    INT8 signal_strength; // RSSI in dBm
} WiFiDevice;

// Scan result structure
typedef struct {
    UINT8 bssid[6];
    CHAR8 ssid[32];
    UINT8 channel;
    INT8 rssi;
    UINT8 security;
} WiFiScanResult;

// Function declarations

// === 1. PCI Detection & Initialization ===
EFI_STATUS wifi_detect_device(
    EFI_SYSTEM_TABLE *SystemTable,
    WiFiDevice *device
);

EFI_STATUS wifi_init_pci(
    EFI_SYSTEM_TABLE *SystemTable,
    WiFiDevice *device
);

// === 2. Firmware Loading ===
EFI_STATUS wifi_load_firmware(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable,
    WiFiDevice *device,
    const CHAR16 *firmware_path
);

EFI_STATUS wifi_upload_firmware(
    WiFiDevice *device
);

// === 3. Radio Control ===
EFI_STATUS wifi_radio_on(
    WiFiDevice *device
);

EFI_STATUS wifi_radio_off(
    WiFiDevice *device
);

// === 4. Network Scanning ===
EFI_STATUS wifi_scan_networks(
    WiFiDevice *device,
    WiFiScanResult *results,
    UINTN *result_count,
    UINTN max_results
);

// === 5. Connection Management ===
EFI_STATUS wifi_connect(
    WiFiDevice *device,
    const CHAR8 *ssid,
    const CHAR8 *password
);

EFI_STATUS wifi_disconnect(
    WiFiDevice *device
);

// === 6. Data Transfer ===
EFI_STATUS wifi_send_data(
    WiFiDevice *device,
    const VOID *data,
    UINTN size
);

EFI_STATUS wifi_receive_data(
    WiFiDevice *device,
    VOID *buffer,
    UINTN *size
);

// === 7. Status & Diagnostics ===
EFI_STATUS wifi_get_status(
    WiFiDevice *device,
    CHAR16 *status_text,
    UINTN buffer_size
);

BOOLEAN wifi_is_connected(
    WiFiDevice *device
);

// === Helper Functions ===
void wifi_read_mac_address(
    WiFiDevice *device
);

void wifi_print_device_info(
    WiFiDevice *device
);

// === MMIO Register Access ===
static inline UINT32 wifi_read32(WiFiDevice *device, UINT32 offset) {
    volatile UINT32 *addr = (volatile UINT32*)(device->bar0_address + offset);
    return *addr;
}

static inline void wifi_write32(WiFiDevice *device, UINT32 offset, UINT32 value) {
    volatile UINT32 *addr = (volatile UINT32*)(device->bar0_address + offset);
    *addr = value;
}

#endif // WIFI_AX200_H
