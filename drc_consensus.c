/*
 * DRC Network Consensus - Implementation
 * Made in Senegal 🇸🇳
 */

#include "drc_consensus.h"
#include <efi.h>
#include <efilib.h>

// Validator endpoints (to be changed for production)
static const CHAR8* validator_urls[MAX_VALIDATORS] = {
    "http://192.168.1.100:5000/validate",  // Change to your validator IPs
    "http://192.168.1.100:5001/validate",
    "http://192.168.1.100:5002/validate"
};

// HTTP POST helper (simplified - real implementation with EFI_HTTP_PROTOCOL to come)
static BOOLEAN send_http_post(const CHAR8* url, const CHAR8* json_payload, CHAR8* response_buffer, UINT32 buffer_size) {
    // TODO: Implement with EFI_HTTP_PROTOCOL when Ethernet available
    // For now: return FALSE to use simulation mode
    return FALSE;
}

EFI_STATUS drc_request_consensus(SystemState* state, ConsensusResult* result) {
    if (!state || !result) return EFI_INVALID_PARAMETER;
    
    result->approvals = 0;
    result->rejections = 0;
    result->approved = FALSE;
    
    Print(L"\r\n[DRC CONSENSUS] Querying %d validators...\r\n", MAX_VALIDATORS);
    
    // Prepare JSON payload
    CHAR8 json_payload[512];
    AsciiSPrint(json_payload, sizeof(json_payload),
                "{\"failed_boots\": %u, \"model_name\": \"%a\", \"drc_version\": \"%a\"}",
                state->failed_boots, state->model_name, state->drc_version);
    
    // Try HTTP POST to each validator
    BOOLEAN use_simulation = TRUE;  // Switch to FALSE when Ethernet ready
    
    for (UINT32 i = 0; i < MAX_VALIDATORS; i++) {
        Print(L"  Validator %d (%a): ", i + 1, validator_urls[i]);
        
        BOOLEAN approved = FALSE;
        CHAR8 reason[256] = {0};
        
        if (!use_simulation) {
            // Real HTTP POST
            CHAR8 response[1024];
            if (send_http_post(validator_urls[i], json_payload, response, sizeof(response))) {
                // Parse JSON response: {"approved": true/false, "reason": "..."}
                // Simplified parsing (real JSON parser to be added)
                if (AsciiStrStr(response, "\"approved\": true") || 
                    AsciiStrStr(response, "\"approved\":true")) {
                    approved = TRUE;
                } else {
                    approved = FALSE;
                }
                Print(L"%a\r\n", approved ? "✓ APPROVED" : "✗ REJECTED");
            } else {
                Print(L"✗ NETWORK ERROR\r\n");
                result->rejections++;
                continue;
            }
        } else {
            // Simulation mode (until Ethernet ready)
            BOOLEAN valid_model = (AsciiStrCmp(state->model_name, "stories15M") == 0 ||
                                   AsciiStrCmp(state->model_name, "stories110M") == 0);
            BOOLEAN valid_drc = (AsciiStrCmp(state->drc_version, "5.1") == 0);
            BOOLEAN not_too_many_failures = (state->failed_boots < 5);
            
            approved = (valid_model && valid_drc && not_too_many_failures);
            
            if (!valid_model) {
                AsciiSPrint(reason, sizeof(reason), "Model '%a' not approved", state->model_name);
            } else if (!valid_drc) {
                AsciiSPrint(reason, sizeof(reason), "DRC version '%a' not approved", state->drc_version);
            } else if (!not_too_many_failures) {
                AsciiSPrint(reason, sizeof(reason), "Too many failed boots (%u)", state->failed_boots);
            } else {
                AsciiSPrint(reason, sizeof(reason), "All checks passed");
            }
            
            Print(L"%a\r\n", approved ? "✓ APPROVED" : "✗ REJECTED");
        }
        
        if (approved) {
            result->approvals++;
        } else {
            result->rejections++;
            if (result->reason[0] == 0) {  // First rejection sets reason
                AsciiStrCpyS(result->reason, sizeof(result->reason), reason);
            }
        }
    }
    
    Print(L"\r\n[CONSENSUS] %d/%d approvals (need %d)\r\n", 
          result->approvals, MAX_VALIDATORS, CONSENSUS_THRESHOLD);
    
    if (result->approvals >= CONSENSUS_THRESHOLD) {
        result->approved = TRUE;
        Print(L"✓ BOOT APPROVED\r\n\r\n");
        return EFI_SUCCESS;
    } else {
        result->approved = FALSE;
        Print(L"✗ BOOT REJECTED: %a\r\n\r\n", result->reason);
        return EFI_ACCESS_DENIED;
    }
}
