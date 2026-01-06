// llm_kernel_efi.c
// Dedicated entrypoint for the "LLM-Kernel" workstream.
// Intentionally separate from llama2_efi_final.c (stable REPL).
//
// Goal: provide a clean starting point to integrate:
// - memory zones (weights / kv-cache / scratch / output)
// - memory sentinel / invariants
// - kernel-style ownership rules (no accidental AllocatePool scatter)
//
// Notes:
// - This file compiles stand-alone as a UEFI app even if the private
//   LLM-Kernel headers are not present yet.

#include <efi.h>
#include <efilib.h>

#include "djiblas.h"

#include "llmk_zones.h"
#include "llmk_sentinel.h"
#include "llmk_log.h"
#include "llmk_infer.h"

// (rdtsc helper lives in llmk_infer.c for inference calibration)

// Optional private headers (can live locally without being committed).
// We will wire them in when you drop the topo + code.
#if defined(__has_include)
  #if __has_include("memory_zones.h")
    #define LLMK_HAS_ZONES 1
    #include "memory_zones.h"
  #else
    #define LLMK_HAS_ZONES 0
  #endif
  #if __has_include("memory_sentinel.h")
    #define LLMK_HAS_SENTINEL 1
    #include "memory_sentinel.h"
  #else
    #define LLMK_HAS_SENTINEL 0
  #endif
#else
  #define LLMK_HAS_ZONES 0
  #define LLMK_HAS_SENTINEL 0
#endif

static void print_banner(void) {
    Print(L"\r\n");
    Print(L"----------------------------------------\r\n");
    Print(L"  LLM-KERNEL (WIP)\r\n");
    Print(L"  Dedicated build target\r\n");
    Print(L"----------------------------------------\r\n");

#if LLMK_HAS_ZONES
    Print(L"[llmk] memory_zones.h: present\r\n");
#else
    Print(L"[llmk] memory_zones.h: missing (OK for now)\r\n");
#endif

#if LLMK_HAS_SENTINEL
    Print(L"[llmk] memory_sentinel.h: present\r\n");
#else
    Print(L"[llmk] memory_sentinel.h: missing (OK for now)\r\n");
#endif

    Print(L"\r\n");
}

static void demo_llmk(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_BOOT_SERVICES *BS = SystemTable->BootServices;

  Print(L"[llmk] init zones...\r\n");
  LlmkZones zones;
  LlmkZonesConfig cfg;
  cfg.total_bytes = 768ULL * 1024ULL * 1024ULL;
  cfg.weights_bytes = 0;
  cfg.kv_bytes = 0;
  cfg.scratch_bytes = 0;
  cfg.activations_bytes = 0;
  cfg.zone_c_bytes = 0;

  EFI_STATUS st = llmk_zones_init(BS, &cfg, &zones);
  if (EFI_ERROR(st)) {
    Print(L"[llmk] zones init failed: %r\r\n", st);
    return;
  }
  llmk_zones_print(&zones);

  Print(L"[llmk] init Zone C log...\r\n");
  LlmkLog log;
  st = llmk_log_init(&zones, &log);
  if (EFI_ERROR(st)) {
    Print(L"[llmk] log init failed: %r\r\n", st);
    // continue without log
    log.entries = 0;
    log.capacity = 0;
    log.write_idx = 0;
  }

  Print(L"[llmk] init sentinel...\r\n");
  LlmkSentinel sentinel;
  LlmkSentinelConfig scfg;
  scfg.enabled = TRUE;
  scfg.strict_mode = TRUE;
  scfg.strict_alloc = TRUE;
  scfg.strict_budget = TRUE;
  // Budgets are calibrated by the real inference demo.
  scfg.max_cycles = 0;
  scfg.max_cycles_prefill = 0;
  scfg.max_cycles_decode = 0;
  scfg.log_violations = TRUE;

  st = llmk_sentinel_init(&sentinel, &zones, (log.capacity ? &log : 0), &scfg);
  if (EFI_ERROR(st)) {
    Print(L"[llmk] sentinel init failed: %r\r\n", st);
    return;
  }
  llmk_sentinel_print_status(&sentinel);

  // Open filesystem for model/tokenizer I/O.
  Print(L"[llmk] opening file system...\r\n");
  EFI_LOADED_IMAGE *LoadedImage;
  EFI_STATUS status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &LoadedImageProtocol, &LoadedImage);
  if (EFI_ERROR(status)) {
    Print(L"[llmk] LoadedImage protocol failed: %r\r\n", status);
    goto finalize;
  }

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
  status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &FileSystemProtocol, &FileSystem);
  if (EFI_ERROR(status)) {
    Print(L"[llmk] FileSystem protocol failed: %r\r\n", status);
    goto finalize;
  }

  EFI_FILE_HANDLE Root;
  status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
  if (EFI_ERROR(status)) {
    Print(L"[llmk] OpenVolume failed: %r\r\n", status);
    goto finalize;
  }

  (void)llmk_infer_demo(ImageHandle, SystemTable, &sentinel, Root);
  uefi_call_wrapper(Root->Close, 1, Root);

finalize:

  llmk_zones_print(&zones);
  llmk_sentinel_print_status(&sentinel);

  if (log.capacity) {
    llmk_log_dump(&log, 16);
  }
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    print_banner();

  demo_llmk(ImageHandle, SystemTable);

    Print(L"[llmk] demo complete; shutting down.\r\n");
    uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);

    // Shouldn't return; but if it does, avoid running into firmware UI.
    while (1) {
        uefi_call_wrapper(SystemTable->BootServices->Stall, 2, 1000 * 1000);
    }
}
