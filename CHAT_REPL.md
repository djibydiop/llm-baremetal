# Interactive Chat REPL

## Overview

The LLM Bare-Metal bootloader now includes a full **Interactive Chat REPL** (Read-Eval-Print Loop) that allows real-time conversations with the language model directly on UEFI firmware.

## Features

### 🎯 Core Functionality
- **Real-time keyboard input**: Type naturally with backspace support
- **Conversation context**: Maintains full conversation history
- **Token management**: Tracks context window usage
- **Temperature control**: Adjust response creativity on-the-fly

### 💬 REPL Commands

| Command | Description | Example |
|---------|-------------|---------|
| `/help` | Show help menu with all commands | `/help` |
| `/quit` | Exit the REPL | `/quit` or `/q` |
| `/reset` | Clear conversation history | `/reset` |
| `/temp X` | Set temperature (0.0 - 2.0) | `/temp 0.7` |
| `/tokens` | Show context usage statistics | `/tokens` |

### 🌡️ Temperature Guide

| Value | Behavior | Use Case |
|-------|----------|----------|
| `0.0` | Deterministic (same output every time) | Factual answers, code generation |
| `0.7` | Balanced (recommended) | General conversation |
| `1.0` | Default creative | Story generation |
| `1.5+` | Very creative/random | Brainstorming, poetry |

## Usage

### Starting the REPL

1. **Boot from USB or QEMU**
   ```bash
   # QEMU test
   qemu-system-x86_64 -bios OVMF.fd -drive format=raw,file=qemu-test.img \
       -cpu Haswell -m 1024 -nographic
   ```

2. **Wait for model loading**
   - The bootloader auto-selects the optimal model for your RAM
   - Loading takes 30-90 seconds depending on model size

3. **Chat interface appears**
   ```
   ╔════════════════════════════════════════════════════════╗
   ║            INTERACTIVE CHAT REPL v2.0                  ║
   ╚════════════════════════════════════════════════════════╝
   
   Commands:
     /help    - Show this help message
     /reset   - Clear conversation history
     /quit    - Exit the REPL
     /temp X  - Set temperature (0.0 - 2.0)
     /tokens  - Show current token usage
   
   Context: 256 tokens max
   Temperature: 1.00
   ```

### Example Conversation

```
You: Hello! What's the weather like?
Assistant: I'm a language model running on bare metal, so I don't have access to real-time weather data...

You: Can you tell me a short story about a cat?
Assistant: Once upon a time, there was a clever cat named Whiskers who lived in a cozy cottage...

You: /tokens
📊 Token Usage:
   Used:      127 / 256 tokens (49.6%)
   Remaining: 129 tokens
   Turns:     2

You: /temp 0.5
✅ Temperature set to: 0.50

You: /quit
👋 Goodbye! Exiting REPL...
```

## Technical Details

### Architecture

```
┌─────────────────────────────────────────────────┐
│           User Input (Keyboard)                 │
└──────────────────┬──────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────┐
│        read_user_input() - EFI Console          │
│  - Line editing (backspace support)             │
│  - Enter to submit                              │
└──────────────────┬──────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────┐
│        Command Parser                           │
│  - /quit, /help, /reset, /temp, /tokens         │
└──────────────────┬──────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────┐
│        BPE Tokenization                         │
│  - encode_prompt() - Greedy longest match       │
└──────────────────┬──────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────┐
│        Conversation Context Manager             │
│  - Maintains token history (conversation_tokens)│
│  - Tracks position (conversation_pos)           │
│  - Auto-warns at 80% context                    │
└──────────────────┬──────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────┐
│        Transformer Forward Pass                 │
│  - AVX2 optimized matrix operations             │
│  - Temperature-based sampling                   │
│  - Token-by-token generation                    │
└──────────────────┬──────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────┐
│        Token Decoder & Display                  │
│  - decode_token() - BPE piece lookup            │
│  - Real-time character output                   │
└─────────────────────────────────────────────────┘
```

### Context Management

The REPL maintains conversation history in a fixed-size buffer:

```c
int conversation_tokens[512];     // Token sequence history
int conversation_pos = 0;         // Current position in context window
int max_context = transformer->config.seq_len;  // Usually 256 tokens
```

**Context Warnings:**
- **50% full**: No warning
- **80% full**: Yellow warning displayed
- **90% full**: Input rejected, user must `/reset`

### Memory Layout

```
Conversation Buffer:
┌────────────────────────────────────────────────┐
│ [BOS] [User1] [Response1] [User2] [Response2] │
│  └─1─┘ └─10─┘ └───80───┘ └─12─┘ └───90────┘  │
│                                                │
│  Total: 193 tokens / 256 max                   │
└────────────────────────────────────────────────┘
```

## Keyboard Input

### Supported Keys

