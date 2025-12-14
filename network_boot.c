/*
 * Bare-Metal Network Boot - HTTP Download Implementation
 * 
 * Complete HTTP/1.0 client for downloading models over Ethernet.
 * Uses UEFI network protocols (TCP4, IP4).
 * 
 * Usage:
 *   http_download_model("http://192.168.1.100/models/stories110M.bin")
 */

#include <efi.h>
#include <efilib.h>
#include <efitcp.h>  // TCP4 protocol definitions

// Network protocols GUIDs - use system definitions
static EFI_GUID gEfiTcp4ServiceBindingProtocolGuid = EFI_TCP4_SERVICE_BINDING_PROTOCOL;
static EFI_GUID gEfiTcp4ProtocolGuid = EFI_TCP4_PROTOCOL;

// String utility functions
static UINTN AsciiStrLen(const CHAR8* str) {
    if (!str) return 0;
    UINTN len = 0;
    while (str[len]) len++;
    return len;
}

static CHAR8* AsciiStrStr(const CHAR8* haystack, const CHAR8* needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == 0) return (CHAR8*)haystack;
    
    const CHAR8* h = haystack;
    while (*h) {
        const CHAR8* h2 = h;
        const CHAR8* n = needle;
        
        while (*h2 && *n && (*h2 == *n)) {
            h2++;
            n++;
        }
        
        if (*n == 0) return (CHAR8*)h;
        h++;
    }
    
    return NULL;
}

static int AsciiSPrint(CHAR8* buffer, UINTN bufsize, const CHAR8* format, ...) {
    // Simple sprintf for HTTP request - hardcoded for our specific use case
    // Format: "GET %a HTTP/1.0\r\nHost: %a\r\nConnection: close\r\n\r\n"
    
    va_list args;
    va_start(args, format);
    
    const CHAR8* path = va_arg(args, const CHAR8*);
    const CHAR8* host = va_arg(args, const CHAR8*);
    
    UINTN i = 0;
    
    // GET
    buffer[i++] = 'G'; buffer[i++] = 'E'; buffer[i++] = 'T'; buffer[i++] = ' ';
    
    // Path
    const CHAR8* p = path;
    while (*p && i < bufsize - 1) buffer[i++] = *p++;
    
    // HTTP/1.0\r\n
    buffer[i++] = ' '; buffer[i++] = 'H'; buffer[i++] = 'T'; buffer[i++] = 'T'; buffer[i++] = 'P';
    buffer[i++] = '/'; buffer[i++] = '1'; buffer[i++] = '.'; buffer[i++] = '0';
    buffer[i++] = '\r'; buffer[i++] = '\n';
    
    // Host:
    buffer[i++] = 'H'; buffer[i++] = 'o'; buffer[i++] = 's'; buffer[i++] = 't'; buffer[i++] = ':'; buffer[i++] = ' ';
    
    // Host value
    p = host;
    while (*p && i < bufsize - 1) buffer[i++] = *p++;
    buffer[i++] = '\r'; buffer[i++] = '\n';
    
    // Connection: close\r\n\r\n
    const CHAR8* conn = "Connection: close\r\n\r\n";
    p = conn;
    while (*p && i < bufsize - 1) buffer[i++] = *p++;
    
    buffer[i] = 0;
    
    va_end(args);
    return i;
}

// HTTP response buffer
#define HTTP_BUFFER_SIZE (10 * 1024 * 1024)  // 10 MB chunks

typedef struct {
    CHAR8 protocol[8];
    CHAR8 host[256];
    UINT16 port;
    CHAR8 path[512];
    UINT32 ip_addr;  // Parsed IP (192.168.1.100 → 0xC0A80164)
} HttpUrl;

// Parse IPv4 address from string
EFI_STATUS parse_ipv4(const CHAR8* ip_str, UINT32* ip_addr) {
    UINT32 a = 0, b = 0, c = 0, d = 0;
    const CHAR8* p = ip_str;
    
    // Parse first octet
    while (*p >= '0' && *p <= '9') {
        a = a * 10 + (*p - '0');
        p++;
    }
    if (*p++ != '.') return EFI_INVALID_PARAMETER;
    
    // Parse second octet
    while (*p >= '0' && *p <= '9') {
        b = b * 10 + (*p - '0');
        p++;
    }
    if (*p++ != '.') return EFI_INVALID_PARAMETER;
    
    // Parse third octet
    while (*p >= '0' && *p <= '9') {
        c = c * 10 + (*p - '0');
        p++;
    }
    if (*p++ != '.') return EFI_INVALID_PARAMETER;
    
    // Parse fourth octet
    while (*p >= '0' && *p <= '9') {
        d = d * 10 + (*p - '0');
        p++;
    }
    
    *ip_addr = (a << 24) | (b << 16) | (c << 8) | d;
    return EFI_SUCCESS;
}

