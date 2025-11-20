# 🔍 Guide Rapide - Interprétation QEMU

## Ce que vous devriez voir dans la fenêtre QEMU

### ✅ **Cas de succès**
```
========================================
  LLaMA2 Bare-Metal EFI (stories15M)
  95% code from Andrej Karpathy
  Architecture by Meta Platforms
========================================

[DEBUG] Initializing transformer...
[DEBUG] Loading model from stories15M.bin...
[DEBUG] Model loaded! Config validated.
[DEBUG] Running forward pass (token=1, pos=0)...
[DEBUG] Forward pass complete!
Top token: 123 (logit=4.567)

[DEBUG] Generating 20 tokens:
[0] 123 [1] 456 [2] 789 [3] 234 [4] 567
[5] 890 [6] 345 [7] 678 [8] 901 [9] 234

[SUCCESS] Generation complete!

Press any key to exit.
```

### ⚠️ **Problèmes possibles**

#### Problème 1: Fichier non trouvé
```
[DEBUG] Loading model from stories15M.bin...
[ERROR] Failed to load model: Not Found
```
→ Le fichier stories15M.bin n'est pas accessible

#### Problème 2: Erreur mémoire
```
[DEBUG] Forward pass complete!
[puis plantage ou redémarrage]
```
→ Dépassement de pile ou buffer overflow

#### Problème 3: Math NaN/Inf
```
[DEBUG] Forward pass complete!
Top token: -1 (logit=nan)
```
→ Erreur dans les fonctions mathématiques

#### Problème 4: Écran noir ou pas de sortie
→ L'EFI boot n'a pas démarré du tout

## 🧪 Test Minimal (test-minimal.img)

Devrait afficher:
```
========================================
  MINIMAL EFI TEST - WORKING!
========================================

✅ EFI boot successful
✅ Print() function working
✅ UEFI environment initialized

Press any key to test file system...

[puis après appui sur touche]

[TEST] Opening file system...
✅ Loaded image protocol: OK
✅ File system protocol: OK
✅ Volume opened: OK
✅ stories15M.bin opened: OK
✅ File size: 60816028 bytes (58.00 MB)

========================================
  TEST COMPLETE!
========================================
```

## 📝 Ce qui se passe

1. **OVMF boot** (~2-3 secondes)
   - Écran TianoCore
   - Initialisation UEFI
   
2. **Chargement EFI** (~1 seconde)
   - Lecture BOOTX64.EFI depuis le disque
   
3. **Exécution du programme**
   - Print des messages [DEBUG]
   - Chargement modèle (~2-3 secondes)
   - Forward pass (~5-10 secondes pour 15M params)
   - Génération tokens (~1-2 secondes par token)

## 🎯 Prochaines étapes selon résultat

### Si ✅ "Generation complete!" apparaît:
→ **VICTOIRE!** Le modèle fonctionne!
→ Prochaine étape: implémenter le tokenizer BPE complet
→ Puis: générer du texte cohérent au lieu de juste des IDs

### Si ❌ erreur de fichier:
→ Vérifier le contenu du disque avec `mdir`
→ Essayer un chemin absolu dans le code

### Si ❌ crash après forward pass:
→ Réduire MAX_SEQ_LEN à 64
→ Vérifier les limites de tableau dans matmul/attention

### Si ⬛ écran noir:
→ Vérifier OVMF.fd path
→ Essayer avec hello.efi pour confirmer boot UEFI
