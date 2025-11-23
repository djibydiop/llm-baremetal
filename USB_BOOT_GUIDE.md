# 🚀 GUIDE DE TEST USB - LLM BARE-METAL

**Date** : 23 novembre 2025  
**Objectif** : Tester le bootloader IA sur un PC/laptop réel avec UEFI

---

## ✅ PRÉREQUIS

### Matériel Nécessaire

- ✅ **USB 3.0+** (8 GB minimum, 16 GB recommandé)
- ✅ **PC avec UEFI** (pas Legacy BIOS)
  - **CPU recommandé** : Intel Haswell (2013+) ou AMD Zen (2018+) pour AVX2
  - **RAM** : 8 GB minimum (16 GB recommandé pour TinyLlama)
  - **Secure Boot** : Doit être désactivé

### Fichiers Requis

```
✅ llama2-disk.img (5,200 MB) - Image disque bootable
✅ stories15M.bin (58 MB) - Modèle 1
✅ nanogpt.bin (471 MB) - Modèle 2  
✅ tinyllama_chat.bin (4,196 MB) - Modèle 3
✅ tokenizer.bin (0.41 MB) - Vocabulaire BPE
```

**Tous les fichiers sont présents et prêts !** ✅

---

## 📝 ÉTAPE 1 : CRÉER LA CLÉ USB BOOTABLE

### Option A : Avec WSL (Recommandé)

```bash
# 1. Insérer la clé USB
# 2. Identifier le périphérique USB dans WSL
wsl lsblk

# Exemple de sortie :
# NAME   MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
# sda      8:0    1  14.9G  0 disk           <- Votre USB
# └─sda1   8:1    1  14.9G  0 part

# 3. Copier l'image sur USB (ATTENTION: remplacer /dev/sdX par votre USB!)
cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal
wsl sudo dd if=llama2-disk.img of=/dev/sdX bs=4M status=progress

# 4. Attendre la fin (prend 5-10 minutes)
wsl sudo sync

# 5. Éjecter proprement
wsl sudo eject /dev/sdX
```

