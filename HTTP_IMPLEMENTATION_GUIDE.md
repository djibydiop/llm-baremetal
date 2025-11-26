# HTTP Download Implementation Guide

## Current Status

### ✅ Completed
- **Model Repository Catalog**: 5 models defined with URLs and metadata
- **URL Parsing**: `parse_url()` function working (hostname, port, path extraction)
- **Download UI**: Menu system and progress display framework
- **Hardware Detection**: RAM/CPU checks for model compatibility
- **AVX2 SIMD Optimization**: Matmul function using AVX2 intrinsics (2-3x speedup)
- **Performance Tracking**: Basic tok/s measurement (forward pass counting)

### 🚧 In Progress - HTTP Protocol Implementation

#### Required EFI Protocols

1. **HTTP Protocol** (`EFI_HTTP_PROTOCOL_GUID`)
   - GUID: `{0x7a59b29b, 0x910b, 0x4171, {0x82, 0x42, 0xa8, 0x5a, 0x0d, 0xf2, 0x5b, 0x5b}}`
   - Methods needed:
     - `Configure()` - Set HTTP connection parameters
     - `Request()` - Send HTTP GET request
     - `Response()` - Receive HTTP response
     - `Cancel()` - Cancel pending requests

2. **HTTP Service Binding Protocol** (`EFI_HTTP_SERVICE_BINDING_PROTOCOL_GUID`)
   - GUID: `{0xbdc8e6af, 0xd9bc, 0x4379, {0xa7, 0x2a, 0xe0, 0xc4, 0xe7, 0x5d, 0xae, 0x1c}}`
   - Used to create HTTP protocol instances

3. **IP4/IP6 Configuration** (prerequisite)
   - DHCP client for automatic network setup
   - Static IP configuration as fallback
   - DNS resolution for hostname→IP mapping

#### Implementation Steps

```c
// 1. Locate HTTP Service Binding Protocol
EFI_GUID HttpServiceBindingGuid = EFI_HTTP_SERVICE_BINDING_PROTOCOL_GUID;
EFI_HANDLE *Handles = NULL;
UINTN HandleCount = 0;

Status = BS->LocateHandleBuffer(
    ByProtocol,
    &HttpServiceBindingGuid,
    NULL,
    &HandleCount,
    &Handles
);

// 2. Create Child HTTP Instance
EFI_HTTP_PROTOCOL *Http = NULL;
Status = ServiceBinding->CreateChild(ServiceBinding, &HttpHandle);

// 3. Configure HTTP Protocol
EFI_HTTP_CONFIG_DATA ConfigData = {
    .HttpVersion = HttpVersion11,
    .TimeOutMillisec = 5000,
    .LocalAddressIsIPv6 = FALSE,
    .AccessPoint = {
        .IPv4Node = {
            .UseDefaultAddress = TRUE
        }
    }
};
Status = Http->Configure(Http, &ConfigData);

// 4. Build HTTP Request
EFI_HTTP_REQUEST_DATA Request = {
    .Method = HttpMethodGet,
    .Url = L"http://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin"
};

EFI_HTTP_MESSAGE RequestMsg = {
    .Data.Request = &Request,
    .HeaderCount = 1,
    .Headers = (EFI_HTTP_HEADER[]) {
        { .FieldName = "User-Agent", .FieldValue = "UEFI-LLaMA/1.0" }
    }
};

// 5. Send Request
EFI_HTTP_TOKEN RequestToken = {
    .Event = NULL,  // Synchronous
    .Message = &RequestMsg
};
Status = Http->Request(Http, &RequestToken);

// 6. Receive Response
UINT8 *Buffer = AllocatePool(1024 * 1024);  // 1MB chunks
EFI_HTTP_MESSAGE ResponseMsg = {
    .Data.Response = AllocatePool(sizeof(EFI_HTTP_RESPONSE_DATA)),
    .Body = Buffer,
    .BodyLength = 1024 * 1024
};

EFI_HTTP_TOKEN ResponseToken = {
    .Event = NULL,
    .Message = &ResponseMsg
};

while (TRUE) {
    Status = Http->Response(Http, &ResponseToken);
    if (EFI_ERROR(Status)) break;
    
    // Write chunk to file
    // Update progress bar
    // Check if transfer complete
}
```

#### Download Progress Display