| Key | Action |
|-----|--------|
| `A-Z`, `a-z` | Alphabetic input |
| `0-9` | Numeric input |
| `Space` | Whitespace |
| `!@#$%^&*()` | Punctuation |
| `Enter` | Submit message |
| `Backspace` | Delete last character |

### Limitations

- **No arrow keys**: Navigation not supported (UEFI limitation)
- **No copy/paste**: Console-only input
- **Max line length**: 512 characters
- **No multi-line**: Single line per message

## Performance

### Response Times

| Model | RAM | Tokens/sec (Real HW) | First Token Latency |
|-------|-----|----------------------|---------------------|
| `stories15M.bin` | 256 MB | 30-50 tok/s | 100-200 ms |
| `stories42M.bin` | 512 MB | 15-30 tok/s | 200-400 ms |
| `stories110M.bin` | 1024 MB | 8-15 tok/s | 400-800 ms |

*Note: QEMU is 10-20x slower than real hardware*

### Optimization Tips

1. **Use smaller models** for faster responses
2. **Lower temperature** (0.5-0.7) for deterministic, faster generation
3. **Reset context** frequently to avoid slowdown
4. **Real hardware** - QEMU is extremely slow

## Error Handling

### Context Overflow

```
⚠️  Context window nearly full!
   Use /reset to clear history, or /quit to exit.
```

**Solution**: Type `/reset` to start fresh conversation.

### Forward Pass Failure

```
[ERROR] Forward pass failed!
```

**Possible causes:**
- Out of memory (model too large for available RAM)
- Corrupted model file
- Hardware incompatibility

**Solution**: Reboot with smaller model or more RAM.

### Tokenization Failure

```
❌ Failed to encode input. Try again.
```

**Cause**: Input contains non-ASCII characters or tokenizer error.

**Solution**: Use only ASCII characters.

## Comparison to Old Demo Mode

| Feature | Old Demo Mode | New Chat REPL |
|---------|---------------|---------------|
| User input | ❌ No (auto-demo only) | ✅ Yes (real-time) |
| Commands | ❌ None | ✅ 5 commands |
| Context tracking | ❌ Basic | ✅ Advanced |
| Temperature control | ❌ Fixed | ✅ Adjustable |
| Conversation memory | ❌ None | ✅ Full history |
| Interactive | ❌ No | ✅ Yes |

## Known Issues

1. **QEMU Keyboard**: Some QEMU configurations may have keyboard input issues
   - **Workaround**: Use `-serial mon:stdio` and test on real hardware
   
2. **Context Reset**: No automatic context sliding window
   - **Workaround**: Manual `/reset` when context fills up

3. **No File I/O**: Generated text not saved to disk
   - **Future**: Add `/save` command

## Future Enhancements

- [ ] Context sliding window (automatic old message removal)
- [ ] Multi-line input support
- [ ] Save conversation to file (`/save`)
- [ ] Load conversation from file (`/load`)
- [ ] System prompts (`/system "You are a helpful assistant"`)
- [ ] Token streaming with progress bar
- [ ] Arrow key navigation for editing

## Development

### Building

```bash
# Build with chat REPL
make clean && make

# Create test disk image
make disk

# Test in QEMU
qemu-system-x86_64 -bios OVMF.fd -drive format=raw,file=qemu-test.img \
    -cpu Haswell -m 1024 -nographic
```

### Code Structure

**Main REPL function**: `chat_repl()` in `llama2_efi.c` (lines 2549-2785)

**Key components:**
- `read_user_input()` - Keyboard input handler
- `encode_prompt()` - BPE tokenization
- `forward()` - Transformer inference
- `decode_token()` - Token to text conversion

## Testing

### Unit Tests

Test the REPL with these scenarios:

1. **Basic conversation**
   ```
   You: Hello
   Assistant: [response]
   You: Tell me a joke
   Assistant: [joke]
   ```

2. **Temperature adjustment**
   ```
   You: /temp 0.0
   You: What is 2+2?
   [Should give same answer every time]
   ```

3. **Context management**
   ```
   You: /tokens
   [Keep chatting until 80% full]
   You: /reset
   You: /tokens
   [Should show 0 tokens used]
   ```

4. **Error handling**
   ```
   [Wait until 90% context full]
   You: More text
   [Should reject with warning]
   ```

### Hardware Test

**Minimum requirements:**
- x86-64 CPU with UEFI firmware
- 512 MB RAM (for `stories42M.bin`)
- USB drive (for booting)
- Keyboard (PS/2 or USB)

**Test procedure:**
1. Create bootable USB with `dd` or Rufus
2. Boot from USB in UEFI mode
3. Wait for model loading (~60 seconds)
4. Type test messages
5. Verify responses are coherent
6. Test all `/` commands

## License

MIT License - Same as llama2.c by Andrej Karpathy

## Credits

- **Base inference**: Andrej Karpathy ([llama2.c](https://github.com/karpathy/llama2.c))
- **UEFI framework**: GNU-EFI
- **Chat REPL**: Custom implementation for bare-metal UEFI