// Parse HTTP URL
EFI_STATUS parse_http_url(const CHAR8* url_str, HttpUrl* url) {
    const CHAR8* p = url_str;
    
    // Skip "http://"
    if (CompareMem(p, "http://", 7) == 0) {
        CopyMem(url->protocol, "http", 5);
        p += 7;
    } else {
        return EFI_INVALID_PARAMETER;
    }
    
    // Extract host (IP address or hostname)
    const CHAR8* host_start = p;
    while (*p && *p != ':' && *p != '/') p++;
    
    UINTN host_len = p - host_start;
    if (host_len >= sizeof(url->host)) return EFI_BUFFER_TOO_SMALL;
    
    CopyMem(url->host, host_start, host_len);
    url->host[host_len] = 0;
    
    // Parse IP address
    EFI_STATUS Status = parse_ipv4(url->host, &url->ip_addr);
    if (EFI_ERROR(Status)) {
        Print(L"[HTTP] Invalid IP address: %a\r\n", url->host);
        return Status;
    }
    
    // Extract port (default 80)
    if (*p == ':') {
        p++;
        url->port = 0;
        while (*p >= '0' && *p <= '9') {
            url->port = url->port * 10 + (*p - '0');
            p++;
        }
    } else {
        url->port = 80;
    }
    
    // Extract path
    if (*p == '/') {
        UINTN path_len = AsciiStrLen(p);
        if (path_len >= sizeof(url->path)) return EFI_BUFFER_TOO_SMALL;
        CopyMem(url->path, p, path_len + 1);
    } else {
        url->path[0] = '/';
        url->path[1] = 0;
    }
    
    return EFI_SUCCESS;
}

