# Guide QEMU - LLM Bare-Metal REPL

## Vue d'ensemble
Après les améliorations récentes, le projet peut maintenant :
- ✅ **Activer AVX2/FMA en UEFI** (OSXSAVE + XCR0) automatiquement au démarrage
- ✅ **Mesurer temps/tok_s** via UEFI GetTime (fiable sous QEMU)
- ✅ **Fallback robuste** WHPX→TCG si l'accélération échoue
- ✅ **Chipset flexible** (pc/q35) pour contourner des bugs MSI/MMIO

---

## Commandes QEMU disponibles

### 1. Mode rapide (auto-détection)
```powershell
.\run-qemu.ps1
```
- Tente WHPX → retombe en TCG si échec
- CPU = `host` si WHPX, sinon `qemu64`
- Chipset = `pc` (défaut)
- RAM = 4096 MB

### 2. TCG avec AVX2/FMA (fonctionnel sur ta machine)
```powershell
.\run-qemu.ps1 -Accel tcg -Cpu max -ForceAvx2
```
- ✅ **Activé :** DjibLAS SGEMM AVX2+FMA, attention SIMD AVX2
- ⚠️ **Lent :** ~0.5 tok/s (émulation complète CPU)
- 👍 **Fiable :** ne crash jamais

### 3. WHPX avec CPU host (si Hyper-V fonctionne)
```powershell
.\run-qemu.ps1 -Accel whpx -Cpu host
```
- 🚀 **Rapide :** proche des perfs native si WHPX marche
- ⚠️ **Chez toi :** plante avec "Failed to emulate MMIO access"
- 🔄 **Fallback auto :** retente en TCG si échec

### 4. WHPX + chipset q35 (contournement bugs MSI)
```powershell
.\run-qemu.ps1 -Accel whpx -Cpu host -Machine q35
```
- Chipset Q35 = plus moderne, meilleure gestion interruptions
- Peut résoudre certains crashes WHPX liés aux MSI

### 5. Mode GUI (fenêtre QEMU séparée)
```powershell
.\run-qemu.ps1 -Gui
```
- Ouvre une fenêtre SDL QEMU
- Clavier fonctionne directement dans la fenêtre

### 6. Nouvelle fenêtre PowerShell
```powershell
.\run-qemu.ps1 -NewWindow
```
- Lance QEMU dans une nouvelle fenêtre PowerShell
- Utile pour garder ton terminal principal libre

---

## Paramètres disponibles

| Paramètre | Valeurs | Défaut | Description |
|-----------|---------|--------|-------------|
| `-Accel` | `auto`, `whpx`, `tcg`, `none` | `auto` | Accélération matérielle |
| `-Cpu` | `auto`, `host`, `max`, `qemu64` | `auto` | Modèle CPU virtuel |
| `-Machine` | `pc`, `q35` | `pc` | Chipset (q35 = plus moderne) |
| `-ForceAvx2` | switch | off | Force AVX2/FMA dans le CPU (TCG) |
| `-MemMB` | 512-8192 | 4096 | RAM invité en MB |
| `-Gui` | switch | off | Fenêtre graphique SDL |
| `-NewWindow` | switch | off | Nouvelle fenêtre PowerShell |
| `-QemuPath` | string | auto | Chemin vers qemu-system-x86_64.exe |
| `-OvmfPath` | string | auto | Chemin vers firmware UEFI OVMF |
| `-ImagePath` | string | auto | Chemin vers .img |

---

## Diagnostics WHPX (nécessite admin)

### Vérifier si WHPX peut fonctionner
```powershell
# Ouvrir PowerShell en admin, puis :
Get-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform
Get-WindowsOptionalFeature -Online -FeatureName VirtualMachinePlatform
```

**Attendu :** `State : Enabled` pour les deux

### Activer WHPX si désactivé
```powershell
# En admin :
dism /online /enable-feature /featurename:HypervisorPlatform /all /norestart
dism /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart
# Puis redémarrer
```

### Vérifier config boot Hyper-V
```powershell
# En admin :
bcdedit /enum | findstr /i hypervisorlaunchtype
```

**Attendu :** `hypervisorlaunchtype    Auto`

---

## Problème actuel sur ta machine

### Symptôme
```
WHPX: Failed to emulate MMIO access with EmulatorReturnStatus: 2
WHPX: Failed to exec a virtual processor
```

### Causes possibles
1. **BIOS :** Intel VT-x désactivé (VirtualizationFirmwareEnabled = False)
2. **Windows :** Features WHPX non activées ou mal configurées
3. **QEMU/OVMF :** Incompatibilité chipset pc + interruptions MSI

### Solutions à essayer (ordre de priorité)
1. **BIOS :** Activer Intel VT-x / VT-d dans les paramètres CPU
2. **Windows (admin) :** Activer HypervisorPlatform + VirtualMachinePlatform
3. **QEMU :** Tester avec `-Machine q35` (meilleure gestion interruptions)
4. **Fallback :** Utiliser TCG avec AVX2 (fonctionnel, mais lent)

---

## Résultats actuels

### ✅ Qui fonctionne
- **TCG + AVX2** : DjibLAS et attention AVX2 activés, mesures temps fiables
- **AVX state enablement** : CR4.OSXSAVE + XCR0 configurés au boot UEFI
- **Fallback auto** : Si WHPX échoue, retente en TCG sans intervention

### ⚠️ À corriger
- **WHPX** : Nécessite activation VT-x dans BIOS + features Windows
- **Performance TCG** : ~0.5 tok/s (normal pour émulation complète)

---

## Pour USB / Hardware réel

Le fichier `llm-baremetal-boot.img` (499MB) est prêt pour :
- **USB** : Flash avec Rufus (GPT + UEFI non-CSM)
- **Hardware** : Boot direct, AVX2 activé si CPU le supporte

**Commande build :**
```powershell
.\build.ps1 -Target repl -ModelBin stories110M.bin
```

---

## Notes importantes

1. **QEMU version :** Tu utilises QEMU 10.2.0-rc3 (très récent, WHPX peut être instable)
2. **CPU réel :** Intel i5-6200U (Skylake) → supporte SSE4.2, AVX2, FMA
3. **Windows :** 10 Pro 22H2 (Build 19045), Hyper-V installé mais VT-x désactivé
4. **Stats timing :** Utilise maintenant UEFI GetTime (wall-clock) au lieu de TSC

---

## Contact & Contribution

**Made in Senegal 🇸🇳 by Djiby Diop**
- Date : Janvier 2026
- Projet : LLM bare-metal UEFI REPL avec kernel primitives

---

*Mis à jour le 5 janvier 2026*
