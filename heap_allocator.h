// heap_allocator.h - Simple heap allocator for UEFI bare-metal
// Provides malloc/free-like interface over UEFI memory pools

#ifndef HEAP_ALLOCATOR_H
#define HEAP_ALLOCATOR_H

#include <efi.h>
#include <efilib.h>

// ============================================================================
// HEAP ALLOCATOR - Simple memory manager for bare-metal LLM
// ============================================================================
// Uses UEFI AllocatePool/FreePool with tracking for debugging
// Supports alignment, reallocation, and memory statistics

typedef struct {
    UINTN total_allocated;      // Total bytes allocated
    UINTN total_freed;           // Total bytes freed
    UINTN current_usage;         // Current memory in use
    UINTN peak_usage;            // Maximum memory used
    UINTN allocation_count;      // Number of active allocations
    UINTN allocation_failures;   // Number of failed allocations
} HeapStats;

// Memory block header (stored before each allocation)
typedef struct MemBlock {
    UINTN size;                  // Size of allocation (excluding header)
    UINT32 magic;                // Magic number for corruption detection
    struct MemBlock* next;       // Next block in allocated list
    UINT32 alignment_padding;    // Padding for alignment
} MemBlock;

#define HEAP_MAGIC 0xDEADBEEF
#define HEAP_FREED_MAGIC 0xFEEDFACE

// Global heap state
typedef struct {
    EFI_BOOT_SERVICES* bs;       // UEFI Boot Services
    MemBlock* allocated_list;    // Linked list of allocated blocks
    HeapStats stats;             // Statistics
    BOOLEAN initialized;         // Is heap initialized?
    BOOLEAN debug_mode;          // Print debug messages?
} HeapAllocator;

// ============================================================================
// HEAP API
// ============================================================================

// Initialize heap allocator (call once at startup)
EFI_STATUS heap_init(EFI_BOOT_SERVICES* bs, BOOLEAN debug);

// Allocate memory (like malloc)
void* heap_alloc(UINTN size);

// Allocate aligned memory (for SIMD operations)
void* heap_alloc_aligned(UINTN size, UINTN alignment);

// Allocate and zero memory (like calloc)
void* heap_calloc(UINTN count, UINTN element_size);

// Reallocate memory (like realloc)
void* heap_realloc(void* ptr, UINTN new_size);

// Free memory (like free)
void heap_free(void* ptr);

// Get heap statistics
HeapStats heap_get_stats(void);

// Print heap statistics (for debugging)
void heap_print_stats(void);

// Check heap integrity (detect corruption)
BOOLEAN heap_check_integrity(void);

// Dump all allocations (for leak detection)
void heap_dump_allocations(void);

// ============================================================================
// IMPLEMENTATION
// ============================================================================

static HeapAllocator g_heap = {0};

EFI_STATUS heap_init(EFI_BOOT_SERVICES* bs, BOOLEAN debug) {
    if (g_heap.initialized) {
        return EFI_ALREADY_STARTED;
    }
    
    g_heap.bs = bs;
    g_heap.allocated_list = NULL;
    g_heap.stats.total_allocated = 0;
    g_heap.stats.total_freed = 0;
    g_heap.stats.current_usage = 0;
    g_heap.stats.peak_usage = 0;
    g_heap.stats.allocation_count = 0;
    g_heap.stats.allocation_failures = 0;
    g_heap.debug_mode = debug;
    g_heap.initialized = TRUE;
    
    if (debug) {
        Print(L"[HEAP] Initialized\r\n");
    }
    
    return EFI_SUCCESS;
}

void* heap_alloc(UINTN size) {
    if (!g_heap.initialized) {
        return NULL;
    }
    
    if (size == 0) {
        return NULL;
    }
    
    // Allocate block + header
    UINTN total_size = sizeof(MemBlock) + size;
    MemBlock* block = NULL;
    
    EFI_STATUS status = uefi_call_wrapper(g_heap.bs->AllocatePool, 3,
        EfiLoaderData, total_size, (void**)&block);
    
    if (EFI_ERROR(status) || block == NULL) {
        g_heap.stats.allocation_failures++;
        if (g_heap.debug_mode) {
            Print(L"[HEAP] Allocation failed: %lu bytes\r\n", size);
        }
        return NULL;
    }
    
    // Initialize block header
    block->size = size;
    block->magic = HEAP_MAGIC;
    block->next = g_heap.allocated_list;
    block->alignment_padding = 0;
    
    // Add to allocated list
    g_heap.allocated_list = block;
    
    // Update statistics
    g_heap.stats.total_allocated += size;
    g_heap.stats.current_usage += size;
    g_heap.stats.allocation_count++;
    
    if (g_heap.stats.current_usage > g_heap.stats.peak_usage) {
        g_heap.stats.peak_usage = g_heap.stats.current_usage;
    }
    
    if (g_heap.debug_mode) {
        Print(L"[HEAP] Alloc: %lu bytes @ 0x%lx\r\n", size, (UINT64)block + sizeof(MemBlock));
    }
    
    // Return pointer after header
    return (void*)((UINT8*)block + sizeof(MemBlock));
}

void* heap_alloc_aligned(UINTN size, UINTN alignment) {
    if (!g_heap.initialized || size == 0 || alignment == 0) {
        return NULL;
    }
    
    // Allocate extra space for alignment
    UINTN padded_size = size + alignment + sizeof(MemBlock);
    void* raw_ptr = heap_alloc(padded_size);
    
    if (raw_ptr == NULL) {
        return NULL;
    }
    
    // Calculate aligned pointer
    UINTN addr = (UINTN)raw_ptr;
    UINTN aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    
    // Store original pointer before aligned pointer
    *((void**)(aligned_addr - sizeof(void*))) = raw_ptr;
    
    return (void*)aligned_addr;
}

