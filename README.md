# llm-baremetal

UEFI x86_64 bare-metal LLM chat REPL (GNU-EFI). Boots from USB.

Made in Senegal 🇸🇳 by Djiby Diop

## Build (Windows + WSL)

1) Put `tokenizer.bin` and a model weights file (e.g. `stories110M.bin`) in this folder.
2) Build + create boot image:

```powershell
./build.ps1
```

## Run (QEMU)

```powershell
./run.ps1 -Gui
```

## Notes

- Model weights are intentionally not tracked in git; use GitHub Releases or your own files.
- Optional config: copy `repl.cfg.example` → `repl.cfg` (not committed) and rebuild.

## DjibQuant (optional)

If you want smaller weights files, DjibQuant tooling/docs live in this repo (see DJIBQUANT.md).
