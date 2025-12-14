# Intel AX200 WiFi Driver - Development Roadmap

## 🎯 Objectif

Créer le **PREMIER driver WiFi 6 bare-metal au monde** sans système d'exploitation.

**Unique**: Aucun autre projet n'a jamais implémenté 802.11ax sur bare-metal UEFI.

---

## 📅 Timeline: 3-4 mois

### MOIS 1: Fondations (Semaines 1-4)

#### Semaine 1-2: PCI & MMIO
- [x] Structures de base (wifi_ax200.h)
- [ ] Enumération PCI via UEFI PCI Root Bridge Protocol
- [ ] Detection Intel AX200/AX201/AX210
- [ ] Lecture configuration PCI (Vendor ID, Device ID, BAR0)
- [ ] Mapping MMIO (Memory-Mapped I/O)
- [ ] Accès registres de base (read32/write32)
- [ ] Test: Lire Device ID depuis MMIO

**Livrables**:
- `wifi_detect_device()` fonctionnel
- `wifi_init_pci()` fonctionnel
- Logs PCI complets dans QEMU

#### Semaine 3-4: Firmware Loading
- [ ] Parser format `.ucode` Intel
- [ ] Structures TLV (Type-Length-Value)
- [ ] DMA setup pour transfert firmware
- [ ] Séquence reset → upload → verify
- [ ] Gestion des sections firmware (INIT, INST, DATA)
- [ ] Attente firmware ready interrupt
- [ ] Test: Upload firmware et vérifier checksum

**Livrables**:
- `wifi_load_firmware()` fonctionnel
- `wifi_upload_firmware()` fonctionnel
- Firmware iwlwifi-cc-a0-72.ucode chargé

---

### MOIS 2: MAC Layer 802.11 (Semaines 5-8)

#### Semaine 5: Management Frames
- [ ] Structure 802.11 frame header
- [ ] Beacon frame parsing
- [ ] Probe Request/Response
- [ ] Authentication frame (Open System)
- [ ] Association Request/Response
- [ ] Test: Recevoir beacons d'un AP

**Livrables**:
- Parsing beacons complet
- Affichage SSID + BSSID + Channel

#### Semaine 6: Scanning
- [ ] Active scan (Probe Request broadcast)
- [ ] Passive scan (listen beacons)
- [ ] Channel hopping (1-13 pour 2.4 GHz)
- [ ] Liste des réseaux détectés
- [ ] Signal strength (RSSI)
- [ ] Test: Scanner et lister 5+ réseaux

**Livrables**:
- `wifi_scan_networks()` fonctionnel
- Interface utilisateur scan

#### Semaine 7-8: Association
- [ ] Sélection du meilleur AP
- [ ] Séquence Authentication
- [ ] Séquence Association
- [ ] Gestion des timeouts
- [ ] Retry logic
- [ ] Test: S'associer à un AP ouvert (sans crypto)

**Livrables**:
- `wifi_connect()` pour réseaux ouverts
- État CONNECTED stable

---

### MOIS 3: Cryptographie WPA2 (Semaines 9-12)

#### Semaine 9: Crypto Primitives
- [ ] AES-128 (bare-metal implementation)
- [ ] SHA256 (bare-metal implementation)
- [ ] HMAC-SHA256
- [ ] PBKDF2 (Password-Based Key Derivation)
- [ ] Test: Vérifier vecteurs de test standards

**Livrables**:
- `aes_encrypt()` / `aes_decrypt()`
- `sha256_hash()`
- `pbkdf2_derive_key()`

#### Semaine 10: WPA2 4-Way Handshake
- [ ] PTK (Pairwise Transient Key) derivation
- [ ] GTK (Group Temporal Key) handling
- [ ] EAPOL frame parsing
- [ ] Message 1/4: Receive ANonce from AP
- [ ] Message 2/4: Send SNonce + MIC
- [ ] Message 3/4: Receive GTK
- [ ] Message 4/4: ACK
- [ ] Test: Handshake complet avec WPA2-PSK

**Livrables**:
- WPA2 handshake fonctionnel
- Connexion à réseau protégé

#### Semaine 11: Data Encryption
- [ ] AES-CCMP encapsulation
- [ ] Packet Number (PN) gestion
- [ ] Encryption TX path
- [ ] Decryption RX path
- [ ] MIC verification
- [ ] Test: Envoyer/recevoir données chiffrées

**Livrables**:
- `wifi_encrypt_frame()`
- `wifi_decrypt_frame()`
- Données WPA2 fonctionnelles