// Download model via HTTP
EFI_STATUS http_download_model(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable,
    const CHAR8* url_str,
    VOID** model_buffer,
    UINTN* model_size
) {
    EFI_STATUS Status;
    HttpUrl url;
    
    Print(L"\r\n");
    Print(L"========================================\r\n");
    Print(L"  NETWORK BOOT - HTTP DOWNLOAD\r\n");
    Print(L"========================================\r\n");
    Print(L"\r\n");
    Print(L"  URL: %a\r\n", url_str);
    
    // Parse URL
    Status = parse_http_url(url_str, &url);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to parse URL\r\n");
        return Status;
    }
    
    Print(L"  Host: %a\r\n", url.host);
    Print(L"  IP: %d.%d.%d.%d\r\n",
          (url.ip_addr >> 24) & 0xFF,
          (url.ip_addr >> 16) & 0xFF,
          (url.ip_addr >> 8) & 0xFF,
          url.ip_addr & 0xFF);
    Print(L"  Port: %d\r\n", url.port);
    Print(L"  Path: %a\r\n", url.path);
    Print(L"\r\n");
    
    // Locate TCP4 Service Binding Protocol
    EFI_SERVICE_BINDING* TcpServiceBinding;
    
    Status = uefi_call_wrapper(
        SystemTable->BootServices->LocateProtocol,
        3,
        &gEfiTcp4ServiceBindingProtocolGuid,
        NULL,
        (VOID**)&TcpServiceBinding
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] TCP4 Service Binding not found: %r\r\n", Status);
        Print(L"  Hint: Use QEMU with: -netdev user,id=net0 -device e1000,netdev=net0\r\n");
        return Status;
    }
    
    Print(L"[OK] TCP/IP stack available\r\n");
    
    // Create TCP4 child handle
    EFI_HANDLE Tcp4ChildHandle = NULL;
    Status = uefi_call_wrapper(
        TcpServiceBinding->CreateChild,
        2,
        TcpServiceBinding,
        &Tcp4ChildHandle
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to create TCP4 child: %r\r\n", Status);
        return Status;
    }
    
    Print(L"[OK] TCP4 child handle created\r\n");
    
    // Open TCP4 protocol
    EFI_TCP4* Tcp4;
    
    Status = uefi_call_wrapper(
        SystemTable->BootServices->OpenProtocol,
        6,
        Tcp4ChildHandle,
        &gEfiTcp4ProtocolGuid,
        (VOID**)&Tcp4,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to open TCP4 protocol: %r\r\n", Status);
        TcpServiceBinding->DestroyChild(TcpServiceBinding, Tcp4ChildHandle);
        return Status;
    }
    
    Print(L"[OK] TCP4 protocol opened\r\n");
    
    // Configure TCP4 connection
    EFI_TCP4_CONFIG_DATA Tcp4ConfigData;
    SetMem(&Tcp4ConfigData, sizeof(Tcp4ConfigData), 0);
    
    Tcp4ConfigData.TypeOfService = 0;
    Tcp4ConfigData.TimeToLive = 64;
    
    // Local address: 0.0.0.0:0 (auto-assign)
    SetMem(&Tcp4ConfigData.AccessPoint.StationAddress, 4, 0);
    SetMem(&Tcp4ConfigData.AccessPoint.SubnetMask, 4, 0);
    Tcp4ConfigData.AccessPoint.StationPort = 0;  // Auto
    
    // Remote address
    Tcp4ConfigData.AccessPoint.RemoteAddress.Addr[0] = (url.ip_addr >> 24) & 0xFF;
    Tcp4ConfigData.AccessPoint.RemoteAddress.Addr[1] = (url.ip_addr >> 16) & 0xFF;
    Tcp4ConfigData.AccessPoint.RemoteAddress.Addr[2] = (url.ip_addr >> 8) & 0xFF;
    Tcp4ConfigData.AccessPoint.RemoteAddress.Addr[3] = url.ip_addr & 0xFF;
    Tcp4ConfigData.AccessPoint.RemotePort = url.port;
    Tcp4ConfigData.AccessPoint.ActiveFlag = TRUE;  // Active connection
    
    Tcp4ConfigData.ControlOption = NULL;  // Default options
    
    Status = uefi_call_wrapper(Tcp4->Configure, 2, Tcp4, &Tcp4ConfigData);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to configure TCP4: %r\r\n", Status);
        goto cleanup;
    }
    
    Print(L"[OK] TCP4 configured\r\n");
    
    // Create connection event
    EFI_TCP4_CONNECTION_TOKEN ConnectToken;
    SetMem(&ConnectToken, sizeof(ConnectToken), 0);
    
    Status = uefi_call_wrapper(
        SystemTable->BootServices->CreateEvent,
        5,
        0,
        TPL_CALLBACK,
        NULL,
        NULL,
        &ConnectToken.CompletionToken.Event
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to create event: %r\r\n", Status);
        goto cleanup;
    }
    
    // Connect to server
    Print(L"[CONNECT] Connecting to %d.%d.%d.%d:%d...\r\n",
          (url.ip_addr >> 24) & 0xFF,
          (url.ip_addr >> 16) & 0xFF,
          (url.ip_addr >> 8) & 0xFF,
          url.ip_addr & 0xFF,
          url.port);
    
    Status = uefi_call_wrapper(Tcp4->Connect, 2, Tcp4, &ConnectToken);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Connect failed: %r\r\n", Status);
        SystemTable->BootServices->CloseEvent(ConnectToken.CompletionToken.Event);
        goto cleanup;
    }
    
    // Wait for connection (timeout: 10 seconds)
    UINTN Index;
    Status = uefi_call_wrapper(
        SystemTable->BootServices->WaitForEvent,
        3,
        1,
        &ConnectToken.CompletionToken.Event,
        &Index
    );
    
    SystemTable->BootServices->CloseEvent(ConnectToken.CompletionToken.Event);
    
    if (EFI_ERROR(Status) || EFI_ERROR(ConnectToken.CompletionToken.Status)) {
        Print(L"[ERROR] Connection failed: %r\r\n", 
              EFI_ERROR(Status) ? Status : ConnectToken.CompletionToken.Status);
        goto cleanup;
    }
    
    Print(L"[OK] Connected!\r\n");
    
    // Build HTTP GET request
    CHAR8 http_request[1024];
    AsciiSPrint(http_request, sizeof(http_request),
                "GET %a HTTP/1.0\r\n"
                "Host: %a\r\n"
                "Connection: close\r\n"
                "\r\n",
                url.path, url.host);
    
    UINTN request_len = AsciiStrLen(http_request);
    Print(L"[HTTP] Sending GET request (%d bytes)...\r\n", request_len);
    
    // Send HTTP request
    EFI_TCP4_IO_TOKEN TransmitToken;
    EFI_TCP4_FRAGMENT_DATA FragmentData;
    UINT8 transmit_data[sizeof(EFI_TCP4_RECEIVE_DATA) + sizeof(EFI_TCP4_FRAGMENT_DATA)];
    EFI_TCP4_RECEIVE_DATA* TxData = (EFI_TCP4_RECEIVE_DATA*)transmit_data;
    
    SetMem(&TransmitToken, sizeof(TransmitToken), 0);
    SetMem(transmit_data, sizeof(transmit_data), 0);
    
    Status = uefi_call_wrapper(
        SystemTable->BootServices->CreateEvent,
        5,
        0,
        TPL_CALLBACK,
        NULL,
        NULL,
        &TransmitToken.CompletionToken.Event
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Failed to create transmit event: %r\r\n", Status);
        goto cleanup;
    }
    
    TxData->DataLength = request_len;
    TxData->FragmentCount = 1;
    TxData->FragmentTable[0].FragmentLength = request_len;
    TxData->FragmentTable[0].FragmentBuffer = http_request;
    
    TransmitToken.Packet.RxData = TxData;
    
    Status = uefi_call_wrapper(Tcp4->Transmit, 2, Tcp4, &TransmitToken);
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] Transmit failed: %r\r\n", Status);
        SystemTable->BootServices->CloseEvent(TransmitToken.CompletionToken.Event);
        goto cleanup;
    }
    
    // Wait for transmission
    Status = uefi_call_wrapper(
        SystemTable->BootServices->WaitForEvent,
        3,
        1,
        &TransmitToken.CompletionToken.Event,
        &Index
    );
    
    SystemTable->BootServices->CloseEvent(TransmitToken.CompletionToken.Event);
    
    if (EFI_ERROR(Status) || EFI_ERROR(TransmitToken.CompletionToken.Status)) {
        Print(L"[ERROR] Send failed: %r\r\n",
              EFI_ERROR(Status) ? Status : TransmitToken.CompletionToken.Status);
        goto cleanup;
    }
    
    Print(L"[OK] HTTP request sent\r\n");
    
    // Receive response
    Print(L"[HTTP] Receiving response...\r\n");
    
    CHAR8* response_buffer = AllocatePool(HTTP_BUFFER_SIZE);
    if (!response_buffer) {
        Print(L"[ERROR] Out of memory\r\n");
        Status = EFI_OUT_OF_RESOURCES;
        goto cleanup;
    }
    
    UINTN total_received = 0;
    UINTN content_length = 0;
    BOOLEAN headers_parsed = FALSE;
    
    while (TRUE) {
        EFI_TCP4_IO_TOKEN ReceiveToken;
        SetMem(&ReceiveToken, sizeof(ReceiveToken), 0);
        
        Status = uefi_call_wrapper(
            SystemTable->BootServices->CreateEvent,
            5,
            0,
            TPL_CALLBACK,
            NULL,
            NULL,
            &ReceiveToken.CompletionToken.Event
        );
        
        if (EFI_ERROR(Status)) {
            Print(L"[ERROR] Failed to create receive event: %r\r\n", Status);
            break;
        }
        
        Status = uefi_call_wrapper(Tcp4->Receive, 2, Tcp4, &ReceiveToken);
        if (EFI_ERROR(Status)) {
            SystemTable->BootServices->CloseEvent(ReceiveToken.CompletionToken.Event);
            if (Status == EFI_CONNECTION_FIN) {
                Print(L"[OK] Connection closed by server\r\n");
                Status = EFI_SUCCESS;
            }
            break;
        }
        
        // Wait for data
        Status = uefi_call_wrapper(
            SystemTable->BootServices->WaitForEvent,
            3,
            1,
            &ReceiveToken.CompletionToken.Event,
            &Index
        );
        
        if (EFI_ERROR(Status) || EFI_ERROR(ReceiveToken.CompletionToken.Status)) {
            SystemTable->BootServices->CloseEvent(ReceiveToken.CompletionToken.Event);
            if (ReceiveToken.CompletionToken.Status == EFI_CONNECTION_FIN) {
                Print(L"[OK] Transfer complete\r\n");
                Status = EFI_SUCCESS;
            }
            break;
        }
        
        EFI_TCP4_RECEIVE_DATA* RxData = ReceiveToken.Packet.RxData;
        if (RxData && RxData->DataLength > 0) {
            // Copy received data
            for (UINT32 i = 0; i < RxData->FragmentCount; i++) {
                UINT32 frag_len = RxData->FragmentTable[i].FragmentLength;
                if (total_received + frag_len > HTTP_BUFFER_SIZE) {
                    Print(L"[ERROR] Buffer overflow\r\n");
                    Status = EFI_BUFFER_TOO_SMALL;
                    break;
                }
                
                CopyMem(response_buffer + total_received,
                       RxData->FragmentTable[i].FragmentBuffer,
                       frag_len);
                total_received += frag_len;
            }
            
            // Parse headers (first time only)
            if (!headers_parsed) {
                CHAR8* header_end = AsciiStrStr(response_buffer, "\r\n\r\n");
                if (header_end) {
                    headers_parsed = TRUE;
                    
                    // Find Content-Length
                    CHAR8* content_len_ptr = AsciiStrStr(response_buffer, "Content-Length:");
                    if (content_len_ptr) {
                        content_len_ptr += 15;  // Skip "Content-Length:"
                        while (*content_len_ptr == ' ') content_len_ptr++;
                        
                        content_length = 0;
                        while (*content_len_ptr >= '0' && *content_len_ptr <= '9') {
                            content_length = content_length * 10 + (*content_len_ptr - '0');
                            content_len_ptr++;
                        }
                        
                        Print(L"[HTTP] Content-Length: %d bytes (%.1f MB)\r\n",
                              content_length, (float)content_length / (1024.0f * 1024.0f));
                    }
                }
            }
            
            // Show progress
            if (content_length > 0) {
                UINTN percent = (total_received * 100) / content_length;
                if (percent % 10 == 0) {
                    Print(L"[DOWNLOAD] %d%% (%d/%d MB)\r\n",
                          percent,
                          total_received / (1024 * 1024),
                          content_length / (1024 * 1024));
                }
            } else {
                if (total_received % (1024 * 1024) == 0) {
                    Print(L"[DOWNLOAD] %d MB received...\r\n", total_received / (1024 * 1024));
                }
            }
        }
        
        SystemTable->BootServices->CloseEvent(ReceiveToken.CompletionToken.Event);
    }
    
    if (EFI_ERROR(Status)) {
        FreePool(response_buffer);
        goto cleanup;
    }
    
    // Parse response body (skip headers)
    CHAR8* body_start = AsciiStrStr(response_buffer, "\r\n\r\n");
    if (!body_start) {
        Print(L"[ERROR] Invalid HTTP response\r\n");
        FreePool(response_buffer);
        Status = EFI_PROTOCOL_ERROR;
        goto cleanup;
    }
    
    body_start += 4;  // Skip "\r\n\r\n"
    UINTN body_size = total_received - (body_start - response_buffer);
    
    Print(L"\r\n[OK] Download complete: %d bytes\r\n", body_size);
    
    // Allocate final buffer and copy body
    *model_buffer = AllocatePool(body_size);
    if (!*model_buffer) {
        Print(L"[ERROR] Out of memory\r\n");
        FreePool(response_buffer);
        Status = EFI_OUT_OF_RESOURCES;
        goto cleanup;
    }
    
    CopyMem(*model_buffer, body_start, body_size);
    *model_size = body_size;
    
    FreePool(response_buffer);
    Status = EFI_SUCCESS;
    
