/*
 * IEEE 802.11 Protocol Parsing
 * Beacon frame parsing for WiFi scan results
 */

#include "wifi_802_11.h"

/**
 * Find Information Element by ID
 */
const InformationElement* find_ie(
    const UINT8 *ies,
    UINTN ies_len,
    UINT8 ie_id
) {
    UINTN offset = 0;
    
    while (offset + 2 <= ies_len) {
        const InformationElement *ie = (const InformationElement*)(ies + offset);
        
        if (offset + 2 + ie->len > ies_len) {
            break;  // Malformed IE
        }
        
        if (ie->id == ie_id) {
            return ie;
        }
        
        offset += 2 + ie->len;
    }
    
    return NULL;
}

/**
 * Detect security type from capability and RSN IE
 */
WiFiSecurityType detect_security_type(
    UINT16 capability,
    const UINT8 *ies,
    UINTN ies_len
) {
    // Check if privacy bit is set
    if (!(capability & CAP_PRIVACY)) {
        return WIFI_SEC_OPEN;
    }
    
    // Look for RSN (WPA2/WPA3) IE
    const InformationElement *rsn_ie = find_ie(ies, ies_len, WLAN_EID_RSN);
    if (rsn_ie && rsn_ie->len >= 2) {
        // Check RSN version
        UINT16 rsn_version = *(UINT16*)rsn_ie->data;
        
        // Check for WPA3 (SAE)
        if (rsn_ie->len >= 6) {
            UINT8 akm_suite = rsn_ie->data[5];
            if (akm_suite == 0x08) {  // SAE
                return WIFI_SEC_WPA3;
            }
        }
        
        return WIFI_SEC_WPA2;
    }
    
    // Look for WPA IE (vendor-specific)
    // TODO: Parse vendor-specific IEs for WPA1
    
    // If privacy set but no RSN, assume WEP
    return WIFI_SEC_WEP;
}

/**
 * Convert frequency (MHz) to channel number
 */
UINT8 get_channel_from_frequency(UINT16 freq_mhz) {
    // 2.4 GHz band
    if (freq_mhz >= 2412 && freq_mhz <= 2484) {
        if (freq_mhz == 2484) {
            return 14;  // Channel 14 (Japan only)
        }
        return (freq_mhz - 2407) / 5;
    }
    
    // 5 GHz band
    if (freq_mhz >= 5170 && freq_mhz <= 5825) {
        return (freq_mhz - 5000) / 5;
    }
    
    return 0;  // Unknown
}

/**
 * Parse 802.11 beacon frame
 */
EFI_STATUS parse_beacon_frame(
    const UINT8 *frame_data,
    UINTN frame_len,
    ParsedBeacon *result
) {
    if (!frame_data || !result || frame_len < sizeof(IEEE80211Header) + sizeof(BeaconFrameBody)) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Clear result
    SetMem(result, sizeof(ParsedBeacon), 0);
    
    // Parse MAC header
    const IEEE80211Header *hdr = (const IEEE80211Header*)frame_data;
    
    // Check frame type (must be management beacon)
    if (hdr->frame_control.type != IEEE80211_FTYPE_MGMT ||
        hdr->frame_control.subtype != (IEEE80211_STYPE_BEACON >> 4)) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Extract BSSID (addr2 = source = AP MAC)
    CopyMem(result->bssid, hdr->addr2, 6);
    
    // Parse beacon body
    const BeaconFrameBody *beacon = (const BeaconFrameBody*)(frame_data + sizeof(IEEE80211Header));
    result->beacon_interval = beacon->beacon_interval;
    result->capability = beacon->capability_info;
    
    // Parse Information Elements
    const UINT8 *ies = frame_data + sizeof(IEEE80211Header) + sizeof(BeaconFrameBody);
    UINTN ies_len = frame_len - sizeof(IEEE80211Header) - sizeof(BeaconFrameBody);
    
    // Extract SSID
    const InformationElement *ssid_ie = find_ie(ies, ies_len, WLAN_EID_SSID);
    if (ssid_ie && ssid_ie->len > 0 && ssid_ie->len <= 32) {
        CopyMem(result->ssid, ssid_ie->data, ssid_ie->len);
        result->ssid[ssid_ie->len] = 0;  // Null terminate
    }
    
    // Extract channel from DS Parameter Set
    const InformationElement *ds_ie = find_ie(ies, ies_len, WLAN_EID_DS_PARAMS);
    if (ds_ie && ds_ie->len >= 1) {
        result->channel = ds_ie->data[0];
    }
    
    // Detect 5GHz band (check for VHT capability)
    const InformationElement *vht_ie = find_ie(ies, ies_len, WLAN_EID_VHT_CAPABILITY);
    result->is_5ghz = (vht_ie != NULL);
    
    // Detect security type
    result->security = detect_security_type(beacon->capability_info, ies, ies_len);
    
    // Extract max supported rate
    const InformationElement *rates_ie = find_ie(ies, ies_len, WLAN_EID_SUPP_RATES);
    if (rates_ie && rates_ie->len > 0) {
        UINT8 max_rate = 0;
        for (UINT8 i = 0; i < rates_ie->len; i++) {
            UINT8 rate = rates_ie->data[i] & 0x7F;  // Strip basic rate bit
            if (rate > max_rate) {
                max_rate = rate;
            }
        }
        result->max_rate = (max_rate * 500) / 1000;  // Convert to Mbps
    }
    
    // TODO: Extract RSSI from PHY metadata (driver-specific)
    result->rssi = -60;  // Placeholder
    
    return EFI_SUCCESS;
}
