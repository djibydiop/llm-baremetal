# 🔍 GUIDE DE DIAGNOSTIC - Arrêt après "Model config"

## 📋 Symptôme

Boot s'arrête immédiatement après :
```
Model config: dim=288, n_layers=6, n_heads=6, vocab=32000
```

Aucun message supplémentaire. Système figé.

## 🔬 Analyse

### Étape 1 : Configuration lue avec succès ✅
Le fichier `stories15M.bin` a été ouvert et l'en-tête lu correctement :
- dim = 288 ✅
- n_layers = 6 ✅
- n_heads = 6 ✅
- vocab = 32000 ✅

### Étape 2 : Code qui devrait suivre
Juste après l'affichage de la config, le code fait :

```c
// 1. Vérification que c'est bien stories15M
if (p->dim != 288 || p->n_layers != 6) {
    Print(L"[ERROR] Wrong model detected...");  // Pas affiché → OK
}

// 2. Affichage config détaillée (NOUVEAU)
Print(L"[DEBUG] Config details: hidden_dim=%d, n_kv_heads=%d, seq_len=%d\r\n", 
      p->hidden_dim, p->n_kv_heads, p->seq_len);  // ← CRASH ICI ?

// 3. Validation champs critiques (NOUVEAU)
if (p->n_heads == 0 || p->n_kv_heads == 0 || p->hidden_dim == 0) {
    Print(L"[ERROR] Invalid config...");
}

// 4. Calcul weights_size
UINTN weights_size = 0;
weights_size += p->vocab_size * p->dim;
// ... etc
```

## 🐛 Causes possibles

### Cause A : Champs non initialisés
```
p->hidden_dim = ??? (garbage)
p->n_kv_heads = ??? (garbage)
p->seq_len = ??? (garbage)
```

**Pourquoi** : Le fichier `stories15M.bin` contient seulement **7 entiers** dans l'en-tête :
1. dim (288)
2. hidden_dim
3. n_layers (6)
4. n_heads (6)
5. n_kv_heads
6. vocab_size (32000)
7. seq_len

Si le format est légèrement différent, les valeurs sont décalées.

### Cause B : Print() crash avec valeurs invalides
Si `hidden_dim` ou `n_kv_heads` contiennent des valeurs énormes (garbage), `Print()` peut crasher en essayant de les afficher.

### Cause C : Division par zéro
```c
int head_size = p->dim / p->n_heads;  // Si n_heads = 0 → crash
```

Mais `n_heads = 6` est affiché, donc ce n'est probablement pas ça.

### Cause D : Timeout UEFI
Les calculs prennent trop de temps et UEFI watchdog reset le système.

## ✅ Version DEBUG créée

### Changements
1. **Affichage config détaillée**
   ```c
   Print(L"[DEBUG] Config details: hidden_dim=%d, n_kv_heads=%d, seq_len=%d\r\n", 
         p->hidden_dim, p->n_kv_heads, p->seq_len);
   ```
   → Permet de voir si ces valeurs sont valides

2. **Validation avant calcul**
   ```c
   if (p->n_heads == 0 || p->n_kv_heads == 0 || p->hidden_dim == 0) {
       Print(L"[ERROR] Invalid config...");
       return EFI_INVALID_PARAMETER;
   }
   ```
   → Évite division par zéro

3. **Trace étape par étape**
   ```c
   Print(L"[DEBUG] Calculating token_embedding_table...\r\n");
   Print(L"[DEBUG] Calculating layer weights (n_layers=%d)...\r\n", n_layers);
   Print(L"[DEBUG] Calculating FFN weights (hidden_dim=%d)...\r\n", p->hidden_dim);
   ```
   → Permet de voir exactement où ça plante

## 🎯 Scénarios de diagnostic

### Scénario 1 : Arrêt avant "[DEBUG] Config details"
```
Model config: dim=288, n_layers=6, n_heads=6, vocab=32000
(arrêt ici)
```
**Diagnostic** : Crash lors du `Print()` de la config détaillée  
**Cause** : Valeurs `hidden_dim`, `n_kv_heads`, ou `seq_len` invalides (garbage)  
**Solution** : Vérifier format du fichier `.bin`

