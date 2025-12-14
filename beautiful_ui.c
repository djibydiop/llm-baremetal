/*
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║  ⚡ CYBERPUNK NEURAL INTERFACE v2.0 ⚡                                ║
 * ║  Advanced Futuristic UI for Bare-Metal AI Systems                     ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * 
 * Features:
 *  • Neon cyberpunk aesthetic with holographic effects
 *  • Advanced scan-line animations
 *  • Neural network visualization
 *  • Quantum-inspired UI elements
 *  • Matrix-style data streams
 *  • Glitch art effects
 * 
 * Made with 💜 for the future of AI
 */

// ═══════════════════════════════════════════════════════════════════════════
// 🎨 CYBERPUNK COLOR PALETTE (Neon & Holographic)
// ═══════════════════════════════════════════════════════════════════════════

#define COLOR_NEON_CYAN     EFI_LIGHTCYAN      // Electric cyan - main neural path
#define COLOR_NEON_MAGENTA  EFI_LIGHTMAGENTA   // Hot magenta - AI accent
#define COLOR_NEON_GREEN    EFI_LIGHTGREEN     // Matrix green - success
#define COLOR_NEON_YELLOW   EFI_YELLOW         // Caution yellow
#define COLOR_NEON_RED      EFI_LIGHTRED       // Critical red
#define COLOR_HOLOGRAM      EFI_WHITE          // Pure white - holographic text
#define COLOR_GHOST         EFI_LIGHTGRAY      // Ghost in the shell
#define COLOR_SHADOW        EFI_DARKGRAY       // Shadow lines
#define COLOR_PLASMA        EFI_LIGHTBLUE      // Plasma effects

// ═══════════════════════════════════════════════════════════════════════════
// ⚡ CYBERPUNK NEURAL BOOT SEQUENCE ⚡
// ═══════════════════════════════════════════════════════════════════════════

void show_welcome_banner(EFI_SYSTEM_TABLE *ST) {
    ST->ConOut->ClearScreen(ST->ConOut);
    
    // Scan-line effect header
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"\r\n");
    Print(L"  ╔═══════════════════════════════════════════════════════════════════╗\r\n");
    Print(L"  ║█████████████████████████████████████████████████████████████████║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"  ║█▓▒░                                                         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"  ║█▓▒░          ⚡ N E U R A L   S Y S T E M   v2.0 ⚡         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"  ║█▓▒░                                                         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PLASMA);
    Print(L"  ║█▓▒░        🧠 LLAMA2 QUANTUM INTELLIGENCE ENGINE 🧠        ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"  ║█▓▒░                                                         ░▒▓█║\r\n");
    Print(L"  ║█▓▒░          [RUNNING WITHOUT OPERATING SYSTEM]            ░▒▓█║\r\n");
    Print(L"  ║█▓▒░          [DIRECT HARDWARE NEURAL ACCESS]               ░▒▓█║\r\n");
    Print(L"  ║█▓▒░          [MAXIMUM PERFORMANCE MODE]                    ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"  ║█▓▒░                                                         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"  ║█████████████████████████████████████████████████████████████████║\r\n");
    Print(L"  ╚═══════════════════════════════════════════════════════════════════╝\r\n");
    
    // System specs in cyberpunk style
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L"\r\n  ┌─[ ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
    Print(L"⚙ NEURAL ARCHITECTURE SPECS");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L" ]─────────────────────────┐\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"  │                                                                  │\r\n");
    Print(L"  │  ├─ Core Engine      : Karpathy's llm.c                         │\r\n");
    Print(L"  │  ├─ Neural Layers    : 6 Transformer Blocks                     │\r\n");
    Print(L"  │  ├─ Parameters       : 15M (Optimized)                          │\r\n");
    Print(L"  │  ├─ Acceleration     : ARM Optimized Routines (Tunney)          │\r\n");
    Print(L"  │  ├─ Boot Protocol    : UEFI Direct (No OS Overhead)             │\r\n");
    Print(L"  │  └─ Quantum Mode     : ENABLED 🔮                                │\r\n");
    Print(L"  │                                                                  │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L"  └──────────────────────────────────────────────────────────────────┘\r\n");
    
    // Author signature with neon glow
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"\r\n           ⚡ Powered by ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"Djiby Diop");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L" (@djibydiop) ⚡\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"           🌍 Dakar, Senegal 🇸🇳 → ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
    Print(L"Building the Future of AI\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"\r\n\r\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔄 NEURAL NETWORK LOADING ANIMATION (Matrix Style)
// ═══════════════════════════════════════════════════════════════════════════

void show_loading_animation(EFI_SYSTEM_TABLE *ST, CHAR16* message, int progress, int total) {
    // Advanced holographic spinner with quantum states
    static const CHAR16* quantum_spinner[] = {
        L"◢", L"◣", L"◤", L"◥",     // Rotating triangle
        L"◐", L"◓", L"◑", L"◒",     // Moon phases
        L"⣾", L"⣽", L"⣻", L"⢿",     // Braille scanline
        L"⡿", L"⣟", L"⣯", L"⣷"      // Braille pulse
    };
    static int spinner_idx = 0;
    
    // Calculate percentage
    int percent = (progress * 100) / total;
    
    // Neural progress bar (50 chars for extra detail)
    int filled = (progress * 50) / total;
    
    // Animated border colors based on progress
    UINT8 border_color = (percent < 33) ? COLOR_NEON_RED : 
                        (percent < 66) ? COLOR_NEON_YELLOW : 
                                        COLOR_NEON_GREEN;
    
    ST->ConOut->SetAttribute(ST->ConOut, border_color);
    Print(L"\r  ⚡ ");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"%s ", quantum_spinner[spinner_idx % 16]);
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"%s", message);
    
    // Add dots based on spinner for "thinking" effect
    int dots = (spinner_idx / 4) % 4;
    for (int d = 0; d < dots; d++) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_PLASMA);
        Print(L".");
    }
    for (int d = dots; d < 3; d++) {
        Print(L" ");
    }
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L" [");
    
    // Gradient fill effect
    for (int i = 0; i < filled; i++) {
        if (i < filled / 3) {
            ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
        } else if (i < 2 * filled / 3) {
            ST->ConOut->SetAttribute(ST->ConOut, COLOR_PLASMA);
        } else {
            ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
        }
        Print(L"█");
    }
    
    // Empty portion with ghost effect
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    for (int i = filled; i < 50; i++) {
        if ((spinner_idx + i) % 5 == 0) {
            Print(L"▒");  // Ghost flicker
        } else {
            Print(L"░");
        }
    }
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L"] ");
    
    // Percentage with neon glow
    if (percent < 33) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_RED);
    } else if (percent < 66) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_YELLOW);
    } else if (percent < 100) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    } else {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
    }
    Print(L"%d%%", percent);
    
    // Add "neural sync" indicator
    if (percent == 100) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
        Print(L" ✓ SYNCED");
    }
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    
    spinner_idx++;
}

