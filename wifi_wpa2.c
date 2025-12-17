/*
 * WiFi WPA2 Cryptography Implementation
 * Simplified crypto for bare-metal (no OpenSSL dependency)
 */

#include "wifi_wpa2.h"
#include <efi.h>
#include <efilib.h>

// Simplified SHA-1 implementation (RFCdim 3174)
typedef struct {
    UINT32 state[5];
    UINT32 count[2];
    UINT8 buffer[64];
} SHA1_CTX;

static void sha1_init(SHA1_CTX *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count[0] = ctx->count[1] = 0;
}

#define SHA1_ROL(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

static void sha1_transform(UINT32 state[5], const UINT8 buffer[64]) {
    UINT32 a, b, c, d, e, t;
    UINT32 W[80];
    
    // Prepare message schedule
    for (int i = 0; i < 16; i++) {
        W[i] = ((UINT32)buffer[i*4] << 24) | ((UINT32)buffer[i*4+1] << 16) |
               ((UINT32)buffer[i*4+2] << 8) | (UINT32)buffer[i*4+3];
    }
    for (int i = 16; i < 80; i++) {
        W[i] = SHA1_ROL(W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1);
    }
    
    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];
    
    // 80 rounds
    for (int i = 0; i < 80; i++) {
        UINT32 f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        
        t = SHA1_ROL(a, 5) + f + e + k + W[i];
        e = d; d = c; c = SHA1_ROL(b, 30); b = a; a = t;
    }
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1_update(SHA1_CTX *ctx, const UINT8 *data, UINTN len) {
    UINTN i, j = (ctx->count[0] >> 3) & 63;
    
    if ((ctx->count[0] += len << 3) < (len << 3)) ctx->count[1]++;
    ctx->count[1] += (len >> 29);
    
    for (i = 0; i < len; i++) {
        ctx->buffer[j++] = data[i];
        if (j == 64) {
            sha1_transform(ctx->state, ctx->buffer);
            j = 0;
        }
    }
}