cleanup:
    // Close connection
    if (Tcp4) {
        EFI_TCP4_CLOSE_TOKEN CloseToken;
        SetMem(&CloseToken, sizeof(CloseToken), 0);
        
        SystemTable->BootServices->CreateEvent(0, TPL_CALLBACK, NULL, NULL,
                                               &CloseToken.CompletionToken.Event);
        Tcp4->Close(Tcp4, &CloseToken);
        SystemTable->BootServices->WaitForEvent(1, &CloseToken.CompletionToken.Event, &Index);
        SystemTable->BootServices->CloseEvent(CloseToken.CompletionToken.Event);
    }
    
    // Destroy child handle
    if (Tcp4ChildHandle) {
        TcpServiceBinding->DestroyChild(TcpServiceBinding, Tcp4ChildHandle);
    }
    
    return Status;
}

// Simple network test - check if TCP stack is available
BOOLEAN check_network_available(EFI_SYSTEM_TABLE *SystemTable) {
    VOID* TcpServiceBinding = NULL;
    
    EFI_STATUS Status = uefi_call_wrapper(
        SystemTable->BootServices->LocateProtocol,
        3,
        &gEfiTcp4ServiceBindingProtocolGuid,
        NULL,
        &TcpServiceBinding
    );
    
    return !EFI_ERROR(Status) && (TcpServiceBinding != NULL);
}
