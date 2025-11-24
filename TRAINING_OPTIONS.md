# Options d'Amélioration stories15M

## 📊 Modèle Actuel
- **Fichier**: stories15M.bin (60 MB)
- **Architecture**: dim=288, 6 layers, 6 heads
- **Vocabulaire**: 32000 tokens
- **Performance**: ✅ Fonctionne en bare-metal
- **Qualité**: Texte basique mais fonctionnel

## 🎯 Options d'Amélioration

### Option 1: Fine-tuning stories15M ⭐ RECOMMANDÉ
**Avantages:**
- ✅ Garde la taille (60 MB)
- ✅ Compatible bare-metal garanti
- ✅ Temps raisonnable (2-3 heures GPU)
- ✅ Meilleure cohérence de texte

**Commandes:**
```bash
cd llm-baremetal
wsl bash -c "source venv/bin/activate && python3 train_stories15m.py --finetune"
```

**Paramètres:**
- Plus d'époques sur TinyStories
- Learning rate réduit (1e-4)
- Batch size optimisé
- Validation loss tracking

---

### Option 2: Entraîner stories42M
**Avantages:**
- 🚀 Texte beaucoup plus cohérent
- 📚 Meilleure compréhension contextuelle
- 🎨 Vocabulaire plus riche

**Inconvénients:**
- ⏰ Temps long (6-8 heures GPU)
- 💾 Taille plus grande (~150 MB)
- 🔧 Peut nécessiter ajustements bare-metal

**Architecture:**
- dim=512
- 8 layers
- 8 heads
- Vocab: 32000

---

### Option 3: Utiliser NanoGPT-124M (Déjà téléchargé)
**Avantages:**
- ✅ Déjà présent (nanogpt.bin, 472 MB)
- 🎯 GPT-2 architecture éprouvée
- 📖 Pas besoin d'entraînement

**Inconvénients:**
- 💾 Fichier lourd (472 MB)
- ⏱️ Chargement plus long en bare-metal
- 🔧 Nécessite code multi-modèles

**Test rapide:**
```bash
# Modifier select_model() pour charger NanoGPT
# Recompiler et tester
```

---

### Option 4: Entraînement from Scratch
**Avantages:**
- 🎯 Contrôle total du dataset
- 🔧 Personnalisation complète
- 📚 Dataset TinyStories complet

**Inconvénients:**
- ⏰ Temps long (4-5 heures GPU)
- 💻 Nécessite bon GPU
- 🔬 Résultats variables

---

## 📋 Script d'Entraînement Fine-tuning

Créer `train_stories15m_finetune.py`:

```python
import torch
from torch.nn import functional as F
from dataclasses import dataclass
import numpy as np

@dataclass
class ModelConfig:
    dim: int = 288
    n_layers: int = 6
    n_heads: int = 6
    vocab_size: int = 32000
    max_seq_len: int = 256

# Configuration fine-tuning
learning_rate = 1e-4  # Réduit pour fine-tuning
batch_size = 64
max_iters = 50000
eval_interval = 1000

# Charger le modèle existant
checkpoint = torch.load('stories15M.pt')
model.load_state_dict(checkpoint['model'])

# Dataset TinyStories
# ... (code de chargement)

# Boucle d'entraînement améliorée
for iter in range(max_iters):
    # Training step
    optimizer.zero_grad()
    logits = model(x)
    loss = F.cross_entropy(logits.view(-1, vocab_size), y.view(-1))
    loss.backward()
    optimizer.step()
    
    # Validation
    if iter % eval_interval == 0:
        val_loss = evaluate()
        print(f"Iter {iter}: train_loss={loss:.4f}, val_loss={val_loss:.4f}")
        
        # Save checkpoint
        if val_loss < best_val_loss:
            torch.save({
                'model': model.state_dict(),
                'iter': iter,
                'val_loss': val_loss
            }, 'stories15M_finetuned.pt')

# Export to .bin
python3 export.py stories15M_finetuned.pt --output=stories15M_improved.bin
```

---

## 🚀 Plan d'Action Recommandé

### Phase 1: Test Rapide (maintenant)
1. Tester NanoGPT-124M existant (472 MB)
2. Vérifier si ça boot correctement
3. Comparer qualité vs stories15M

### Phase 2: Fine-tuning (2-3 heures)
1. Cloner llama2.c si pas déjà fait
2. Télécharger TinyStories complet
3. Lancer fine-tuning stories15M
4. Exporter vers .bin
5. Tester en bare-metal

### Phase 3: Éventuel Upgrade (si nécessaire)
1. Si fine-tuning insuffisant → stories42M
2. Entraîner modèle plus grand
3. Optimiser pour bare-metal

---

## 💻 Commandes Rapides

### Tester NanoGPT maintenant:
```bash
# Modifier llama2_efi.c pour utiliser nanogpt.bin
# Rebuild
make clean && make
make test-image
./test-qemu.ps1
```

### Lancer Fine-tuning:
```bash
wsl
cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal
git clone https://github.com/karpathy/llama2.c.git
cd llama2.c
python3 tinystories.py download
python3 train.py --finetune=../stories15M.bin --max_iters=50000 --learning_rate=1e-4
python3 export.py --checkpoint=out/model.pt --output=../stories15M_improved.bin
```

---

## ❓ Quelle Option Choisir?

**Tu as un GPU ?**
- ✅ Oui → Option 1 (fine-tuning) ou 2 (stories42M)
- ❌ Non → Option 3 (NanoGPT déjà prêt)

**Tu veux quoi ?**
- 🎯 Meilleure qualité, taille OK → Fine-tuning (Option 1)
- 🚀 Max qualité, taille pas grave → stories42M (Option 2)
- ⚡ Rapide, test maintenant → NanoGPT (Option 3)
- 🔬 Expérimentation complète → From scratch (Option 4)

---

## 📊 Comparaison Finale

| Option | Temps | Taille | Qualité | Difficulté |
|--------|-------|--------|---------|------------|
| Fine-tune | 2-3h | 60MB | ⭐⭐⭐⭐ | Facile |
| stories42M | 6-8h | 150MB | ⭐⭐⭐⭐⭐ | Moyen |
| NanoGPT | 0h | 472MB | ⭐⭐⭐⭐⭐ | Facile |
| From scratch | 4-5h | 60MB | ⭐⭐⭐⭐ | Difficile |

**Ma recommandation: Commence par tester NanoGPT (0 temps), puis fine-tune si besoin!**
