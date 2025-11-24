# 💾 Guide de Boot USB - LLM Bare-Metal

**Test sur matériel réel avec Rufus**

---

## ⚠️ IMPORTANT - Avant de commencer

### Prérequis :
- ✅ **Clé USB** : Minimum 1GB (512MB suffit, mais 1GB+ recommandé)
- ✅ **Rufus** : Déjà installé
- ✅ **Image disque** : `qemu-test.img` (512MB)
- ✅ **Ordinateur UEFI** : PC moderne (2012+)

### ⚠️ ATTENTION :
- **Toutes les données sur la clé USB seront EFFACÉES**
- Sauvegarde tes fichiers importants avant !
- Ne débranche pas la clé pendant l'écriture

---

## 📋 Étape 1 : Préparation

### 1.1 Insérer la clé USB
- Branche ta clé USB
- Note la lettre du lecteur (ex: `E:`, `F:`, `G:`)
- **Windows affichera un message "Formater le disque" - IGNORE-LE pour l'instant**

### 1.2 Vérifier l'image
Ouvre PowerShell et vérifie que l'image existe :

```powershell
cd C:\Users\djibi\Desktop\yama_oo\yama_oo\llm-baremetal
ls -lh qemu-test.img
```

Tu devrais voir : `512M qemu-test.img`

---

## 🚀 Étape 2 : Déploiement avec Rufus

### 2.1 Lancer Rufus
1. Ouvre **Rufus** (avec droits admin si demandé)
2. Accepte les autorisations UAC

### 2.2 Configuration Rufus

**Configuration recommandée** :