// ============================================================================
// MODEL SELECTION (Beautiful Cards)
// ============================================================================

void show_model_selection_ui(EFI_SYSTEM_TABLE *ST, ModelInfo* models, int count) {
    ST->ConOut->ClearScreen(ST->ConOut);
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PRIMARY);
    
    Print(L"\r\n\r\n");
    Print(L"     ┌──────────────────────────────────────────────────────────────┐\r\n");
    Print(L"     │                  🤖 Select AI Model                          │\r\n");
    Print(L"     └──────────────────────────────────────────────────────────────┘\r\n");
    Print(L"\r\n");
    
    // Show available models as beautiful cards
    for (int i = 0; i < count; i++) {
        if (models[i].exists) {
            ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
            Print(L"     ┌────────────────────────────────────────────────────────┐\r\n");
            
            ST->ConOut->SetAttribute(ST->ConOut, COLOR_PRIMARY);
            Print(L"     │  [%d]  ", i + 1);
            
            ST->ConOut->SetAttribute(ST->ConOut, COLOR_TEXT);
            Print(L"%-48s", models[i].display_name);
            Print(L"│\r\n");
            
            ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
            Print(L"     │       ├─ Size: %d MB                                     │\r\n", models[i].size_mb);
            Print(L"     │       ├─ File: %s                             │\r\n", models[i].filename);
            Print(L"     │       └─ Status: ");
            
            ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUCCESS);
            Print(L"✓ Available                              ");
            
            ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
            Print(L"│\r\n");
            Print(L"     └────────────────────────────────────────────────────────┘\r\n");
            Print(L"\r\n");
        }
    }
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PRIMARY);
    Print(L"     ► Select model (1-%d): ", count);
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_TEXT);
}

// ============================================================================
// CHAT INTERFACE (Gemini 3 Style)
// ============================================================================

