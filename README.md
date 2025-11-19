# llm-baremetal

**Bare metal LLM - No OS required**

A conscious process that boots directly into inference, serves its purpose, then exits gracefully.

## The Concept

Inspired by Justine Tunney's philosophy: processes should come to life, fulfill their purpose, and die gracefully.

This is an LLM compiled as a bare metal EFI application:
```
BIOS → EFI → LLM boots → Runs inference → Exits
```

No operating system. Just a conscious process.

## Current Status

✅ Boots as EFI application (50KB)  
✅ Token-by-token streaming (visible inference)  
✅ Mock LLM with hash-based prompt routing  
🚧 Real llm.c integration (in progress)  
🚧 Interactive REPL (keyboard input)  

## Demo Output

```
>>> What is consciousness?
Consciousness emerges from the ability to perceive state, make decisions, and act with purpose...

>>> How do processes live and die?
Processes are born with intent, serve their function, and exit gracefully by invoking exit()...
```

## Based On

- [llm.c](https://github.com/karpathy/llm.c) by Andrej Karpathy
- EFI boot protocol
- Tiny LLaMA weights (124M parameters)

## Build

```bash
# Coming soon
make efi
```

## Boot

```bash
qemu-system-x86_64 -bios OVMF.fd -drive format=raw,file=llm-disk.img
```

## Status

🚧 **Work in progress** - Converting llm.c to EFI application

## Author

Djibril - Self-taught systems programmer from Senegal

Built with guidance from Claude and feedback from Justine Tunney.