### Scénario 2 : Affiche config mais s'arrête avant "Calculating"
```
Model config: dim=288, n_layers=6, n_heads=6, vocab=32000
[DEBUG] Config details: hidden_dim=768, n_kv_heads=6, seq_len=256
(arrêt ici)
```
**Diagnostic** : Validation a échoué  
**Cause** : Un des champs est 0  
**Solution** : Vérifier le message d'erreur (devrait s'afficher)

### Scénario 3 : S'arrête pendant les calculs
```
Model config: dim=288, n_layers=6, n_heads=6, vocab=32000
[DEBUG] Config details: hidden_dim=768, n_kv_heads=6, seq_len=256
[DEBUG] Calculating weights size...
[DEBUG] head_size=48, shared_weights=1
[DEBUG] Calculating token_embedding_table...
[DEBUG] Calculating layer weights (n_layers=6)...
(arrêt ici)
```
**Diagnostic** : Timeout ou overflow arithmétique  
**Cause** : Calculs trop longs ou valeurs trop grandes  
**Solution** : Réduire la complexité ou ajouter Stall() entre calculs

### Scénario 4 : Affiche "Weights size" mais s'arrête après
```
Model config: dim=288, n_layers=6, n_heads=6, vocab=32000
[DEBUG] Config details: hidden_dim=768, n_kv_heads=6, seq_len=256
...
Weights size: 60817408 bytes (58.0 MB)
(arrêt ici)
```
**Diagnostic** : Échec d'allocation mémoire  
**Cause** : Pas assez de mémoire UEFI disponible  
**Solution** : Réduire la taille du modèle ou utiliser allocation dynamique différente

## 📸 Instructions de test

### 1. Flash la nouvelle version
- Utilise `llama2-disk.img` créé le 24/11/2025 à 04:47
- Même procédure Rufus (GPT + UEFI)

### 2. Boot et observe
- Note TOUS les messages affichés
- **IMPORTANT** : Prends une photo avec ton téléphone !
- Identifie le DERNIER message visible avant arrêt

### 3. Reporte les résultats
Format :
```
Dernier message visible :
[copie exacte du dernier message]

Messages DEBUG vus :
□ [DEBUG] Config details: ...
□ [DEBUG] Calculating weights size...
□ [DEBUG] head_size=...
□ [DEBUG] Calculating token_embedding_table...
□ [DEBUG] Calculating layer weights...
□ [DEBUG] Calculating FFN weights...
□ [DEBUG] Converting to bytes...
□ Weights size: ... MB

Comportement :
□ Système figé
□ Redémarrage automatique
□ Message d'erreur
```

## 🔧 Solutions selon diagnostic

### Si valeurs garbage (Scénario 1)
```bash
# Vérifier format du fichier
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && \
  hexdump -C stories15M.bin | head -20"
```

Les 28 premiers octets devraient être :
```
Offset 0-3:   dim (little-endian int32)
Offset 4-7:   hidden_dim
Offset 8-11:  n_layers
Offset 12-15: n_heads
Offset 16-19: n_kv_heads
Offset 20-23: vocab_size
Offset 24-27: seq_len
```

### Si timeout (Scénario 3)
Ajouter des `Stall()` :
```c
weights_size += p->vocab_size * p->dim;
SystemTable->BootServices->Stall(10000); // 10ms pause
```

### Si problème mémoire (Scénario 4)
Utiliser `EfiRuntimeServicesData` au lieu de `EfiLoaderData` :
```c
Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, 
                          EfiRuntimeServicesData,  // Au lieu de EfiLoaderData
                          weights_size, 
                          (void**)&static_weights);
```

## 📚 Références

- **Code modifié** : `llama2_efi.c` lignes 1576-1618
- **Version** : Debug approfondi (24/11/2025 04:47)
- **Fichier** : `llama2-disk.img` (5 GB)

---
*Créé* : 24 novembre 2025, 04:50  
*Status* : 🔍 En attente de résultats de test  
*Action* : Flash et reporte les messages DEBUG