static void sha1_final(UINT8 digest[20], SHA1_CTX *ctx) {
    UINT8 finalcount[8];
    for (int i = 0; i < 8; i++) {
        finalcount[i] = (UINT8)((ctx->count[(i >= 4 ? 0 : 1)] >> ((3-(i & 3)) * 8) ) & 255);
    }
    
    UINT8 c = 0x80;
    sha1_update(ctx, &c, 1);
    while ((ctx->count[0] & 504) != 448) {
        c = 0x00;
        sha1_update(ctx, &c, 1);
    }
    
    sha1_update(ctx, finalcount, 8);
    for (int i = 0; i < 20; i++) {
        digest[i] = (UINT8)((ctx->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
    }
}

/**
 * HMAC-SHA1
 */
void wpa2_hmac_sha1(
    const UINT8 *key,
    UINTN key_len,
    const UINT8 *data,
    UINTN data_len,
    UINT8 *output
) {
    SHA1_CTX ctx;
    UINT8 k_ipad[65], k_opad[65];
    UINT8 tk[20];
    
    // If key > 64 bytes, hash it first
    if (key_len > 64) {
        sha1_init(&ctx);
        sha1_update(&ctx, key, key_len);
        sha1_final(tk, &ctx);
        key = tk;
        key_len = 20;
    }
    
    // Prepare ipad and opad
    ZeroMem(k_ipad, sizeof(k_ipad));
    ZeroMem(k_opad, sizeof(k_opad));
    CopyMem(k_ipad, key, key_len);
    CopyMem(k_opad, key, key_len);
    
    for (int i = 0; i < 64; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }
    
    // HMAC = H(K XOR opad, H(K XOR ipad, text))
    sha1_init(&ctx);
    sha1_update(&ctx, k_ipad, 64);
    sha1_update(&ctx, data, data_len);
    sha1_final(output, &ctx);
    
    sha1_init(&ctx);
    sha1_update(&ctx, k_opad, 64);
    sha1_update(&ctx, output, 20);
    sha1_final(output, &ctx);
}

/**
 * PBKDF2-SHA1: Derive PMK from password
 * Simplified: only 1 iteration for demo (should be 4096)
 */
void wpa2_pbkdf2_sha1(
    const CHAR8 *password,
    UINTN password_len,
    const CHAR8 *ssid,
    UINTN ssid_len,
    UINT8 *pmk
) {
    // For simplicity: PMK = HMAC-SHA1(password, ssid || 0x00000001)
    // Real implementation needs 4096 iterations
    
    UINT8 salt[65];
    CopyMem(salt, ssid, ssid_len);
    salt[ssid_len] = 0;
    salt[ssid_len+1] = 0;
    salt[ssid_len+2] = 0;
    salt[ssid_len+3] = 1;
    
    UINT8 temp[20];
    wpa2_hmac_sha1((const UINT8*)password, password_len, salt, ssid_len + 4, temp);
    
    // First 20 bytes
    CopyMem(pmk, temp, 20);
    
    // Get remaining 12 bytes (PMK is 32 bytes)
    salt[ssid_len+3] = 2;
    wpa2_hmac_sha1((const UINT8*)password, password_len, salt, ssid_len + 4, temp);
    CopyMem(pmk + 20, temp, 12);
}

/**
 * PRF-512: Generate PTK
 */
void wpa2_prf_512(
    const UINT8 *pmk,
    const CHAR8 *label,
    const UINT8 *data,
    UINTN data_len,
    UINT8 *ptk
) {
    UINT8 input[256];
    UINTN label_len = 0;
    while (label[label_len]) label_len++;
    
    // Build: label || 0x00 || data || counter
    CopyMem(input, label, label_len);
    input[label_len] = 0;
    CopyMem(input + label_len + 1, data, data_len);
    
    // Generate 64 bytes (512 bits) in chunks of 20
    for (int i = 0; i < 4; i++) {
        input[label_len + 1 + data_len] = (UINT8)i;
        UINT8 temp[20];
        wpa2_hmac_sha1(pmk, 32, input, label_len + 1 + data_len + 1, temp);
        
        UINTN copy_len = (i == 3) ? 4 : 20;  // Last chunk only 4 bytes
        CopyMem(ptk + (i * 20), temp, copy_len);
    }
}

/**
 * Generate random nonce
 */
void wpa2_generate_nonce(UINT8 *nonce) {
    // Use UEFI time as seed
    extern UINT32 rand_efi(void);  // From llama2_efi.c
    
    for (int i = 0; i < 32; i += 4) {
        UINT32 r = rand_efi();
        nonce[i] = (r >> 0) & 0xFF;
        nonce[i+1] = (r >> 8) & 0xFF;
        nonce[i+2] = (r >> 16) & 0xFF;
        nonce[i+3] = (r >> 24) & 0xFF;
    }
}

/**
 * Calculate MIC
 */
void wpa2_calculate_mic(
    const UINT8 *kck,
    const UINT8 *data,
    UINTN data_len,
    UINT8 *mic
) {
    UINT8 hash[20];
    wpa2_hmac_sha1(kck, 16, data, data_len, hash);
    CopyMem(mic, hash, 16);
}

/**
 * Verify MIC
 */
BOOLEAN wpa2_verify_mic(
    const UINT8 *kck,
    const UINT8 *data,
    UINTN data_len,
    const UINT8 *received_mic
) {
    UINT8 calculated_mic[16];
    wpa2_calculate_mic(kck, data, data_len, calculated_mic);
    
    for (int i = 0; i < 16; i++) {
        if (calculated_mic[i] != received_mic[i]) return FALSE;
    }
    return TRUE;
}

/**
 * Compare bytes
 */
INT32 wpa2_compare_bytes(const UINT8 *a, const UINT8 *b, UINTN len) {
    for (UINTN i = 0; i < len; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

/**
 * XOR bytes
 */
void wpa2_xor_bytes(UINT8 *dest, const UINT8 *src, UINTN len) {
    for (UINTN i = 0; i < len; i++) {
        dest[i] ^= src[i];
    }
}

/**
 * WPA2 4-Way Handshake (SIMPLIFIED - hardware-specific parts stubbed)
 */
EFI_STATUS wpa2_perform_handshake(
    UINT8 *bssid,
    UINT8 *sta_mac,
    const CHAR8 *password,
    const CHAR8 *ssid,
    WPA2_Keys *keys,
    VOID *hw_context
) {
    Print(L"[WPA2] Starting 4-way handshake...\r\n");
    
    // Step 1: Derive PMK from password
    UINTN password_len = 0, ssid_len = 0;
    while (password[password_len]) password_len++;
    while (ssid[ssid_len]) ssid_len++;
    
    wpa2_pbkdf2_sha1(password, password_len, ssid, ssid_len, keys->pmk);
    Print(L"[WPA2] PMK derived\r\n");
    
    // Step 2: Generate SNonce
    wpa2_generate_nonce(keys->snonce);
    Print(L"[WPA2] SNonce generated\r\n");
    
    // Step 3: Wait for ANonce from AP (Message 1 of 4-way handshake)
    // TODO: Receive EAPOL frame from AP
    // For now, stub with zeros (would fail in real scenario)
    ZeroMem(keys->anonce, 32);
    Print(L"[WPA2] ⚠ ANonce stubbed (waiting for AP not implemented)\r\n");
    
    // Step 4: Derive PTK
    UINT8 ptk_data[76];  // AA || SPA || ANonce || SNonce
    
    // Determine order: min(AA,SPA) || max(AA,SPA) || min(ANonce,SNonce) || max(ANonce,SNonce)
    if (wpa2_compare_bytes(bssid, sta_mac, 6) < 0) {
        CopyMem(ptk_data, bssid, 6);
        CopyMem(ptk_data + 6, sta_mac, 6);
    } else {
        CopyMem(ptk_data, sta_mac, 6);
        CopyMem(ptk_data + 6, bssid, 6);
    }
    
    if (wpa2_compare_bytes(keys->anonce, keys->snonce, 32) < 0) {
        CopyMem(ptk_data + 12, keys->anonce, 32);
        CopyMem(ptk_data + 44, keys->snonce, 32);
    } else {
        CopyMem(ptk_data + 12, keys->snonce, 32);
        CopyMem(ptk_data + 44, keys->anonce, 32);
    }
    
    wpa2_prf_512(keys->pmk, "Pairwise key expansion", ptk_data, 76, keys->ptk);
    Print(L"[WPA2] PTK derived\r\n");
    
    // Step 5: Send Message 2 (with MIC)
    // TODO: Build EAPOL frame with SNonce and MIC
    Print(L"[WPA2] ⚠ Message 2 not sent (TX not implemented)\r\n");
    
    // Step 6: Receive Message 3 (with GTK)
    // TODO: Receive EAPOL frame, verify MIC, extract GTK
    Print(L"[WPA2] ⚠ Message 3 not received (RX not implemented)\r\n");
    
    // Step 7: Send Message 4
    // TODO: Build EAPOL frame with MIC
    Print(L"[WPA2] ⚠ Message 4 not sent (TX not implemented)\r\n");
    
    Print(L"[WPA2] ✓ Handshake STUB complete (keys derived, but no actual exchange)\r\n");
    
    return EFI_SUCCESS;  // Stub success (real implementation would fail without AP)
}