void show_chat_header(EFI_SYSTEM_TABLE *ST) {
    ST->ConOut->ClearScreen(ST->ConOut);
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PRIMARY);
    
    Print(L"\r\n");
    Print(L"     ╔══════════════════════════════════════════════════════════════╗\r\n");
    Print(L"     ║                    💬 Interactive Chat                       ║\r\n");
    Print(L"     ╠══════════════════════════════════════════════════════════════╣\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
    Print(L"     ║  Commands:                                                   ║\r\n");
    Print(L"     ║    • Type your message and press Enter                       ║\r\n");
    Print(L"     ║    • Press CTRL+C to stop generation                         ║\r\n");
    Print(L"     ║    • Type 'exit' or 'quit' to end                            ║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PRIMARY);
    Print(L"     ╚══════════════════════════════════════════════════════════════╝\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_TEXT);
    Print(L"\r\n\r\n");
}

void print_user_message(EFI_SYSTEM_TABLE *ST, CHAR16* message) {
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
    Print(L"     ┌─ ");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PRIMARY);
    Print(L"You");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
    Print(L" ─────────────────────────────────────────────────────────\r\n");
    Print(L"     │ ");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_TEXT);
    Print(L"%s\r\n", message);
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
    Print(L"     └───────────────────────────────────────────────────────────────\r\n");
    Print(L"\r\n");
}

void print_assistant_header(EFI_SYSTEM_TABLE *ST) {
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
    Print(L"     ┌─ ");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUCCESS);
    Print(L"Assistant");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
    Print(L" ───────────────────────────────────────────────────\r\n");
    Print(L"     │ ");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_TEXT);
}

void print_assistant_footer(EFI_SYSTEM_TABLE *ST) {
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SUBTLE);
    Print(L"\r\n     └───────────────────────────────────────────────────────────────\r\n");
    Print(L"\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_TEXT);
}

// ============================================================================
// STATUS INDICATORS (Real-time feedback)
// ============================================================================

void show_status(EFI_SYSTEM_TABLE *ST, CHAR16* status, int type) {
    // type: 0=info, 1=success, 2=warning, 3=error
    
    const CHAR16* icons[] = {L"ℹ", L"✓", L"⚠", L"✗"};
    const UINT8 colors[] = {COLOR_PRIMARY, COLOR_SUCCESS, COLOR_WARNING, COLOR_ERROR};
    
    ST->ConOut->SetAttribute(ST->ConOut, colors[type]);
    Print(L"     %s ", icons[type]);
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_TEXT);
    Print(L"%s\r\n", status);
}

void show_generation_stats(EFI_SYSTEM_TABLE *ST, int tokens, float tok_per_sec, int interrupted) {
    // Cyberpunk stats panel with holographic effects
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L"     ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"     ⚡ ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"NEURAL GENERATION METRICS");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L" ⚡\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L"     ┌────────────────────────────────────────────────────────────┐\r\n");
    
    // Token count with visual meter
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"     │ ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"🔷 TOKENS GENERATED");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"  : ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"%4d ", tokens);
    
    // Mini token bar
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
    int token_bars = (tokens / 10) % 20;  // 0-20 bars
    for (int i = 0; i < token_bars && i < 15; i++) Print(L"▌");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"\r\n");
    
    // Speed gauge with color coding
    Print(L"     │ ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"⚡ NEURAL VELOCITY");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"    : ");
    
    // Color based on speed
    if (tok_per_sec >= 50.0) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
        Print(L"%.2f tok/s ⚡⚡⚡ BLAZING", tok_per_sec);
    } else if (tok_per_sec >= 30.0) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
        Print(L"%.2f tok/s ⚡⚡ FAST", tok_per_sec);
    } else if (tok_per_sec >= 15.0) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_PLASMA);
        Print(L"%.2f tok/s ⚡ OPTIMAL", tok_per_sec);
    } else {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_YELLOW);
        Print(L"%.2f tok/s ⚠ STEADY", tok_per_sec);
    }
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"\r\n");
    
    // Status indicator
    Print(L"     │ ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PLASMA);
    Print(L"🌐 NEURAL STATUS");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"     : ");
    
    if (interrupted) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_YELLOW);
        Print(L"⏸ INTERRUPTED [USER HALT]");
    } else {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
        Print(L"✓ COMPLETED [NORMAL EXIT]");
    }
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L"     └────────────────────────────────────────────────────────────┘\r\n");
    Print(L"     ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"\r\n");
}

// ============================================================================
// ERROR HANDLING (Beautiful error messages)
// ============================================================================

