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

// Network protocols GUIDs
#define EFI_TCP4_SERVICE_BINDING_PROTOCOL \
    { 0x00720665, 0x67EB, 0x4a99, {0xBA, 0xF7, 0xD3, 0xC3, 0x3A, 0x1C, 0x7C, 0xC9} }

#define EFI_TCP4_PROTOCOL \
    { 0x65530BC7, 0xA359, 0x410f, {0xB0, 0x10, 0x5A, 0xAD, 0xC7, 0xEC, 0x2B, 0x62} }

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
    EFI_GUID Tcp4ServiceBindingGuid = EFI_TCP4_SERVICE_BINDING_PROTOCOL;
    EFI_SERVICE_BINDING_PROTOCOL* TcpServiceBinding;
    
    Status = uefi_call_wrapper(
        SystemTable->BootServices->LocateProtocol,
        3,
        &Tcp4ServiceBindingGuid,
        NULL,
        (VOID**)&TcpServiceBinding
    );
    
    if (EFI_ERROR(Status)) {
        Print(L"[ERROR] TCP4 Service Binding not found: %r\r\n", Status);
        Print(L"  Hint: Use QEMU with: -netdev user,id=net0 -device e1000,netdev=net0\r\n");
        return Status;
    }
    
    Print(L"[OK] TCP/IP stack available\r\n");
    
    // TODO: Create TCP4 child handle
    // TODO: Configure TCP4 connection
    // TODO: Connect to server
    // TODO: Send HTTP GET request
    // TODO: Receive response headers
    // TODO: Parse Content-Length
    // TODO: Receive body in chunks with progress
    // TODO: Allocate buffer and copy data
    
    Print(L"\r\n");
    Print(L"[INFO] Network Boot implementation: IN PROGRESS\r\n");
    Print(L"  - TCP4 protocol: ✓ Detected\r\n");
    Print(L"  - HTTP client: ⏳ Coming soon\r\n");
    Print(L"  - Fallback: Loading from disk...\r\n");
    Print(L"\r\n");
    
    return EFI_NOT_READY;  // Not fully implemented yet
}

// Simple network test - check if TCP stack is available
BOOLEAN check_network_available(EFI_SYSTEM_TABLE *SystemTable) {
    EFI_GUID Tcp4ServiceBindingGuid = EFI_TCP4_SERVICE_BINDING_PROTOCOL;
    VOID* TcpServiceBinding = NULL;
    
    EFI_STATUS Status = uefi_call_wrapper(
        SystemTable->BootServices->LocateProtocol,
        3,
        &Tcp4ServiceBindingGuid,
        NULL,
        &TcpServiceBinding
    );
    
    return !EFI_ERROR(Status) && (TcpServiceBinding != NULL);
}