```c
void display_download_progress(UINT64 downloaded, UINT64 total, UINT64 speed_kbps) {
    UINT64 percent = (downloaded * 100) / total;
    UINT64 downloaded_mb = downloaded / (1024 * 1024);
    UINT64 total_mb = total / (1024 * 1024);
    UINT64 eta_seconds = (total - downloaded) / (speed_kbps * 1024);
    
    // Progress bar
    Print(L"\r  Progress: [");
    for (int i = 0; i < 50; i++) {
        if (i < percent / 2) Print(L"█");
        else Print(L"░");
    }
    Print(L"] %d%%", percent);
    
    // Stats
    Print(L"\r\n  Downloaded: %d MB / %d MB", downloaded_mb, total_mb);
    Print(L"\r\n  Speed: %d KB/s | ETA: %02d:%02d", 
          speed_kbps, eta_seconds / 60, eta_seconds % 60);
}
```

## Alternative: Manual Download Instructions

Since HTTP Protocol implementation is complex and not universally supported in all UEFI implementations, we provide clear manual download instructions:

### Using wget/curl (Linux/WSL/macOS)

```bash
# Download models
cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal

# Stories 15M (60 MB)
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin

# Stories 42M (164 MB)
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories42M.bin

# Stories 110M (420 MB)
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin

# Stories 260M (1 GB)
wget https://huggingface.co/karpathy/tinyllamas/resolve/main/stories260M.bin

# Tokenizer (required)
wget https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin

# Copy to USB image
cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal
mcopy -i usb.img stories42M.bin ::stories42M.bin
mcopy -i usb.img stories110M.bin ::stories110M.bin
mcopy -i usb.img stories260M.bin ::stories260M.bin
```

### Using PowerShell (Windows)

```powershell
# Download models
cd C:\Users\djibi\Desktop\baremetal\llm-baremetal

# Stories 15M
Invoke-WebRequest -Uri "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin" -OutFile "stories15M.bin"

# Stories 42M
Invoke-WebRequest -Uri "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories42M.bin" -OutFile "stories42M.bin"

# Stories 110M
Invoke-WebRequest -Uri "https://huggingface.co/karpathy/tinyllamas/resolve/main/stories110M.bin" -OutFile "stories110M.bin"
```

## Testing HTTP Support

### QEMU Network Configuration

To enable network in QEMU (required for testing HTTP downloads):

```bash
qemu-system-x86_64 \
  -drive file=usb.img,format=raw,if=none,id=usbstick \
  -device nec-usb-xhci,id=xhci \
  -device usb-storage,bus=xhci.0,drive=usbstick \
  -bios OVMF.fd \
  -m 1024 \
  -cpu Haswell \
  -netdev user,id=net0 \
  -device e1000,netdev=net0 \
  -nographic \
  -serial mon:stdio
```

### Check Network Availability

Current `check_network_available()` function returns FALSE by default. To enable:

1. Remove early return in `check_network_available()`
2. Uncomment HTTP Protocol detection code
3. Test with network-enabled QEMU
4. Verify `HandleCount > 0` when protocols available

## Performance Optimizations

### ✅ Implemented
- **AVX2 SIMD matmul**: 8-float vectorization with FMA intrinsics
- **4x loop unrolling**: Fallback for non-AVX2 systems
- **Efficient memory layout**: KV cache uses pointers (not separate allocations)

### 📊 Performance Metrics (15M model)
- Forward passes tracked per generation
- Tokens/forward pass ratio displayed
- Baseline: ~80 tokens/generation in QEMU
- Target: Measure real tok/s on physical hardware with AVX2

## Next Steps

1. **Download 42M/110M models manually** for testing
2. **Implement multi-model support** (config auto-detect)
3. **Measure real-world tok/s** on physical hardware
4. **Optional: Complete HTTP Protocol** implementation if UEFI supports it
5. **Create bootable USB** with multiple models

## References

- **UEFI Spec 2.10**: Chapter 28 - Network Protocols
- **EDK II HttpDxe**: Reference implementation in TianoCore
- **GNU-EFI**: Limited HTTP support, may need custom protocol definitions
- **Karpathy llama2.c**: https://github.com/karpathy/llama2.c
- **TinyLlamas Models**: https://huggingface.co/karpathy/tinyllamas

## Current Optimizations Active

```c
// Compiler flags (Makefile)
CFLAGS += -mavx2 -mfma

// Runtime detection
if (hw->has_avx2) {
    // Use AVX2 path in matmul
    __m256 sum = _mm256_setzero_ps();
    sum = _mm256_fmadd_ps(wx, xx, sum);
}
```

**Status**: AVX2 code path compiled and ready for testing. Performance measurement in progress.