| Paramètre | Valeur |
|-----------|--------|
| **Périphérique** | Ta clé USB (ex: `E:\`) |
| **Type de démarrage** | "Image disque ou ISO" |
| **Sélection** | Clique sur `SÉLECTIONNER` → Choisis `qemu-test.img` |
| **Schéma de partition** | GPT |
| **Système de destination** | UEFI (non CSM) |
| **Système de fichiers** | FAT32 |
| **Taille d'unité d'allocation** | Par défaut (4096) |
| **Nom de volume** | `LLM_BOOT` |

### 2.3 Options avancées (bouton en bas)
- ☑️ Vérifier le périphérique pour les secteurs défectueux : **NON** (optionnel)
- ☑️ Créer un fichier autorun.inf : **NON**
- ☑️ Créer un fichier .bak étendu : **NON**

### 2.4 Lancement de l'écriture
1. Clique sur **DÉMARRER**
2. Si Rufus demande "Écrire en mode Image DD" :
   - ✅ **Sélectionne "Écrire en mode Image DD"** ← IMPORTANT !
   - (Pas le mode ISO, on a une image RAW)
3. Confirme que toutes les données seront effacées
4. **Attends la fin** (1-3 minutes selon la vitesse USB)

### 2.5 Vérification
Rufus affichera :
- ✅ `Prêt` ou `COMPLETED`
- La barre de progression à 100%

**NE PAS éjecter tout de suite !**

---

## ✅ Étape 3 : Vérification du contenu

### 3.1 Vérifier les fichiers
Ouvre l'Explorateur Windows et va sur ta clé USB. Tu devrais voir :

```
E:\ (ou F:\ selon ta clé)
├── EFI/
│   └── BOOT/
│       └── BOOTX64.EFI    (90KB - Le bootloader)
├── stories110M.bin        (420MB - Le modèle)
├── tokenizer.bin          (434KB - Le tokenizer)
```

### 3.2 Si les fichiers ne sont pas visibles
- La clé peut être formatée en mode "Image DD"
- C'est normal ! Les fichiers sont là mais dans une partition spéciale
- Tu peux quand même booter

---

## 🖥️ Étape 4 : Configuration BIOS/UEFI

### 4.1 Redémarrer en mode BIOS
1. **Ferme tous les programmes**
2. **Redémarre** ton PC
3. Pendant le démarrage, appuie sur :
   - **F2** (la plupart des PC)
   - **DEL** / **Suppr** (certains PC)
   - **F10** (HP)
   - **F12** (Dell)
   - **ESC** (certains Lenovo)

### 4.2 Réglages BIOS nécessaires

#### Option 1 : Boot Menu (Recommandé)
Appuie sur **F12** ou **F11** pendant le boot pour ouvrir le menu de démarrage :
- Cherche "USB" ou "EFI USB Device"
- Sélectionne ta clé USB
- Appuie sur **Entrée**

#### Option 2 : Configuration BIOS complète

**A. Security Settings** :
- **Secure Boot** : `Disabled` ← IMPORTANT !
- **Fast Boot** : `Disabled` (optionnel)

**B. Boot Settings** :
- **Boot Mode** : `UEFI` (PAS Legacy/CSM)
- **Boot Priority** : Place ta clé USB en premier

**C. Advanced** (si disponible) :
- **CPU Features** : AVX2 activé (normalement par défaut)
- **Virtualization** : N'a pas d'importance ici

### 4.3 Sauvegarder et redémarrer
- Appuie sur **F10** pour sauvegarder
- Confirme "Save and Exit"
- Le PC redémarre et boot sur la clé USB

---

## 🎬 Étape 5 : Premier Boot !

### 5.1 Séquence de démarrage attendue
Tu devrais voir :

```
=== MODEL DETECTION ===
Scanning boot disk...

  [1] Stories 110M (Small - 420MB) (stories110M.bin)

Auto-selecting first available model...

Initializing Transformer (110M parameters)...
Loading model: stories110M.bin
[SUCCESS] Model loaded successfully! (427 MB)

Loading BPE tokenizer...
[SUCCESS] Tokenizer loaded! (32000 tokens)

[Mode Selection - INTERACTIVE REPL Forced]
Note: Keyboard input in QEMU not supported.
      Hardware keyboard should work!

========================================
SELECT PROMPT CATEGORY
========================================
1. Stories (7 prompts)
2. Science (7 prompts)
3. Adventure (7 prompts)
4. Philosophy (5 prompts)
5. History (5 prompts)
6. Technology (5 prompts)
7. AUTO-DEMO (cycle all)

Enter choice (1-7):
```

### 5.2 Interaction
- **Sur hardware réel** : Le clavier devrait fonctionner !
- Tape un chiffre `1-7` puis **Entrée**
- Le système génère du texte en temps réel

### 5.3 Performance attendue
- **Chargement du modèle** : 5-10 secondes
- **Génération** : 10-20 tokens/seconde (selon CPU)
- **AVX2** : Accélération visible

---

## 📸 Étape 6 : Capture et Démonstration

### 6.1 Filmer l'écran
- Utilise ton téléphone pour filmer l'écran
- Commence par montrer le boot UEFI
- Filme la génération de texte en temps réel
- Durée : 30-60 secondes suffisent

### 6.2 Ce qu'on veut voir
- ✅ Boot UEFI (logo constructeur)
- ✅ Détection du modèle
- ✅ Menu des catégories
- ✅ Génération de texte (tokens qui apparaissent)
- ✅ Vitesse de génération

---

## 🐛 Dépannage

### Problème 1 : PC ne boot pas sur USB
**Solutions** :
- Vérifie que Secure Boot est **désactivé**
- Change l'ordre de boot dans le BIOS
- Essaie le Boot Menu (F12)
- Vérifie que le mode est bien **UEFI** (pas Legacy)

### Problème 2 : Écran noir après boot
**Solutions** :
- Attends 10-15 secondes (chargement)
- Vérifie que l'image est bien écrite (refais avec Rufus)
- Teste sur un autre PC si possible

### Problème 3 : "No bootable device"
**Solutions** :
- Recréer l'image avec Rufus en mode "Image DD"
- Vérifier que la clé n'est pas défectueuse
- Essayer un autre port USB (USB 2.0 de préférence)

### Problème 4 : Erreur de chargement du modèle
**Causes possibles** :
- Pas assez de RAM (minimum 4GB requis)
- Fichier `stories110M.bin` corrompu
- Clé USB trop lente (essaie USB 3.0)

### Problème 5 : Clavier ne répond pas
**Sur hardware réel** :
- Essaie un clavier USB filaire (pas Bluetooth)
- Branche sur un port USB 2.0
- Vérifie dans le BIOS que USB est activé

**Sur QEMU** :
- C'est normal ! Le mode interactif ne marche pas en QEMU
- Utilise le mode AUTO-DEMO (option 7)

### Problème 6 : Génération très lente
**Solutions** :
- Vérifie que le CPU supporte AVX2 :
  - Intel : Haswell ou plus récent (2013+)
  - AMD : Excavator ou plus récent (2015+)
- Vérifie dans le BIOS que les optimisations CPU sont activées
- Normal sur vieux PC : attends un peu plus

---

## 📊 Comparaison QEMU vs Hardware

| Aspect | QEMU (Émulation) | Hardware Réel |
|--------|------------------|---------------|
| **Vitesse** | 4-7 tok/s | 10-20 tok/s |
| **Boot** | 15-20s | 5-10s |
| **Clavier** | ❌ Ne marche pas | ✅ Fonctionne |
| **Stabilité** | ⚠️ Peut planter | ✅ Stable |
| **AVX2** | ⚠️ Émulé (lent) | ✅ Natif (rapide) |

---

## 🎯 Checklist Complète

### Avant le boot :
- [ ] Clé USB branchée (1GB+)
- [ ] Image `qemu-test.img` écrite avec Rufus
- [ ] Mode "Image DD" utilisé
- [ ] Sauvegarde faite (données USB effacées)

### Configuration BIOS :
- [ ] Secure Boot désactivé
- [ ] Boot Mode = UEFI
- [ ] Clé USB en priorité de boot

### Premier test :
- [ ] PC boot sur la clé
- [ ] Modèle se charge (5-10s)
- [ ] Menu s'affiche
- [ ] Clavier répond (tester chiffre 1-7)

### Génération :
- [ ] Texte s'affiche progressivement
- [ ] Vitesse acceptable (>5 tok/s)
- [ ] Pas d'erreur de mémoire
- [ ] Peut générer plusieurs prompts

### Capture :
- [ ] Vidéo du boot filmée
- [ ] Génération en temps réel capturée
- [ ] Durée 30-60 secondes minimum

---

## 💡 Astuces

### Pour des performances optimales :
1. **Utilise une clé USB 3.0** (lecture plus rapide du modèle)
2. **PC récent** (2015+) avec AVX2
3. **4GB+ de RAM** recommandés
4. **Ferme autres périphériques USB** (souris Bluetooth, etc.)

### Pour filmer proprement :
1. **Nettoie l'écran** avant de filmer
2. **Éclairage correct** (pas de reflets)
3. **Stable** : pose le téléphone sur un support
4. **Horizontal** : filme en paysage (pas portrait)
5. **Audio** : explique ce qui se passe

### Pour partager :
- Upload sur YouTube (Unlisted si tu veux)
- Partage sur GitHub en issue/discussion
- Twitter/X avec hashtag #LLMBareMetal
- Reddit r/osdev ou r/LocalLLaMA

---

## 📹 Exemple de Narration

Voici ce que tu peux dire pendant la vidéo :

```
"Bonjour, aujourd'hui je teste un LLM qui tourne directement 
sur le firmware UEFI sans système d'exploitation.

[Montre la clé USB]
J'ai flashé cette clé USB avec l'image de 512MB contenant 
le modèle stories110M de 420MB.

[Boot PC]
Je redémarre mon PC et je boot sur la clé USB...

[Écran de boot]
Voilà, on voit le firmware UEFI qui charge...

[Menu catégories]
Parfait ! Le modèle est chargé en 8 secondes. 
Je vais choisir la catégorie 'Stories'...

[Génération]
Et là on voit le texte qui se génère token par token,
en temps réel, directement sur le bare-metal.
Pas d'OS, juste UEFI + transformer.

C'est environ 15 tokens par seconde sur mon laptop.
Assez impressionnant pour du bare-metal !

Le code complet est sur GitHub : github.com/djibydiop/llm-baremetal
Merci d'avoir regardé !"
```

---

## 🆘 Besoin d'aide ?

Si tu rencontres un problème :

1. **Vérifie les étapes ci-dessus** (Dépannage)
2. **Teste dans QEMU d'abord** pour éliminer les bugs logiciels
3. **Ouvre une issue GitHub** : https://github.com/djibydiop/llm-baremetal/issues
4. **Fournis ces infos** :
   - Modèle de ton PC / CPU
   - Message d'erreur exact
   - Capture d'écran si possible
   - Étape où ça bloque

---

## ✅ C'est parti !

Tu es prêt pour le test USB réel ! 

**Résumé en 3 étapes** :
1. **Rufus** : Écris `qemu-test.img` en mode "Image DD" sur la clé
2. **BIOS** : Désactive Secure Boot, active UEFI, boot sur USB
3. **Filme** : Capture le boot et la génération pour la postérité

**Bonne chance ! 🚀**

---

*Guide créé le 24 novembre 2025*  
*Version 1.0 - LLM Bare-Metal Project*