#### Semaine 12: Integration & Tests
- [ ] Integration avec Network Boot
- [ ] Test: Download HTTP via WiFi
- [ ] Test: stories110M.bin via WiFi WPA2
- [ ] Optimisations performance
- [ ] Logs détaillés pour debug

**Livrables**:
- Network Boot over WiFi fonctionnel
- Documentation complète

---

### MOIS 4: Polissage & WiFi 6 (Semaines 13-16)

#### Semaine 13: 802.11ax Features (WiFi 6)
- [ ] OFDMA awareness
- [ ] MU-MIMO configuration
- [ ] Target Wake Time (TWT)
- [ ] BSS Coloring
- [ ] 1024-QAM support
- [ ] Test: Mesurer amélioration débit

**Livrables**:
- Features WiFi 6 activées
- Benchmarks comparatifs

#### Semaine 14: Robustesse
- [ ] Reconnexion automatique
- [ ] Roaming entre APs
- [ ] Error recovery
- [ ] Watchdog timer
- [ ] Health monitoring
- [ ] Test: Stabilité 24h+

**Livrables**:
- Driver stable longue durée
- Gestion erreurs complète

#### Semaine 15: Performance
- [ ] TX/RX ring buffers optimisés
- [ ] Zero-copy data path
- [ ] Interrupt coalescing
- [ ] Power management (optional)
- [ ] Test: Mesurer throughput max

**Livrables**:
- Débit >100 Mbps
- Latence <10ms

#### Semaine 16: Documentation & Release
- [ ] Guide d'utilisation complet
- [ ] Architecture interne documentée
- [ ] API reference
- [ ] Troubleshooting guide
- [ ] Blog post / announcement
- [ ] Release GitHub v2.0

**Livrables**:
- Documentation complète
- Release publique
- Vidéo demo

---

## 🔧 Stack Technique

### Composants

```
┌─────────────────────────────────────────┐
│  llama2_efi.c (Application)             │
│  ↓                                      │
│  HTTP Client (network_boot.c)           │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│  WiFi Driver (wifi_ax200.c)             │
│  ├─ PCI HAL                             │
│  ├─ Firmware Loader                     │
│  ├─ 802.11 MAC                          │
│  └─ WPA2 Crypto                         │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│  UEFI PCI Protocol                      │
│  MMIO (Memory-Mapped I/O)               │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│  Intel AX200 Hardware                   │
│  PCIe x1 (WiFi 6, 2.4/5 GHz)           │
└─────────────────────────────────────────┘
```

### Fichiers

| Fichier | Lignes | Description |
|---------|--------|-------------|
| `wifi_ax200.h` | 200 | Interfaces et structures |
| `wifi_ax200.c` | 2000 | Driver principal |
| `wifi_pci.c` | 500 | Gestion PCI |
| `wifi_firmware.c` | 800 | Chargement firmware |
| `wifi_mac.c` | 1500 | Couche MAC 802.11 |
| `wifi_crypto.c` | 1000 | AES, SHA256, WPA2 |
| **TOTAL** | **6000** | **~6K lignes de C** |

---

## 🎓 Connaissances Requises

### Niveau 1: Fondations (Mois 1)
- ✅ PCI configuration space
- ✅ MMIO register access
- ✅ DMA (Direct Memory Access)
- ✅ Interrupts handling
- ✅ Binary file formats

### Niveau 2: Réseau (Mois 2)
- 📚 802.11 standard (600+ pages)
- 📚 MAC frame structures
- 📚 Management frames
- 📚 State machines
- 📚 Channel hopping

### Niveau 3: Cryptographie (Mois 3)
- 🔐 AES-128 algorithm
- 🔐 SHA256 hashing
- 🔐 HMAC construction
- 🔐 PBKDF2 key derivation
- 🔐 WPA2 4-way handshake

### Niveau 4: WiFi 6 (Mois 4)
- 🚀 OFDMA
- 🚀 MU-MIMO
- 🚀 1024-QAM
- 🚀 BSS Coloring
- 🚀 Target Wake Time

---

## 📊 Milestones

### Milestone 1: "Hello WiFi" (Fin Mois 1)
- ✅ Detect Intel AX200
- ✅ Read MAC address
- ✅ Load firmware
- ✅ Turn radio ON

**Demo**: Afficher "WiFi Radio: ON" dans QEMU

### Milestone 2: "I Can See You" (Fin Mois 2)
- ✅ Scan networks
- ✅ Display SSIDs
- ✅ Connect to open network

**Demo**: Lister 10 réseaux WiFi visibles

