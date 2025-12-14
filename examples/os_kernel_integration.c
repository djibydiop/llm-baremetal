/**
 * ============================================================================
 * EXAMPLE 3: OS Kernel Integration
 * ============================================================================
 * 
 * Demonstrates how to integrate LLM as an operating system kernel service.
 * Perfect for custom OS projects like YamaOS, TractorOS, etc.
 * 
 * This example shows:
 * - Kernel service registration
 * - System call interface
 * - Process isolation
 * - Resource management
 * 
 * ============================================================================
 */

#include "../llm_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================== */
/* KERNEL SERVICE INTERFACE                                                   */
/* ========================================================================== */

#define KERNEL_SERVICE_LLM 0x42

typedef struct {
    int service_id;
    void* service_data;
    int (*init)(void);
    int (*shutdown)(void);
    int (*handle_syscall)(int cmd, void* args);
} KernelService;

// Global LLM service handle
static LLMHandle* g_llm_service = NULL;

/* ========================================================================== */
/* SYSTEM CALL INTERFACE                                                      */
/* ========================================================================== */

#define SYSCALL_LLM_GENERATE 1
#define SYSCALL_LLM_GET_STATS 2
#define SYSCALL_LLM_RESET 3

typedef struct {
    const char* prompt;
    char* output_buffer;
    int buffer_size;
    int max_tokens;
} SyscallLLMGenerate;

typedef struct {
    LLMStats stats;
} SyscallLLMStats;

/**
 * LLM system call handler
 */
int llm_syscall_handler(int cmd, void* args) {
    if (!g_llm_service) {
        return -1;
    }
    
    switch (cmd) {
        case SYSCALL_LLM_GENERATE: {
            SyscallLLMGenerate* req = (SyscallLLMGenerate*)args;
            return llm_generate(g_llm_service, 
                              req->prompt, 
                              req->output_buffer, 
                              req->buffer_size);
        }
        
        case SYSCALL_LLM_GET_STATS: {
            SyscallLLMStats* req = (SyscallLLMStats*)args;
            return llm_get_stats(g_llm_service, &req->stats);
        }
        
        case SYSCALL_LLM_RESET: {
            // Reset statistics
            return 0;
        }
        
        default:
            return -1;
    }
}

/**
 * Initialize LLM kernel service
 */
int kernel_llm_init(void) {
    printf("[KERNEL] Initializing LLM service...\n");
    
    LLMConfig config = {
        .model_path = "stories110M.bin",
        .tokenizer_path = "tokenizer.bin",
        .temperature = 0.9f,
        .max_tokens = 256,
        .seed = 42,
        .enable_neuronet = 1,  // Enable for kernel networking
        .neuronet_node_id = 0  // Kernel is node 0
    };
    
    g_llm_service = llm_init(&config);
    if (!g_llm_service) {
        printf("[KERNEL] Failed to initialize LLM service\n");
        return -1;
    }
    
    printf("[KERNEL] LLM service ready\n");
    return 0;
}

/**
 * Shutdown LLM kernel service
 */
int kernel_llm_shutdown(void) {
    printf("[KERNEL] Shutting down LLM service...\n");
    if (g_llm_service) {
        llm_cleanup(g_llm_service);
        g_llm_service = NULL;
    }
    return 0;
}

/**
 * Register LLM as kernel service
 */
KernelService llm_kernel_service = {
    .service_id = KERNEL_SERVICE_LLM,
    .service_data = NULL,
    .init = kernel_llm_init,
    .shutdown = kernel_llm_shutdown,
    .handle_syscall = llm_syscall_handler
};

/* ========================================================================== */
/* USER SPACE LIBRARY                                                         */
/* ========================================================================== */

/**
 * User-space wrapper for LLM system call
 */
int userspace_llm_generate(const char* prompt, char* output, int size) {
    SyscallLLMGenerate args = {
        .prompt = prompt,
        .output_buffer = output,
        .buffer_size = size,
        .max_tokens = 256
    };
    
    // In real OS, this would be: syscall(SYSCALL_LLM_GENERATE, &args)
    return llm_syscall_handler(SYSCALL_LLM_GENERATE, &args);
}

/**
 * User-space wrapper for stats
 */
int userspace_llm_stats(LLMStats* stats) {
    SyscallLLMStats args;
    int ret = llm_syscall_handler(SYSCALL_LLM_GET_STATS, &args);
    if (ret == 0) {
        *stats = args.stats;
    }
    return ret;
}

/* ========================================================================== */
/* EXAMPLE OS BOOT SEQUENCE                                                   */
/* ========================================================================== */

void simulate_kernel_boot() {
    printf("\n=== OS Kernel Boot Sequence ===\n\n");
    
    // Stage 1: Hardware initialization
    printf("[BOOT] Stage 1: Hardware init...\n");
    
    // Stage 2: Memory management
    printf("[BOOT] Stage 2: Memory manager init...\n");
    
    // Stage 3: Register kernel services
    printf("[BOOT] Stage 3: Registering kernel services...\n");
    printf("[BOOT] - Registering LLM service (ID: 0x%02X)\n", KERNEL_SERVICE_LLM);
    
    // Stage 4: Initialize services
    printf("[BOOT] Stage 4: Initializing services...\n");
    if (llm_kernel_service.init() == 0) {
        printf("[BOOT] - LLM service started successfully\n");
    } else {
        printf("[BOOT] - LLM service failed to start\n");
        return;
    }
    
    // Stage 5: Start userspace
    printf("[BOOT] Stage 5: Starting userspace...\n\n");
}

void simulate_userspace_process() {
    printf("=== Userspace Process ===\n\n");
    
    printf("[PROCESS-1] Making LLM system call...\n");
    
    char output[512];
    int ret = userspace_llm_generate(
        "In the operating system", 
        output, 
        sizeof(output)
    );
    
    if (ret == LLM_SUCCESS) {
        printf("[PROCESS-1] Generated: %s\n\n", output);
    } else {
        printf("[PROCESS-1] System call failed\n\n");
    }
    
    // Another process
    printf("[PROCESS-2] Getting LLM statistics...\n");
    LLMStats stats;
    ret = userspace_llm_stats(&stats);
    if (ret == 0) {
        printf("[PROCESS-2] Tokens generated: %llu\n", stats.tokens_generated);
        printf("[PROCESS-2] Tokens/sec: %.2f\n\n", stats.tokens_per_second);
    }
}

void simulate_kernel_shutdown() {
    printf("=== OS Shutdown ===\n\n");
    
    printf("[SHUTDOWN] Stopping services...\n");
    llm_kernel_service.shutdown();
    printf("[SHUTDOWN] Complete\n");
}

/* ========================================================================== */
/* MAIN                                                                       */
/* ========================================================================== */

int main() {
    printf("=== OS Kernel Integration Example ===\n");
    printf("Shows how to integrate LLM as a kernel service\n");
    printf("Use case: Custom OS like YamaOS, TractorOS, etc.\n\n");
    
    // Simulate OS lifecycle
    simulate_kernel_boot();
    simulate_userspace_process();
    simulate_kernel_shutdown();
    
    printf("\n=== Integration Points ===\n");
    printf("1. Register service during kernel init\n");
    printf("2. Expose system call interface\n");
    printf("3. Handle syscalls from userspace\n");
    printf("4. Cleanup during shutdown\n\n");
    
    printf("To integrate into your OS:\n");
    printf("- Copy kernel_llm_init() to your kernel startup\n");
    printf("- Register syscall handler in your syscall table\n");
    printf("- Provide userspace library wrapper\n");
    printf("- Add to kernel module list\n\n");
    
    return 0;
}
