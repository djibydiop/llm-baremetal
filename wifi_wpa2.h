/*
 * WiFi WPA2 Cryptography for Bare-Metal UEFI
 * 
 * Implements WPA2-PSK 4-way handshake and crypto operations
 * Pure C implementation - no external dependencies
 * 
 * Author: llm-baremetal project
 * License: MIT
 */

#ifndef WIFI_WPA2_H
#define WIFI_WPA2_H

#include <efi.h>
#include <efilib.h>

// WPA2 constants
#define WPA2_PMK_LEN 32      // Pre-Master Key (256 bits)
#define WPA2_PTK_LEN 64      // Pairwise Transient Key (512 bits)
#define WPA2_GTK_LEN 32      // Group Temporal Key (256 bits)
#define WPA2_NONCE_LEN 32    // Nonce (256 bits)
#define WPA2_MIC_LEN 16      // Message Integrity Code (128 bits)

// WPA2 Key structure
typedef struct {
    UINT8 pmk[WPA2_PMK_LEN];     // From PBKDF2(password, ssid)
    UINT8 ptk[WPA2_PTK_LEN];     // From PRF(PMK, nonces, MACs)
    UINT8 gtk[WPA2_GTK_LEN];     // From AP (encrypted)
    UINT8 anonce[WPA2_NONCE_LEN]; // AP nonce
    UINT8 snonce[WPA2_NONCE_LEN]; // Station nonce
    UINT8 mic[WPA2_MIC_LEN];      // Message integrity check
} WPA2_Keys;

// EAPOL frame types
#define EAPOL_KEY 3

// EAPOL-Key frame structure (simplified)
typedef struct {
    UINT8 protocol_version;
    UINT8 packet_type;
    UINT16 body_length;
    UINT8 descriptor_type;
    UINT16 key_info;
    UINT16 key_length;
    UINT8 replay_counter[8];
    UINT8 nonce[32];
    UINT8 key_iv[16];
    UINT8 key_rsc[8];
    UINT8 reserved[8];
    UINT8 mic[16];
    UINT16 key_data_length;
    // Key data follows...
} __attribute__((packed)) EAPOL_Key_Frame;

// 802.11 authentication frame
typedef struct {
    UINT16 frame_control;
    UINT16 duration;
    UINT8 da[6];  // Destination (AP)
    UINT8 sa[6];  // Source (Station)
    UINT8 bssid[6];
    UINT16 seq_ctrl;
    UINT16 auth_algorithm;  // 0=Open, 1=Shared
    UINT16 auth_seq;        // 1 or 2
    UINT16 status_code;
} __attribute__((packed)) Auth_Frame;

// 802.11 association request frame
typedef struct {
    UINT16 frame_control;
    UINT16 duration;
    UINT8 da[6];
    UINT8 sa[6];
    UINT8 bssid[6];
    UINT16 seq_ctrl;
    UINT16 capability;
    UINT16 listen_interval;
    // IEs follow (SSID, supported rates, etc.)
} __attribute__((packed)) Assoc_Request_Frame;

// === Crypto Functions ===

/**
 * PBKDF2-SHA1: Derive PMK from password and SSID
 * PMK = PBKDF2(password, ssid, 4096 iterations, 256 bits)
 */
void wpa2_pbkdf2_sha1(
    const CHAR8 *password,
    UINTN password_len,
    const CHAR8 *ssid,
    UINTN ssid_len,
    UINT8 *pmk
);

/**
 * PRF-512: Pseudo-Random Function for PTK derivation
 * PTK = PRF-512(PMK, "Pairwise key expansion", 
 *               min(AA,SPA) || max(AA,SPA) || min(ANonce,SNonce) || max(ANonce,SNonce))
 */
void wpa2_prf_512(
    const UINT8 *pmk,
    const CHAR8 *label,
    const UINT8 *data,
    UINTN data_len,
    UINT8 *ptk
);

/**
 * HMAC-SHA1: For MIC calculation
 */
void wpa2_hmac_sha1(
    const UINT8 *key,
    UINTN key_len,
    const UINT8 *data,
    UINTN data_len,
    UINT8 *output
);

/**
 * Generate random nonce (SNonce)
 */
void wpa2_generate_nonce(
    UINT8 *nonce
);

/**
 * Calculate MIC for EAPOL frame
 */
void wpa2_calculate_mic(
    const UINT8 *kck,  // Key Confirmation Key (first 16 bytes of PTK)
    const UINT8 *data,
    UINTN data_len,
    UINT8 *mic
);

/**
 * Verify MIC from AP
 */
BOOLEAN wpa2_verify_mic(
    const UINT8 *kck,
    const UINT8 *data,
    UINTN data_len,
    const UINT8 *received_mic
);

// === 4-Way Handshake ===

/**
 * Complete WPA2 4-way handshake
 * Returns EFI_SUCCESS if handshake completed
 */
EFI_STATUS wpa2_perform_handshake(
    UINT8 *bssid,           // AP MAC address
    UINT8 *sta_mac,         // Station MAC address
    const CHAR8 *password,
    const CHAR8 *ssid,
    WPA2_Keys *keys,
    VOID *hw_context        // Hardware-specific context
);

// === Helper Functions ===

/**
 * Compare two MACs/nonces to determine min/max for PTK derivation
 */
INT32 wpa2_compare_bytes(const UINT8 *a, const UINT8 *b, UINTN len);

/**
 * XOR two byte arrays
 */
void wpa2_xor_bytes(UINT8 *dest, const UINT8 *src, UINTN len);

#endif // WIFI_WPA2_H