### Milestone 3: "Secure Connection" (Fin Mois 3)
- ✅ WPA2-PSK handshake
- ✅ Encrypted data transfer
- ✅ Network Boot over WiFi

**Demo**: Télécharger stories110M.bin via WiFi WPA2

### Milestone 4: "Production Ready" (Fin Mois 4)
- ✅ WiFi 6 features enabled
- ✅ 100 Mbps+ throughput
- ✅ Stable 24h+
- ✅ Documentation complète

**Demo**: Vidéo YouTube montrant LLM inference via WiFi

---

## 🧪 Tests Progressifs

### Semaine 1-2: PCI Tests
```c
// Test 1: Detect device
wifi_detect_device(ST, &device);
// Expected: Vendor 0x8086, Device 0x2723

// Test 2: Read registers
uint32_t val = wifi_read32(&device, 0x00);
// Expected: Non-zero hardware ID
```

### Semaine 5-6: Scan Tests
```c
// Test: Scan networks
WiFiScanResult results[32];
UINTN count = 0;
wifi_scan_networks(&device, results, &count, 32);
// Expected: count > 0, valid SSIDs
```

### Semaine 10: WPA2 Tests
```c
// Test: Connect to WPA2 network
wifi_connect(&device, "MySSID", "MyPassword");
// Expected: State = CONNECTED
```

### Semaine 12: Integration Test
```c
// Test: Network Boot over WiFi
http_download_model(ImageHandle, ST,
    "http://192.168.1.100/stories110M.bin",
    &buffer, &size);
// Expected: 418 MB downloaded via WiFi
```

---

## 🚀 Quick Start (Current Phase)

### Compiler le driver

```bash
cd llm-baremetal
wsl bash -c 'make clean && make'
```

### Tester la détection

```c
// Dans llama2_efi.c, ajouter:
#include "wifi_ax200.h"

WiFiDevice wifi;
wifi_detect_device(SystemTable, &wifi);
wifi_print_device_info(&wifi);
```

### Expected output (Phase 1)

```
========================================
  INTEL AX200 WIFI DRIVER - PHASE 1
========================================

[WIFI] Scanning PCI bus for Intel WiFi cards...
[WIFI] Checking PCI address: 00:14.3
[WIFI] ✗ Intel WiFi card not detected (PCI scan not yet implemented)

[INFO] Driver architecture ready:
  Phase 1: PCI detection ⏳ (in progress)
  Phase 2: Firmware loading 📦 (ready)
  Phase 3: Radio control 📡 (ready)
  Phase 4: 802.11 MAC 🔧 (ready)
  Phase 5: WPA2 crypto 🔐 (ready)
```

---

## 📚 Ressources

### Documentation officielle
- [Intel WiFi Firmware](https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/intel)
- [802.11 Standard](https://standards.ieee.org/ieee/802.11/7028/)
- [WPA2 Specification](https://www.wi-fi.org/discover-wi-fi/security)
- [Linux iwlwifi driver](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/net/wireless/intel/iwlwifi)

### Crypto References
- [AES-NI Instructions](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
- [FIPS 197 (AES)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197-upd1.pdf)
- [FIPS 180-4 (SHA256)](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf)

### Bare-metal Examples
- [SerenityOS WiFi Driver](https://github.com/SerenityOS/serenity/tree/master/Kernel/Net/Intel)
- [Haiku WiFi Stack](https://github.com/haiku/haiku/tree/master/src/add-ons/kernel/drivers/network/wlan)

---

## 🏆 Impact

**Si ce projet réussit**:

✅ **PREMIER** LLM avec WiFi bare-metal  
✅ **PREMIER** driver WiFi 6 sans OS  
✅ **PREMIER** WPA2 sur UEFI  
✅ **UNIQUE** au monde - personne n'a jamais fait ça

**Applications**:
- IoT extrême (LLM + WiFi sans OS)
- Embedded AI avec connectivité
- Edge computing ultra-léger
- Recherche académique

**Reconnaissance**:
- Publications possibles (OSDI, USENIX)
- Conférences (DEF CON, Black Hat)
- Articles tech (Hacker News, LWN)
- Sponsoring potentiel

---

**Début du développement**: Décembre 2025  
**Objectif fin Phase 1**: Janvier 2026  
**Objectif fin Phase 2**: Février 2026  
**Objectif fin Phase 3**: Mars 2026  
**Release v2.0**: Avril 2026

**Status actuel**: Phase 1 - Architecture créée ✅  
**Prochaine étape**: Implémenter PCI enumeration