void* heap_calloc(UINTN count, UINTN element_size) {
    UINTN size = count * element_size;
    void* ptr = heap_alloc(size);
    
    if (ptr != NULL) {
        // Zero memory
        for (UINTN i = 0; i < size; i++) {
            ((UINT8*)ptr)[i] = 0;
        }
    }
    
    return ptr;
}

void* heap_realloc(void* ptr, UINTN new_size) {
    if (ptr == NULL) {
        return heap_alloc(new_size);
    }
    
    if (new_size == 0) {
        heap_free(ptr);
        return NULL;
    }
    
    // Get old block header
    MemBlock* old_block = (MemBlock*)((UINT8*)ptr - sizeof(MemBlock));
    
    // Verify magic
    if (old_block->magic != HEAP_MAGIC) {
        if (g_heap.debug_mode) {
            Print(L"[HEAP] Realloc: Invalid magic 0x%x\r\n", old_block->magic);
        }
        return NULL;
    }
    
    // If new size fits in old block, keep it
    if (new_size <= old_block->size) {
        return ptr;
    }
    
    // Allocate new block
    void* new_ptr = heap_alloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    // Copy old data
    UINTN copy_size = (new_size < old_block->size) ? new_size : old_block->size;
    for (UINTN i = 0; i < copy_size; i++) {
        ((UINT8*)new_ptr)[i] = ((UINT8*)ptr)[i];
    }
    
    // Free old block
    heap_free(ptr);
    
    return new_ptr;
}

void heap_free(void* ptr) {
    if (!g_heap.initialized || ptr == NULL) {
        return;
    }
    
    // Get block header
    MemBlock* block = (MemBlock*)((UINT8*)ptr - sizeof(MemBlock));
    
    // Verify magic
    if (block->magic != HEAP_MAGIC) {
        if (g_heap.debug_mode) {
            Print(L"[HEAP] Free: Invalid magic 0x%x @ 0x%lx\r\n", 
                  block->magic, (UINT64)ptr);
        }
        return;
    }
    
    // Remove from allocated list
    if (g_heap.allocated_list == block) {
        g_heap.allocated_list = block->next;
    } else {
        MemBlock* prev = g_heap.allocated_list;
        while (prev != NULL && prev->next != block) {
            prev = prev->next;
        }
        if (prev != NULL) {
            prev->next = block->next;
        }
    }
    
    // Update statistics
    g_heap.stats.total_freed += block->size;
    g_heap.stats.current_usage -= block->size;
    g_heap.stats.allocation_count--;
    
    if (g_heap.debug_mode) {
        Print(L"[HEAP] Free: %lu bytes @ 0x%lx\r\n", block->size, (UINT64)ptr);
    }
    
    // Mark as freed (helps detect double-free)
    block->magic = HEAP_FREED_MAGIC;
    
    // Free to UEFI
    uefi_call_wrapper(g_heap.bs->FreePool, 1, block);
}

HeapStats heap_get_stats(void) {
    return g_heap.stats;
}

void heap_print_stats(void) {
    Print(L"\r\n[HEAP] Statistics:\r\n");
    Print(L"  Total allocated:   %lu bytes\r\n", g_heap.stats.total_allocated);
    Print(L"  Total freed:       %lu bytes\r\n", g_heap.stats.total_freed);
    Print(L"  Current usage:     %lu bytes (%.2f MB)\r\n", 
          g_heap.stats.current_usage,
          (float)g_heap.stats.current_usage / (1024.0f * 1024.0f));
    Print(L"  Peak usage:        %lu bytes (%.2f MB)\r\n",
          g_heap.stats.peak_usage,
          (float)g_heap.stats.peak_usage / (1024.0f * 1024.0f));
    Print(L"  Active allocations: %lu\r\n", g_heap.stats.allocation_count);
    Print(L"  Failed allocations: %lu\r\n", g_heap.stats.allocation_failures);
}

BOOLEAN heap_check_integrity(void) {
    if (!g_heap.initialized) {
        return FALSE;
    }
    
    UINTN checked_blocks = 0;
    UINTN corrupted_blocks = 0;
    
    MemBlock* block = g_heap.allocated_list;
    while (block != NULL) {
        checked_blocks++;
        
        if (block->magic != HEAP_MAGIC) {
            corrupted_blocks++;
            if (g_heap.debug_mode) {
                Print(L"[HEAP] Corrupted block @ 0x%lx (magic=0x%x)\r\n",
                      (UINT64)block, block->magic);
            }
        }
        
        block = block->next;
    }
    
    if (g_heap.debug_mode) {
        Print(L"[HEAP] Integrity check: %lu blocks, %lu corrupted\r\n",
              checked_blocks, corrupted_blocks);
    }
    
    return (corrupted_blocks == 0);
}

void heap_dump_allocations(void) {
    Print(L"\r\n[HEAP] Active allocations:\r\n");
    
    UINTN count = 0;
    UINTN total_size = 0;
    
    MemBlock* block = g_heap.allocated_list;
    while (block != NULL) {
        count++;
        total_size += block->size;
        
        Print(L"  [%lu] 0x%lx: %lu bytes\r\n",
              count, (UINT64)block + sizeof(MemBlock), block->size);
        
        block = block->next;
    }
    
    Print(L"Total: %lu allocations, %lu bytes\r\n", count, total_size);
}

#endif // HEAP_ALLOCATOR_H
