// llama2_kernel_integration.h - Instructions pour intégrer LLM-Kernel dans llama2_efi.c
// 
// Ce fichier contient les modifications à faire pour intégrer le LLM-Kernel
//
// ============================================================================
// ÉTAPE 1: Ajouter les includes (après les includes existants, ligne ~18)
// ============================================================================

// LLM-Kernel integration
#include "memory_zones.h"
#include "memory_sentinel.h"

// ============================================================================
// ÉTAPE 2: Initialiser le LLM-Kernel dans efi_main (après InitializeLib)
// ============================================================================

// Dans efi_main(), ajouter après "InitializeLib(ImageHandle, SystemTable);":

    Print(L"\r\n");
    Print(L"╔══════════════════════════════════════════════════════════╗\r\n");
    Print(L"║        🧬 LLM-KERNEL INITIALIZATION                      ║\r\n");
    Print(L"╚══════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    
    // Allocate heap for Zone B (100 MB for stories15M + buffers)
    UINTN heap_size = 100 * 1024 * 1024;  // 100 MB
    EFI_PHYSICAL_ADDRESS heap_base = 0;
    
    Print(L"[KERNEL] Allocating heap (%lu MB)...\r\n", heap_size / (1024 * 1024));
    EFI_STATUS kernel_status = SystemTable->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiLoaderData,
        (heap_size + 4095) / 4096,
        &heap_base
    );
    
    if (EFI_ERROR(kernel_status)) {
        Print(L"❌ Failed to allocate kernel heap: %r\r\n", kernel_status);
        return kernel_status;
    }
    
    Print(L"[KERNEL] Heap base: 0x%lx\r\n", heap_base);
    Print(L"\r\n");
    
    // Initialize memory zones
    Print(L"[KERNEL] Initializing memory zones...\r\n");
    kernel_status = zones_init(heap_base, heap_size);
    if (EFI_ERROR(kernel_status)) {
        Print(L"❌ Failed to initialize zones: %r\r\n", kernel_status);
        return kernel_status;
    }
    
    // Validate zones
    if (!zones_validate()) {
        Print(L"❌ Zone validation failed\r\n");
        return EFI_INVALID_PARAMETER;
    }
    
    Print(L"✅ Memory zones initialized and validated\r\n");
    Print(L"\r\n");
    
    // Initialize Memory Sentinel
    Print(L"[KERNEL] Initializing Memory Sentinel...\r\n");
    SentinelConfig sentinel_config = {
        .enabled = TRUE,
        .strict_mode = FALSE,  // Permissive mode for now
        .max_inference_time_ms = 0,
        .max_inference_cycles = 0,
        .log_violations = TRUE,
        .log_all_accesses = FALSE
    };
    
    kernel_status = sentinel_init(&sentinel_config);
    if (EFI_ERROR(kernel_status)) {
        Print(L"❌ Failed to initialize Sentinel: %r\r\n", kernel_status);
        return kernel_status;
    }
    
    Print(L"✅ Memory Sentinel active\r\n");
    Print(L"\r\n");
    
    // Print zone layout
    zones_print_layout();
    
    Print(L"🏎️  LLM-Kernel ready! Zone B (sacred) is protected.\r\n");
    Print(L"\r\n");

// ============================================================================
// ÉTAPE 3: Modifier load_model() pour utiliser ARENA_WEIGHTS
// ============================================================================

