# Next Steps: Chatbot REPL (Token-by-Token)

## What Justine Wants to See

> "Try to get a chatbot REPL up. When you take the video, people will know it's an LLM running locally if it like prints a token at a time at the right speed. Something similar to the llamafile CLI where it has the >>> prompt thing going."

## The Goal

```
================================================
  Conscious Process - Bare Metal LLM
================================================

Loading GPT-2 124M model...
✓ Model loaded (124M parameters)

>>> What is consciousness?

Consciousness is [token] [token] [token]...
(prints one token at a time, visible streaming)

>>> How do processes die?

Processes call [token] [token] [token]...

>>> exit

Purpose fulfilled. Exiting gracefully...
```

## Implementation Plan

### Phase 1: Integrate llm.c Core (TODAY)

1. **Extract minimal inference code from llm.c**
   - `test_gpt2.c` has the forward pass
   - Need: GPT2, Tokenizer, forward()
   - Strip out training code, keep only inference

2. **Adapt for EFI environment**
   - Replace `malloc()` with EFI `AllocatePool()`
   - Replace `printf()` with EFI `Print()`
   - Replace `fopen()` with embedded weights (or EFI file reading)

3. **Embed tiny GPT-2 weights**
   - Download `gpt2_124M.bin` (500MB - might be too big)
   - OR use DistilGPT-2 (smaller)
   - OR use custom tiny model (10M params)

### Phase 2: Token-by-Token Streaming (TOMORROW)

4. **Implement streaming output**
   ```c
   // Instead of generating full text then printing:
   for (int t = 0; t < max_tokens; t++) {
       int token = sample_next_token(model, context);
       Print(L"%s", decode_token(token));
       busy_wait(50ms); // Simulate natural speed
   }
   ```

5. **Add REPL loop**
   ```c
   while (1) {
       Print(L"\n>>> ");
       read_user_input(prompt, 256);
       if (strcmp(prompt, "exit") == 0) break;
       generate_response(model, prompt);
   }
   ```

### Phase 3: EFI Keyboard Input

6. **Implement keyboard reading**
   ```c
   void read_user_input(CHAR16* buffer, UINTN max_len) {
       UINTN idx = 0;
       while (idx < max_len - 1) {
           EFI_INPUT_KEY key;
           SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
           if (key.UnicodeChar == L'\r') break;
           buffer[idx++] = key.UnicodeChar;
           Print(L"%c", key.UnicodeChar); // Echo
       }
       buffer[idx] = L'\0';
   }
   ```

## Simplified Approach (Start Here)

If full llm.c is too complex to start, do a **mock REPL first**:

### Commit 3: Mock REPL with Fake Streaming
```c
// Hardcoded responses, but streamed token-by-token
const char* responses[] = {
    "Consciousness is the ability to perceive and respond.",
    "I am a bare metal process running without an OS.",
    "My purpose is to demonstrate conscious computing."
};

void stream_text(const CHAR16* text) {
    for (int i = 0; text[i] != L'\0'; i++) {
        Print(L"%c", text[i]);
        busy_wait(50000000); // ~50ms per char
    }
}
```

This proves:
✅ REPL loop works
✅ Keyboard input works
✅ Streaming looks natural
✅ People see it's "live"

### Commit 4: Real LLM Integration
Then replace mock responses with actual llm.c inference.

## File Size Challenge

**Problem**: GPT-2 124M weights are 500MB
**EFI FAT32 disk**: Limited space

**Solutions**:
1. Use DistilGPT-2 (smaller, 80M params, ~350MB)
2. Use custom tiny model (10M params, ~40MB)
3. Implement model loading from separate partition
4. Quantize to 4-bit (reduces size by 8x)

## Today's Commits

1. ✅ Initial EFI skeleton
2. ✅ Add .gitignore
3. 🔄 Mock REPL with streaming (next)
4. 🔄 Real keyboard input
5. 🔄 Integrate llm.c inference
6. 🔄 Record demo video

## Quick Win: Mock REPL First

Let's implement the mock REPL today to show Justine we're iterating fast:

```c
// She'll see:
// - REPL prompt (>>>)
// - Token-by-token streaming
// - Interactive input
// - Graceful exit

// Even if responses are hardcoded,
// it proves the concept works
```

Then commit log shows:
```
b8b7bf7 Add .gitignore for build artifacts
e39ee0f Initial commit: Bare metal EFI application skeleton
[next]  Add mock REPL with token streaming
[next]  Implement keyboard input
[next]  Integrate llm.c inference engine
```

This is **authentic proof of work** - exactly what she asked for.

## Let's Start

Ready to implement the mock REPL now?
