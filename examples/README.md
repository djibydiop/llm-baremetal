# LLM Bare-Metal - Example Use Cases

This directory contains practical examples demonstrating how to integrate LLM Bare-Metal v5.0 into different types of projects.

## 📁 Examples

### 1. `basic_generation.c`
Simple text generation example - minimal integration.

**Use Case:** Add AI text generation to any bare-metal project.

```bash
gcc basic_generation.c -o basic_generation -I. -L. -lllm
./basic_generation
```

---

### 2. `embedded_system.c`
Embedded device integration with memory constraints.

**Use Case:** IoT devices, microcontrollers, resource-limited hardware.

**Features:**
- Custom memory allocator
- Reduced model size
- Hardware I/O integration

---

### 3. `os_kernel_integration.c`
Operating system kernel service example.

**Use Case:** Custom OS kernels (like YamaOS, TractorOS).

**Features:**
- Kernel service registration
- System call interface
- Multi-process support

---

### 4. `neuronet_mesh.c`
NEURO-NET distributed system example.

**Use Case:** Multi-node AI systems, distributed computing.

**Features:**
- All 17 NEURO-NET features
- Node synchronization
- Collective intelligence

---

### 5. `research_platform.c`
Research and experimentation platform.

**Use Case:** Academic research, algorithm development.

**Features:**
- Performance profiling
- Algorithm benchmarking
- Custom transformer modifications

---

### 6. `bootable_demo.c`
Bootable USB demo application.

**Use Case:** Live demos, proof-of-concepts, hackathons.

**Features:**
- UEFI boot integration
- Interactive menu system
- Visual effects

---

### 7. `ffi_rust_integration/`
Rust Foreign Function Interface (FFI) example.

**Use Case:** Integrate with Rust kernels/applications.

**Files:**
- `src/main.rs` - Rust application
- `build.rs` - Build script
- `Cargo.toml` - Dependencies

**Usage:**
```bash
cd ffi_rust_integration
cargo build --release
cargo run
```

---

### 8. `network_api_server.c`
HTTP API server exposing LLM over network.

**Use Case:** Microservices, edge computing, API gateways.

**Features:**
- HTTP request handling
- JSON API endpoints
- Concurrent inference

---

### 9. `voice_assistant.c`
Voice-activated AI assistant.

**Use Case:** Smart devices, voice interfaces.

**Features:**
- Speech recognition integration
- Text-to-speech synthesis
- Wake word detection

---

### 10. `game_npc_ai.c`
AI-powered game NPC dialogue system.

**Use Case:** Game development, interactive fiction.

**Features:**
- Dynamic dialogue generation
- Character personality traits
- Context-aware responses

---

## 🚀 Quick Start

### Compile All Examples

```bash
cd examples
make all
```

### Run Specific Example

```bash
./basic_generation
./neuronet_mesh
./bootable_demo
```

---

## 📚 Integration Templates

Each example includes:
- ✅ Full source code with comments
- ✅ Makefile/build script
- ✅ README with usage instructions
- ✅ Expected output samples

---

## 🛠️ Customization

All examples are designed to be **copy-paste templates**. Modify them for your specific needs:

1. Copy example to your project
2. Adjust paths and configuration
3. Add your custom logic
4. Compile and deploy

---

## 📖 Documentation

For detailed API documentation, see:
- `../INTEGRATION_GUIDE.md` - Complete integration guide
- `../llm_interface.h` - Public API reference
- `../NEURO_NET_v2.0_DOCUMENTATION.md` - NEURO-NET features

---

## 🤝 Contributing

Have a cool use case? Submit your example!

1. Create example file(s)
2. Add README with description
3. Submit pull request
4. Help the community!

---

## 📝 License

All examples are MIT licensed - use them freely in your projects!
