# CHANGELOG - LLM Bare-Metal

## 2026-01-05 - Améliorations Performance & QEMU

### ✅ Nouvelles fonctionnalités

#### AVX2/FMA sous UEFI/QEMU
- **AVX state enablement** : Activation automatique de OSXSAVE (CR4.18) + XCR0 au démarrage UEFI
  - Permet aux kernels AVX2 DjibLAS et attention SIMD de s'activer sous QEMU/OVMF
  - Fichier : `llama2_efi_final.c` → `enable_avx_best_effort()`
- **Résultat** : Sous QEMU TCG avec `-Cpu max -ForceAvx2`, on obtient maintenant :
  - `[DJIBLAS] SGEMM kernel: AVX2+FMA`
  - `[ATTN] SIMD path: AVX2`

#### Mesure temps/tok_s fiable sous QEMU
- **UEFI GetTime wall-clock** : Remplacement du timer TSC (peu fiable sous émulation) par `RuntimeServices->GetTime`
  - Fallback TSC conservé pour compatibilité hardware
  - Fichier : `llama2_efi_final.c` → `uefi_wall_us()`
- **Résultat** : `[stats] tokens=160 time_ms=325000 tok_s=0.492` (cohérent, plus de valeurs aberrantes)

#### Script QEMU robuste
- **Fallback automatique WHPX→TCG** : Si WHPX échoue, retente en TCG automatiquement
- **CPU model override** : Paramètres `-Cpu auto|host|max|qemu64`
- **Force AVX2** : Switch `-ForceAvx2` pour ajouter `avx2=on,fma=on` au CPU virtuel
- **Chipset flexible** : Paramètre `-Machine pc|q35` (q35 = meilleure gestion MSI/interruptions)
- **Exit code normalisé** : Codes 0/1/2/130/0xC000013A traités comme succès (user exit)
- **Fichiers** : `run.ps1`, `run-qemu.ps1`

### 🔧 Correctifs

#### Cleanup 260M artifacts
- Suppression des fichiers d'entraînement 260M (~5.5GB libérés)
  - `llama2.c/out_260m_cpu/` (checkpoints)
  - `llm-baremetal/stories260M.bin` (modèle exporté)
  - `llm-baremetal/llm-baremetal-boot.img` (ancienne image 978MB)
- Image rebuild avec `stories110M.bin` stable (499MB)

#### Dataset TinyStories prêt
- Download + pretokenize complétés (50 shards `.bin`)
- Comportement resume : skip shards existants
- `pin_memory` activé uniquement sur CUDA (évite warnings CPU-only)

### 📋 Documentation

#### Guides créés
- **QEMU_GUIDE.md** : Guide complet des options QEMU, diagnostics WHPX, paramètres disponibles
- **CHANGELOG.md** : Ce fichier

### 🐛 Bugs connus

#### WHPX sur cette machine
- **Symptôme** : `WHPX: Failed to emulate MMIO access` → exit code 3
- **Cause probable** : VT-x désactivé dans BIOS (`VirtualizationFirmwareEnabled: False`)
- **Workaround** : Utiliser TCG (`-Accel tcg`) ou activer VT-x + features Windows
- **Statut** : Fallback automatique implémenté

#### Performance TCG
- **~0.5 tok/s** sous QEMU TCG (attendu pour émulation complète CPU)
- **Solution** : Activer WHPX (nécessite VT-x BIOS + features Windows) ou booter sur USB/hardware réel

---

## 2026-01-04 - Intégration LLM-Kernel + Training

### ✅ Fonctionnalités principales

#### Zone-based memory allocator
- 5 arenas : WEIGHTS, KV, SCRATCH, ACTS, ZONEC
- Sentinel avec cycle budgets (prefill/decode)
- Soft overrun handling : auto-raise budget si dépassement < 3×
- Zone C ring log (post-mortem debugging)

#### SIMD optimizations
- **DjibLAS** : SGEMM SSE2 baseline + AVX2/FMA kernel (compilation séparée, dispatch runtime)
- **Attention** : dot+weighted-sum SSE2 baseline + AVX2 optionnel
- **Softmax** : SSE2 max-reduction + normalization (exp scalaire)

#### Sampling improvements
- **Loop escape** : Détection suffix repeat → ban token + resample (1 fois)
- **No-repeat ngram** : Blocage pré-softmax des n-grams répétés
- **Repetition penalty** : Pénalité configurable sur tokens récents

#### TinyStories training pipeline
- Export llama2.c-format `.bin` pour inférence bare-metal
- Tentative 260M : pipeline validé end-to-end mais qualité insuffisante (training CPU-only trop lent)
- Décision : rester sur `stories110M.bin` stable

---

## Architecture actuelle

### Fichiers clés
- **llama2_efi_final.c** : REPL UEFI principal avec kernel integration
- **djiblas.c / djiblas_avx2.c** : Matmul optimisé runtime-dispatch
- **attention_avx2.c** : Attention SIMD (dot + weighted-sum)
- **llmk_zones.c/h** : Zone allocator + arenas
- **llmk_sentinel.c/h** : Cycle budget + fail-safe
- **llmk_log.c/h** : Zone C ring log (post-mortem)
- **run.ps1 / run-qemu.ps1** : Lanceurs QEMU avec options avancées
- **build.ps1** : Build UEFI + image bootable (WSL/mtools)

### Modèle actuel
- **stories110M.bin** (418MB) : TinyStories 110M params, stable
- Tokenizer : `tokenizer.bin` (32000 vocab BPE)
- Boot image : `llm-baremetal-boot.img` (499MB, GPT+FAT32+UEFI)

### Hardware validé
- **CPU** : Intel i5-6200U (Skylake) - SSE4.2, AVX2, FMA
- **RAM** : 8GB (QEMU limité à 4GB)
- **OS** : Windows 10 Pro 22H2 (Build 19045)
- **QEMU** : 10.2.0-rc3

---

## Roadmap

### Court terme
- [ ] Activer VT-x BIOS + features WHPX pour performance QEMU
- [ ] Tester boot USB sur hardware réel (validation AVX2 native)
- [ ] Mesurer tok/s sur hardware (attendu : 5-10 tok/s avec AVX2)

### Moyen terme
- [ ] Entraîner ou récupérer un modèle mieux converge (260M+ sur GPU)
- [ ] Quantization Q8 (déjà présent dans `quantization_q8.h`)
- [ ] Ajouter commande REPL `/model` pour afficher modèle chargé

### Long terme
- [ ] Support multi-modèles (switch runtime sans reboot)
- [ ] Streaming generation (affichage token par token sans buffer)
- [ ] Optimisations supplémentaires (prefill SIMD, softmax AVX2 exp)

---

**Made in Senegal 🇸🇳 by Djiby Diop**

*Dernière mise à jour : 5 janvier 2026*