⚠️ **ATTENTION** : `/dev/sdX` détruira TOUTES les données du périphérique ! 
- Vérifier 3 fois que c'est bien votre USB
- **NE PAS** utiliser `/dev/sda` (c'est votre disque principal)
- Exemples corrects : `/dev/sdb`, `/dev/sdc`, `/dev/sdd`

### Option B : Avec Rufus (Windows)

1. **Télécharger Rufus** : https://rufus.ie/
2. **Lancer Rufus en admin**
3. **Configuration** :
   - Périphérique : [Sélectionner votre USB]
   - Type de démarrage : "Image disque ou ISO"
   - Cliquer "SÉLECTION" → Choisir `llama2-disk.img`
   - Schéma de partition : **GPT**
   - Système de destination : **UEFI (non CSM)**
4. **Cliquer "DÉMARRER"**
5. **Attendre la fin** (5-10 minutes)

### Option C : Avec balenaEtcher (Multi-plateforme)

1. **Télécharger Etcher** : https://www.balena.io/etcher/
2. **Lancer Etcher**
3. **Flash from file** → Sélectionner `llama2-disk.img`
4. **Select target** → Choisir votre USB
5. **Flash!**

---

## 🔧 ÉTAPE 2 : CONFIGURER LE PC CIBLE

### Désactiver Secure Boot

1. **Redémarrer le PC**
2. **Entrer dans le BIOS/UEFI** :
   - **Dell/HP/Lenovo** : Appuyer sur `F2` ou `F10` au boot
   - **ASUS** : Appuyer sur `Del` ou `F2`
   - **MSI** : Appuyer sur `Del`
   - **Gigabyte** : Appuyer sur `Del`
3. **Trouver "Secure Boot"** (généralement dans Security ou Boot)
4. **Désactiver Secure Boot** : `Disabled`
5. **Sauvegarder et Quitter** : `F10` puis `Yes`

### Vérifier le Mode UEFI

Dans le BIOS/UEFI :
- **Boot Mode** doit être : `UEFI` (pas `Legacy` ou `CSM`)
- **CSM** (Compatibility Support Module) doit être : `Disabled`

---

## 🚀 ÉTAPE 3 : BOOTER SUR L'USB

### Méthode Rapide (Boot Menu)

1. **Redémarrer le PC** avec l'USB branchée
2. **Appuyer sur la touche Boot Menu** :
   - **Dell** : `F12`
   - **HP** : `F9` ou `Esc` puis `F9`
   - **Lenovo** : `F12` ou `F8`
   - **ASUS** : `F8` ou `Esc`
   - **Acer** : `F12`
   - **MSI/Gigabyte** : `F12`
3. **Sélectionner** : `UEFI: [Nom de votre USB]`
   - Exemple : `UEFI: SanDisk Ultra 16GB`
   - **Ne PAS choisir** la version sans "UEFI" devant

### Méthode Alternative (Ordre de Boot)

Si le Boot Menu ne fonctionne pas :

1. **Entrer dans le BIOS/UEFI** (F2/Del au boot)
2. **Aller dans "Boot Order" ou "Boot Priority"**
3. **Mettre l'USB en premier** : `UEFI: [Votre USB]`
4. **Sauvegarder et Redémarrer** : `F10`

---

## 🎯 ÉTAPE 4 : TESTER LE SYSTÈME

### Séquence de Boot Attendue

1. **BIOS/UEFI** : Logo du fabricant (Dell, HP, etc.)
2. **Chargement UEFI** : Écran noir ou logo UEFI
3. **Notre bootloader** : 🎉

```
╔═══════════════════════════════════════════════╗
║   MULTIMODAL LLM BARE-METAL BOOTLOADER       ║
╚═══════════════════════════════════════════════╝

[INFO] CPU Detection...
[SUCCESS] AVX2 enabled! XCR0 = 0x00000007
       (ou [INFO] SSE enabled si CPU ancien)

Scanning for models...
  ✓ [1] stories15M (60MB) - Story generation
  ✓ [2] NanoGPT-124M (471MB) - GPT-2 architecture
  ✓ [3] TinyLlama-1.1B-Chat (4.2GB) - Conversational

Select model (1-3): _
```

### Tests à Effectuer

#### Test 1 : Stories15M (Rapide)

```
Select model (1-3): 1

[Loading stories15M.bin...]
[Model loaded: 60 MB]

[Turn 1/10] You: Once upon a time
[Generating 128 tokens at temp 0.7...]
Assistant: there was a young girl who loved adventures...

✅ **Vérifier** :
- Temps de génération : ~1-2 secondes (avec AVX2)
- Tokens/sec affiché : ~500-700 tok/s
- Texte cohérent et lisible
```

**Taper `/stats` pour voir les statistiques** :
```
═══════════════════════════════════════
 CONVERSATION STATISTICS
═══════════════════════════════════════
 Turns completed: 1/10
 Temperature: 0.70
 Max response tokens: 128
 Total tokens used: 145
 CPU Features: AVX2 + FMA
 Model: stories15M (60MB)
 Inference speed: 650.3 tok/s
═══════════════════════════════════════
```

#### Test 2 : NanoGPT-124M (Moyen)

```
[Turn 1/10] You: exit
[Exiting...]

(Le système redémarre ou retourne au menu)

Select model (1-3): 2

[Loading nanogpt.bin...]
[Model loaded: 471 MB]

[Turn 1/10] You: def fibonacci(n):
[Generating 256 tokens at temp 0.8...]
Assistant:     if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)
```

✅ **Vérifier** :
- Temps de génération : ~3-5 secondes
- Tokens/sec : ~150-200 tok/s (avec AVX2)
- Génération de code Python correcte

#### Test 3 : TinyLlama-1.1B (Complet)

```
Select model (1-3): 3

[Loading tinyllama_chat.bin...]
[Model loaded: 4,196 MB]
[This may take 30-60 seconds...]

[Turn 1/10] You: Explain how UEFI bootloaders work
[Generating 256 tokens at temp 0.7...]
Assistant: UEFI (Unified Extensible Firmware Interface) bootloaders 
work by loading executable files in PE32+ format from a FAT32 
filesystem. The firmware provides boot services and runtime 
services...
```

✅ **Vérifier** :
- Temps de chargement : 30-60 secondes
- Tokens/sec : ~20-30 tok/s (avec AVX2)
- Réponses longues et cohérentes

### Commandes à Tester

| Commande | Test | Résultat Attendu |
|----------|------|------------------|
| `/help` | Affiche l'aide | Liste des 7 commandes |
| `/stats` | Statistiques | Turns, temp, tokens, CPU |
| `/temp 1.0` | Change température | "Temperature set to 1.0" |
| `/tokens 512` | Change longueur | "Max tokens set to 512" |
| `/history` | Historique | Affiche les tours précédents |
| `/clear` | Efface historique | "Conversation cleared" |
| `/exit` | Quitte | Retour menu ou reboot |

---

## 📊 BENCHMARKS À MESURER

### Performance CPU

Relever les tokens/sec pour chaque modèle :

```
stories15M      : _____ tok/s (attendu: 500-700 avec AVX2)
NanoGPT-124M    : _____ tok/s (attendu: 150-200 avec AVX2)
TinyLlama-1.1B  : _____ tok/s (attendu: 20-30 avec AVX2)
```

### Détection CPU

Vérifier au boot :
```
[SUCCESS] AVX2 enabled! XCR0 = 0x00000007
          ^^^ Doit afficher AVX2 si CPU supporte
          
ou

[INFO] SSE enabled (no AVX support)
       ^^^ Si CPU ancien (avant 2013)
```

### Stabilité

- ✅ **Session longue** : Tenir 30 minutes sans crash
- ✅ **Mémoire** : Pas d'erreur allocation même après 100+ tours
- ✅ **Clavier** : Responsive, pas de lag

---

## 🐛 DÉPANNAGE

### Problème 1 : "Secure Boot Violation"

**Symptôme** : Message d'erreur au boot, refuse de lancer
**Solution** : Désactiver Secure Boot dans le BIOS (voir Étape 2)

### Problème 2 : USB non détecté

**Symptôme** : USB n'apparaît pas dans Boot Menu
**Solutions** :
1. Vérifier que l'USB est bien en **UEFI mode** (pas Legacy)
2. Essayer un autre port USB (préférer USB 2.0 pour boot)
3. Recréer l'image USB avec Rufus (Option B)

### Problème 3 : Écran noir après boot

**Symptôme** : Écran noir, pas de texte
**Solutions** :
1. Attendre 30 secondes (peut être lent sur vieux PC)
2. Vérifier câble vidéo (HDMI/DisplayPort)
3. Booter en mode verbose : éditer boot entry UEFI

### Problème 4 : "Model not found"

**Symptôme** : Les 3 modèles affichent ✗ (not found)
**Solutions** :
1. Vérifier que `llama2-disk.img` est complet (5,200 MB exact)
2. Reconstruire le disque : `wsl make llama2-disk`
3. Vérifier que les .bin sont dans le repo avant `make llama2-disk`

### Problème 5 : Clavier ne répond pas

**Symptôme** : Impossible de taper
**Solutions** :
1. Essayer un clavier USB filaire (pas Bluetooth)
2. Changer de port USB
3. Attendre 10 secondes après le prompt
4. Redémarrer et réessayer

### Problème 6 : Très lent / freeze

**Symptôme** : Génération prend >1 minute
**Solutions** :
1. Vérifier RAM disponible : TinyLlama nécessite 8 GB
2. Commencer avec stories15M (modèle léger)
3. Vérifier que AVX2 est bien détecté
4. CPU trop ancien : attendu sur Pentium/Celeron

### Problème 7 : Texte corrompu / caractères bizarres

**Symptôme** : Affichage illisible
**Solutions** :
1. Problème d'encodage UTF-16 vs ASCII
2. Vérifier UEFI firmware version (update BIOS)
3. Tester sur un autre PC

---

## 📸 DOCUMENTATION DU TEST

### À Capturer

1. **Photo/Vidéo du boot** :
   - Logo UEFI → Interface multimodale
   - Détection CPU (AVX2)
   - Sélection modèle

2. **Screenshot de conversation** :
   - Au moins 3 tours de questions/réponses
   - Affichage `/stats`
   - Tokens/sec mesurés

3. **Benchmarks** :
   ```
   CPU: [Modèle exact: Intel i7-10750H]
   RAM: [16 GB]
   
   stories15M:     687 tok/s  ✅
   NanoGPT-124M:   178 tok/s  ✅
   TinyLlama-1.1B: 24 tok/s   ✅
   
   AVX2: Detected ✅
   Stabilité: 45min sans crash ✅
   ```

4. **Retour d'expérience** :
   - Facilité d'installation (1-5) : ___
   - Performance perçue (1-5) : ___
   - Stabilité (1-5) : ___
   - Qualité des réponses (1-5) : ___

---

## 🎥 ENREGISTRER UNE DÉMO

### Setup Vidéo

**Matériel** :
- Téléphone en mode vidéo
- Trépied ou support stable
- Bon éclairage

**Cadrage** :
- Filmer l'écran entier
- Inclure le clavier dans le cadre (montrer la frappe)
- 1080p minimum, 4K idéal

**Contenu** :
1. **Intro (30s)** : Montrer l'USB, le PC
2. **Boot (1min)** : Depuis power-on jusqu'au menu modèle
3. **Demo stories15M (2min)** : Question simple + réponse
4. **Demo TinyLlama (3min)** : Conversation complexe + /stats
5. **Conclusion (30s)** : Afficher tokens/sec, CPU info

### Script Vocal

```
"Voici le premier bootloader IA au monde. Je vais démarrer mon PC 
directement sur cette clé USB, sans système d'exploitation.

[Allumer PC]

Le firmware UEFI charge notre application EFI personnalisée qui 
contient 3 modèles de langage : 60 MB, 471 MB, et 1.1 GB.

[Sélectionner modèle]

Mon CPU détecte AVX2, donc l'inference sera accélérée 3x. Je vais 
maintenant poser une question...

[Taper question]

Et voilà ! La réponse est générée en temps réel, directement sur 
le firmware. Pas de Linux, pas de Windows, juste l'IA sur le 
hardware nu.

[Montrer /stats]

On obtient X tokens par seconde, ce qui est remarquable pour un 
environnement aussi contraint."
```

---

## ✅ CHECKLIST FINALE

Avant le test, vérifier :

- [ ] USB formatée et image copiée (5.2 GB)
- [ ] PC cible avec UEFI (pas Legacy BIOS)
- [ ] Secure Boot désactivé
- [ ] CPU Intel Haswell+ ou AMD Zen+ (pour AVX2)
- [ ] 8 GB RAM minimum
- [ ] Clavier USB filaire disponible
- [ ] Caméra/téléphone pour filmer (optionnel)

Pendant le test :

- [ ] Noter modèle CPU exact
- [ ] Mesurer tokens/sec pour chaque modèle
- [ ] Tester toutes les commandes (/help, /stats, etc.)
- [ ] Session de 30+ minutes pour stabilité
- [ ] Prendre screenshots/vidéo

Après le test :

- [ ] Documenter benchmarks dans un fichier
- [ ] Créer Issue GitHub avec résultats
- [ ] Partager photos/vidéos
- [ ] Décider des prochaines améliorations

---

## 🚀 PROCHAINES ÉTAPES

Après le test réussi :

1. **Publier résultats** sur GitHub (Issue ou Discussion)
2. **Vidéo YouTube** de la démo complète
3. **Article technique** sur Medium/Dev.to
4. **Améliorations** basées sur les retours :
   - INT8 quantization si mémoire limitée
   - UI améliorée si interface confuse
   - Modèles additionnels si demande

---

## 📞 SUPPORT

**Questions/Problèmes** :
- GitHub Issues : https://github.com/djibydiop/llm-baremetal/issues
- Email : [À ajouter si souhaité]

**Contributions** :
- Forkez le repo
- Créez une branche : `git checkout -b feature/mon-test`
- Commitez : `git commit -m "Test sur Lenovo ThinkPad X1"`
- Push : `git push origin feature/mon-test`
- Ouvrez une Pull Request

---

**Bonne chance pour le test ! 🎉**

N'oublie pas de documenter tes résultats et de partager des photos/vidéos.
C'est un projet UNIQUE au monde - montre-le ! 🚀