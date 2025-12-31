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

static void demo_llmk(EFI_SYSTEM_TABLE *SystemTable) {
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

  Print(L"[llmk] init sentinel...\r\n");
  LlmkSentinel sentinel;
  LlmkSentinelConfig scfg;
  scfg.enabled = TRUE;
  scfg.strict_mode = FALSE;
  scfg.max_cycles = 0; // disabled for now
  scfg.log_violations = TRUE;

  st = llmk_sentinel_init(&sentinel, &zones, &scfg);
  if (EFI_ERROR(st)) {
    Print(L"[llmk] sentinel init failed: %r\r\n", st);
    return;
  }
  llmk_sentinel_print_status(&sentinel);

  // Demo allocations
  void *kv = llmk_arena_alloc(&zones, LLMK_ARENA_KV_CACHE, 4ULL * 1024ULL * 1024ULL, 64);
  void *scratch = llmk_arena_alloc(&zones, LLMK_ARENA_SCRATCH, 2ULL * 1024ULL * 1024ULL, 64);
  void *acts = llmk_arena_alloc(&zones, LLMK_ARENA_ACTIVATIONS, 1ULL * 1024ULL * 1024ULL, 64);
  void *w = llmk_arena_alloc(&zones, LLMK_ARENA_WEIGHTS, 4096, 64);

  Print(L"[llmk] alloc kv=0x%lx scratch=0x%lx acts=0x%lx weights=0x%lx\r\n",
      (UINT64)(UINTN)kv, (UINT64)(UINTN)scratch, (UINT64)(UINTN)acts, (UINT64)(UINTN)w);

  // Demonstrate RO weights protection (logical): deny write.
  if (w && !llmk_sentinel_check_write(&sentinel, (UINT64)(UINTN)w, 16)) {
    llmk_sentinel_fail_safe(&sentinel, L"attempted write into WEIGHTS (RO) blocked");
  } else {
    Print(L"[llmk] WARN: weights write check did not block (unexpected)\r\n");
  }

  // Demonstrate allowed write into scratch.
  if (scratch && llmk_sentinel_check_write(&sentinel, (UINT64)(UINTN)scratch, 16)) {
    volatile UINT8 *p = (volatile UINT8 *)scratch;
    for (int i = 0; i < 16; i++) p[i] = (UINT8)i;
    Print(L"[llmk] scratch write OK\r\n");
  } else {
    llmk_sentinel_fail_safe(&sentinel, L"scratch write blocked (unexpected)");
  }

  llmk_zones_print(&zones);
  llmk_sentinel_print_status(&sentinel);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    print_banner();

  demo_llmk(SystemTable);

    Print(L"Press any key to exit...\r\n");
    EFI_INPUT_KEY Key;
    UINTN index;
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &index);
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);

    return EFI_SUCCESS;
}