// Dans load_model(), remplacer l'allocation de static_weights par:

    // Allocate weights from ARENA_WEIGHTS
    UINTN file_size_bytes = (UINTN)file_size;
    Print(L"[KERNEL] Allocating %lu MB from ARENA_WEIGHTS...\r\n", 
          file_size_bytes / (1024 * 1024));
    
    static_weights = (float*)zones_arena_alloc(ARENA_WEIGHTS, file_size_bytes);
    if (!static_weights) {
        Print(L"[ERROR] Failed to allocate weights from ARENA_WEIGHTS\r\n");
        uefi_call_wrapper(File->Close, 1, File);
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Sentinel check: verify write access
    if (!sentinel_check_write((UINTN)static_weights, file_size_bytes)) {
        Print(L"[SENTINEL] ❌ Weights allocation outside Zone B!\r\n");
        uefi_call_wrapper(File->Close, 1, File);
        return EFI_ACCESS_DENIED;
    }
    
    Print(L"✅ Weights buffer at 0x%lx (Zone B verified)\r\n", (UINTN)static_weights);

// ============================================================================
// ÉTAPE 4: Modifier init_run_state() pour utiliser les arenas
// ============================================================================

// Remplacer TOUS les AllocatePool dans init_run_state() par zones_arena_alloc():

EFI_STATUS init_run_state(RunState* s, Config* p, EFI_BOOT_SERVICES *BS) {
    int kv_dim = (p->dim * p->n_kv_heads) / p->n_heads;
    
    Print(L"[LLM-KERNEL] Allocating RunState from arenas...\r\n");
    
    // SCRATCH arena: temporary buffers
    static_x = (float*)zones_arena_alloc(ARENA_SCRATCH, p->dim * sizeof(float));
    static_xb = (float*)zones_arena_alloc(ARENA_SCRATCH, p->dim * sizeof(float));
    static_xb2 = (float*)zones_arena_alloc(ARENA_SCRATCH, p->dim * sizeof(float));
    static_hb = (float*)zones_arena_alloc(ARENA_SCRATCH, p->hidden_dim * sizeof(float));
    static_hb2 = (float*)zones_arena_alloc(ARENA_SCRATCH, p->hidden_dim * sizeof(float));
    static_q = (float*)zones_arena_alloc(ARENA_SCRATCH, p->dim * sizeof(float));
    static_att = (float*)zones_arena_alloc(ARENA_SCRATCH, p->n_heads * p->seq_len * sizeof(float));
    
    // KV_CACHE arena
    static_k = (float*)zones_arena_alloc(ARENA_KV_CACHE, kv_dim * sizeof(float));
    static_v = (float*)zones_arena_alloc(ARENA_KV_CACHE, kv_dim * sizeof(float));
    static_key_cache = (float*)zones_arena_alloc(ARENA_KV_CACHE, p->n_layers * p->seq_len * kv_dim * sizeof(float));
    static_value_cache = (float*)zones_arena_alloc(ARENA_KV_CACHE, p->n_layers * p->seq_len * kv_dim * sizeof(float));
    
    // OUTPUT arena
    static_logits = (float*)zones_arena_alloc(ARENA_OUTPUT, p->vocab_size * sizeof(float));
    
    // Check allocations
    if (!static_x || !static_xb || !static_xb2 || !static_hb || !static_hb2 ||
        !static_q || !static_k || !static_v || !static_key_cache || !static_value_cache ||
        !static_att || !static_logits) {
        Print(L"[ERROR] Arena allocation failed\r\n");
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Zero KV cache
    for (UINTN i = 0; i < p->n_layers * p->seq_len * kv_dim; i++) {
        static_key_cache[i] = 0.0f;
        static_value_cache[i] = 0.0f;
    }
    
    // Point RunState to buffers
    s->x = static_x;
    s->xb = static_xb;
    s->xb2 = static_xb2;
    s->hb = static_hb;
    s->hb2 = static_hb2;
    s->q = static_q;
    s->k = static_k;
    s->v = static_v;
    s->key_cache = static_key_cache;
    s->value_cache = static_value_cache;
    s->att = static_att;
    s->logits = static_logits;
    
    Print(L"✅ RunState buffers allocated from arenas\r\n");
    return EFI_SUCCESS;
}

// ============================================================================
// ÉTAPE 5: Ajouter sentinel_cycle_start/end dans la boucle de génération
// ============================================================================

// Dans generate() ou la boucle principale, ajouter:

    for (int pos = 0; pos < steps; pos++) {
        // Start inference cycle monitoring
        sentinel_cycle_start();
        
        // ... forward pass ...
        float* logits = forward(&transformer, token, pos);
        
        // ... sampling ...
        
        // End inference cycle
        sentinel_cycle_end();
    }

// ============================================================================
// ÉTAPE 6: Afficher les statistiques à la fin
// ============================================================================

// Avant la sortie de efi_main(), ajouter:

    Print(L"\r\n");
    Print(L"╔══════════════════════════════════════════════════════════╗\r\n");
    Print(L"║        🏎️  LLM-KERNEL SHUTDOWN                          ║\r\n");
    Print(L"╚══════════════════════════════════════════════════════════╝\r\n");
    Print(L"\r\n");
    
    sentinel_print_status();
    zones_print_layout();
    
    sentinel_shutdown();
    
    Print(L"✅ LLM-Kernel shut down cleanly\r\n");
    Print(L"\r\n");

// ============================================================================
// RÉSUMÉ DES MODIFICATIONS
// ============================================================================
//
// 1. ✅ Includes ajoutés (memory_zones.h, memory_sentinel.h)
// 2. ✅ Initialisation du kernel dans efi_main
// 3. ✅ Allocation des poids depuis ARENA_WEIGHTS
// 4. ✅ Allocation des buffers depuis ARENA_SCRATCH/KV_CACHE/OUTPUT
// 5. ✅ Monitoring des cycles d'inférence
// 6. ✅ Affichage des statistiques
//
// AVANTAGES:
// - Mémoire partitionnée (weights/scratch/kv/output)
// - Surveillance permanente par Memory Sentinel
// - Détection des dépassements de buffer
// - Protection contre accès Zone A/C
// - Statistiques détaillées
// - Architecture Formule 1 🏎️
//
// ============================================================================
