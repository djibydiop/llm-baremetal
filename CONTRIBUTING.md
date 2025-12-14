# Contributing to LLM BareMetal

Thank you for your interest! 🚀

## Ways to Contribute

### 🐛 Report Bugs
- Use GitHub Issues
- Include hardware specs
- Provide error messages/screenshots

### ✨ Suggest Features
- Check existing Issues first
- Explain the use case
- Be specific about requirements

### 🧪 Test on Hardware
- Try on your PC/laptop
- Report results in Issue #4
- Share boot times and token/sec speeds

### 💻 Submit Code
1. Fork the repo
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Test thoroughly in QEMU
5. Submit Pull Request

### 📝 Improve Documentation
- Fix typos
- Add examples
- Improve clarity

## Development Setup

```bash
# Clone
git clone https://github.com/djibydiop/llm-baremetal.git
cd llm-baremetal

# Install dependencies (Ubuntu/Debian)
sudo apt install build-essential gnu-efi qemu-system-x86

# Download model
./download_model.sh

# Build
make

# Test in QEMU
make disk
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=qemu-test.img,format=raw -m 512
```

## Code Style

- Follow existing code conventions
- Comment complex logic
- Use descriptive variable names
- Keep functions focused and small

## Pull Request Guidelines

- One feature per PR
- Include description of changes
- Reference related Issues (#1, #2, etc.)
- Test before submitting
- Be responsive to feedback

## Community

- Be respectful and constructive
- Help others in Issues/Discussions
- Share your use cases
- Celebrate successes together!

## Questions?

Open an Issue or Discussion. We're here to help! 🙏

---

**Made with ❤️ in Senegal 🇸🇳**