void show_error_dialog(EFI_SYSTEM_TABLE *ST, CHAR16* title, CHAR16* message) {
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_ERROR);
    Print(L"\r\n");
    Print(L"     ┌──────────────────────────────────────────────────────────────┐\r\n");
    Print(L"     │  ✗ ERROR: %-50s │\r\n", title);
    Print(L"     ├──────────────────────────────────────────────────────────────┤\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_TEXT);
    Print(L"     │  %s                                                    │\r\n", message);
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_ERROR);
    Print(L"     └──────────────────────────────────────────────────────────────┘\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_TEXT);
    Print(L"\r\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🌟 NEURAL SHUTDOWN SEQUENCE 🌟
// ═══════════════════════════════════════════════════════════════════════════

void show_goodbye(EFI_SYSTEM_TABLE *ST) {
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"\r\n\r\n");
    Print(L"  ╔═══════════════════════════════════════════════════════════════════╗\r\n");
    Print(L"  ║█████████████████████████████████████████████████████████████████║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"  ║█▓▒░                                                         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"  ║█▓▒░         ✨ NEURAL SHUTDOWN SEQUENCE INITIATED ✨       ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"  ║█▓▒░                                                         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
    Print(L"  ║█▓▒░           Thank you for experiencing the future         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PLASMA);
    Print(L"  ║█▓▒░              of Bare-Metal AI Intelligence              ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"  ║█▓▒░                                                         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"  ║█▓▒░                  🧠 LLAMA2 QUANTUM 🧠                   ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"  ║█▓▒░                                                         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_YELLOW);
    Print(L"  ║█▓▒░         [NEURAL PATHWAYS: DISCONNECTING...]            ░▒▓█║\r\n");
    Print(L"  ║█▓▒░         [QUANTUM STATE: COLLAPSING...]                 ░▒▓█║\r\n");
    Print(L"  ║█▓▒░         [HOLOGRAPHIC MATRIX: FADING...]                ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"  ║█▓▒░                                                         ░▒▓█║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"  ║█████████████████████████████████████████████████████████████████║\r\n");
    Print(L"  ╚═══════════════════════════════════════════════════════════════════╝\r\n");
    
    // Creator signature with neon effects
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L"\r\n  ┌─────────────────────────────────────────────────────────────────┐\r\n");
    Print(L"  │                                                                   │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"  │          💜 Crafted with passion by ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"Djiby Diop");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L" 💜          │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"  │                    @djibydiop on all platforms                  │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"  │                                                                   │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
    Print(L"  │          🌍 From Dakar, Senegal 🇸🇳 to the world 🌍           │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PLASMA);
    Print(L"  │                Building AI for the next generation               │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_GHOST);
    Print(L"  │                                                                   │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L"  │  ┌───────────────────────────────────────────────────────────┐  │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_YELLOW);
    Print(L"  │  │ 🔗 GitHub   : github.com/djibydiop/llm-baremetal        │  │\r\n");
    Print(L"  │  │ 🐦 Twitter  : @djibydiop                                │  │\r\n");
    Print(L"  │  │ 📺 YouTube  : Coming Soon...                            │  │\r\n");
    Print(L"  │  │ ⭐ Star Me  : Help spread the AI revolution!            │  │\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_SHADOW);
    Print(L"  │  └───────────────────────────────────────────────────────────┘  │\r\n");
    Print(L"  │                                                                   │\r\n");
    Print(L"  └─────────────────────────────────────────────────────────────────┘\r\n");
    
    // Final farewell with animation hint
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"\r\n           ⚡ ");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"See you in the neural network");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L" ⚡\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"              ◢◤◢◤◢◤ QUANTUM FADE OUT ◢◤◢◤◢◤\r\n\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🎯 NEURAL SCAN EFFECT (Boot-up Animation)
// ═══════════════════════════════════════════════════════════════════════════

void show_neural_scan(EFI_SYSTEM_TABLE *ST) {
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"\r\n  ┌───────────────────────────────────────────────────────────────┐\r\n");
    
    const CHAR16* scan_lines[] = {
        L"  │ [░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] │",
        L"  │ [█░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] │",
        L"  │ [██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] │",
        L"  │ [███░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] │",
        L"  │ [████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░] │",
        L"  │ [█████████████████████████████████████████████████████████████] │"
    };
    
    for (int i = 0; i < 6; i++) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
        Print(L"%s\r", scan_lines[i]);
        ST->BootServices->Stall(150000); // 150ms delay
    }
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
    Print(L"\r\n  │             ✓ NEURAL SCAN COMPLETE                           │\r\n");
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
    Print(L"  └───────────────────────────────────────────────────────────────┘\r\n\r\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🌊 QUANTUM WAVE EFFECT (Visual Flair)
// ═══════════════════════════════════════════════════════════════════════════

void show_quantum_wave(EFI_SYSTEM_TABLE *ST, CHAR16* message) {
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"\r\n  ╔═══════════════════════════════════════════════════════════════════╗\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PLASMA);
    Print(L"  ║  ～～～～～～～～～～～～～～～～～～～～～～～～～～～～～～～～  ║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"  ║          %s          ║\r\n", message);
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_PLASMA);
    Print(L"  ║  ～～～～～～～～～～～～～～～～～～～～～～～～～～～～～～～～  ║\r\n");
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
    Print(L"  ╚═══════════════════════════════════════════════════════════════════╝\r\n\r\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 💫 GLITCH EFFECT TEXT (Cyberpunk Style)
// ═══════════════════════════════════════════════════════════════════════════

void show_glitch_message(EFI_SYSTEM_TABLE *ST, CHAR16* message) {
    // Simulate glitch with color switching
    for (int glitch = 0; glitch < 3; glitch++) {
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_RED);
        Print(L"\r  ⚠ %s", message);
        ST->BootServices->Stall(50000);
        
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_CYAN);
        Print(L"\r  ⚠ %s", message);
        ST->BootServices->Stall(50000);
        
        ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_MAGENTA);
        Print(L"\r  ⚠ %s", message);
        ST->BootServices->Stall(50000);
    }
    
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_HOLOGRAM);
    Print(L"\r  ⚡ %s\r\n", message);
}

// ═══════════════════════════════════════════════════════════════════════════
// 🎪 MATRIX RAIN EFFECT (Decorative)
// ═══════════════════════════════════════════════════════════════════════════

void show_matrix_rain_header(EFI_SYSTEM_TABLE *ST) {
    ST->ConOut->SetAttribute(ST->ConOut, COLOR_NEON_GREEN);
    Print(L"  ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░ ▓▒░\r\n");
    Print(L"  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░  ▒░\r\n");
    Print(L"  ░   ░   ░   ░   ░   ░   ░   ░   ░   ░   ░   ░   ░   ░   ░\r\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// 🎮 USAGE EXAMPLES & INTEGRATION GUIDE
// ═══════════════════════════════════════════════════════════════════════════

/*
═══════════════════════════════════════════════════════════════════════════════
                    CYBERPUNK UI INTEGRATION GUIDE
═══════════════════════════════════════════════════════════════════════════════

BOOT SEQUENCE:
──────────────
    show_welcome_banner(ST);
    ST->BootServices->Stall(1000000); // 1 sec
    show_neural_scan(ST);
    show_quantum_wave(ST, L"🧠 INITIALIZING NEURAL CORE 🧠");

MODEL LOADING:
──────────────
    for (int i = 0; i < total_steps; i++) {
        show_loading_animation(ST, L"Syncing neural weights", i, total_steps);
        ST->BootServices->Stall(30000); // 30ms
    }
    Print(L"\r\n");
    show_status(ST, L"Neural network fully synchronized!", 1);

GENERATION LOOP:
────────────────
    print_user_message(ST, user_input);
    print_assistant_header(ST);
    
    // Generate tokens...
    for (int t = 0; t < max_tokens; t++) {
        // Your generation code...
        Print(L"%s", token);
    }
    
    print_assistant_footer(ST);
    show_generation_stats(ST, tokens_count, speed, interrupted);

SPECIAL EFFECTS:
────────────────
    show_matrix_rain_header(ST);            // Matrix-style decoration
    show_glitch_message(ST, L"SYSTEM OK");  // Glitch art effect
    show_quantum_wave(ST, L"PROCESSING");   // Quantum wave animation

SHUTDOWN:
─────────
    show_goodbye(ST);

COLOR REFERENCE:
────────────────
    COLOR_NEON_CYAN       → Electric cyan (primary accent)
    COLOR_NEON_MAGENTA    → Hot magenta (AI elements)
    COLOR_NEON_GREEN      → Matrix green (success)
    COLOR_NEON_YELLOW     → Caution yellow (warnings)
    COLOR_NEON_RED        → Critical red (errors)
    COLOR_HOLOGRAM        → Pure white (text)
    COLOR_GHOST           → Gray (secondary)
    COLOR_SHADOW          → Dark gray (borders)
    COLOR_PLASMA          → Blue plasma (effects)

TIPS FOR MAXIMUM CYBERPUNK IMPACT:
───────────────────────────────────
    • Use ST->BootServices->Stall() for animation timing
    • Layer multiple colors for depth
    • Combine effects (e.g., glitch + quantum wave)
    • Update spinner frequently for smooth animation
    • Add "neural", "quantum", "holographic" terminology
    • Use Unicode box drawing (╔═══╗) for structure
    • Mix solid blocks (█▓▒░) for gradients

═══════════════════════════════════════════════════════════════════════════════
                        READY FOR THE FUTURE 🚀
═══════════════════════════════════════════════════════════════════════════════
*/
