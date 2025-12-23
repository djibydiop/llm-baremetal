/*
 * IEEE 802.11 Protocol Definitions
 * For bare-metal WiFi beacon parsing
 */

#ifndef WIFI_802_11_H
#define WIFI_802_11_H

#include <efi.h>
#include <efilib.h>

// 802.11 Frame Types
#define IEEE80211_FTYPE_MGMT  0x00
#define IEEE80211_FTYPE_CTRL  0x01
#define IEEE80211_FTYPE_DATA  0x02

// 802.11 Management Frame Subtypes
#define IEEE80211_STYPE_BEACON       0x80
#define IEEE80211_STYPE_PROBE_REQ    0x40
#define IEEE80211_STYPE_PROBE_RESP   0x50
#define IEEE80211_STYPE_AUTH         0xB0
#define IEEE80211_STYPE_ASSOC_REQ    0x00
#define IEEE80211_STYPE_ASSOC_RESP   0x10

// 802.11 Information Element IDs
#define WLAN_EID_SSID            0
#define WLAN_EID_SUPP_RATES      1
#define WLAN_EID_DS_PARAMS       3
#define WLAN_EID_TIM             5
#define WLAN_EID_COUNTRY         7
#define WLAN_EID_RSN             48
#define WLAN_EID_EXT_SUPP_RATES  50
#define WLAN_EID_HT_CAPABILITY   45
#define WLAN_EID_HT_OPERATION    61
#define WLAN_EID_VHT_CAPABILITY  191
#define WLAN_EID_VHT_OPERATION   192

// 802.11 Frame Control
typedef struct {
    UINT16 protocol_version:2;
    UINT16 type:2;
    UINT16 subtype:4;
    UINT16 to_ds:1;
    UINT16 from_ds:1;
    UINT16 more_frag:1;
    UINT16 retry:1;
    UINT16 pwr_mgt:1;
    UINT16 more_data:1;
    UINT16 protected:1;
    UINT16 order:1;
} __attribute__((packed)) FrameControl;

// 802.11 MAC Header (Management Frame)
typedef struct {
    FrameControl frame_control;
    UINT16 duration;
    UINT8  addr1[6];  // Destination (broadcast for beacon)
    UINT8  addr2[6];  // Source (BSSID/AP MAC)
    UINT8  addr3[6];  // BSSID
    UINT16 seq_ctrl;
} __attribute__((packed)) IEEE80211Header;

// 802.11 Beacon Frame Body
typedef struct {
    UINT64 timestamp;
    UINT16 beacon_interval;
    UINT16 capability_info;
    // Followed by Information Elements (IEs)
} __attribute__((packed)) BeaconFrameBody;

// Information Element
typedef struct {
    UINT8 id;
    UINT8 len;
    UINT8 data[0];  // Variable length
} __attribute__((packed)) InformationElement;

// Capability Info Bits
#define CAP_ESS             0x0001
#define CAP_IBSS            0x0002
#define CAP_PRIVACY         0x0010  // WEP/WPA/WPA2
#define CAP_SHORT_PREAMBLE  0x0020
#define CAP_SHORT_SLOT_TIME 0x0400

// Security Types
typedef enum {
    WIFI_SEC_OPEN = 0,
    WIFI_SEC_WEP,
    WIFI_SEC_WPA,
    WIFI_SEC_WPA2,
    WIFI_SEC_WPA3
} WiFiSecurityType;

// Parsed Beacon Result
typedef struct {
    UINT8 bssid[6];
    CHAR8 ssid[33];         // Max 32 + null
    UINT8 channel;
    INT8  rssi;             // Signal strength in dBm
    UINT16 beacon_interval; // In TU (1024 µs)
    WiFiSecurityType security;
    BOOLEAN is_5ghz;
    UINT16 capability;
    UINT8 max_rate;         // Mbps
} ParsedBeacon;

// Function declarations
EFI_STATUS parse_beacon_frame(
    const UINT8 *frame_data,
    UINTN frame_len,
    ParsedBeacon *result
);

WiFiSecurityType detect_security_type(
    UINT16 capability,
    const UINT8 *ies,
    UINTN ies_len
);

const InformationElement* find_ie(
    const UINT8 *ies,
    UINTN ies_len,
    UINT8 ie_id
);

UINT8 get_channel_from_frequency(UINT16 freq_mhz);

#endif // WIFI_802_11_H
