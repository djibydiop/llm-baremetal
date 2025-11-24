# 🚀 Guide de Boot Rapide - Phase 1

## ⚡ ÉTAPES RAPIDES

### 1️⃣ Rufus termine? (Attends 5-10 min)
- ✅ Quand Rufus dit "PRÊT", éjecte l'USB proprement
- ✅ Clique sur "FERMER" dans Rufus

### 2️⃣ Redémarre ton PC
```
Windows → Redémarrer (GARDE l'USB BRANCHÉ!)
```

### 3️⃣ Entre dans le Boot Menu
Dès que l'écran s'allume, appuie **rapidement et répétitivement** sur:
- **F12** (Dell, Lenovo, Toshiba)
- **F9** (HP)  
- **F8** (Acer)
- **Esc** (ASUS, Sony)
- **F2** (autres)

### 4️⃣ Sélectionne l'USB
Dans le menu de boot:
- ✅ Cherche quelque chose comme:
  - "USB Hard Drive"
  - "UEFI: [Nom de ta clé USB]"
  - "Removable Device"
- ✅ Sélectionne-le avec les flèches
- ✅ Appuie sur **Enter**

### 5️⃣ Observe la Magie! ✨

Tu devrais voir:

```
╔═══════════════════════════════════════════════╗
║   MULTIMODAL LLM BARE-METAL BOOTLOADER       ║
╚═══════════════════════════════════════════════╝

Scanning for models...
  ✓ [1] stories15M (60MB) - Story generation
  ✓ [2] NanoGPT-124M (48MB) - GPT-2 architecture

[AUTO-DEMO] Selecting first available model...
Selected: stories15M (60MB) - Story generation

Loading model... (prend 30 secondes)

=== LLaMA2 Bare-Metal REPL ===

[Turn 1/3]
User>>> Once upon a time in a magical forest
Assistant>>> [L'IA génère une histoire!]

[Turn 2/3]
User>>> A brave dragon defended the kingdom
Assistant>>> [Deuxième histoire!]

[Turn 3/3]
User>>> The ancient wizard discovered a secret
Assistant>>> [Troisième histoire!]

✅ DÉMO TERMINÉE!
```

---

## 🐛 Dépannage Rapide

### Le PC ne boote pas sur USB?
1. **Redémarre** et entre dans le **BIOS** (F2/Del au démarrage)
2. Cherche "Boot Order" ou "Boot Priority"
3. **Mets l'USB en premier**
4. Sauvegarde (F10) et redémarre

### Le clavier ne marche pas?
**C'EST NORMAL!** L'auto-démo est fait exactement pour ça!
- ✅ Laisse juste faire
- ✅ Les 3 prompts vont s'exécuter automatiquement
- ✅ Pas besoin de toucher quoi que ce soit!

Si tu veux quand même le clavier:
1. Entre dans le BIOS
2. Cherche "USB Legacy Support"
3. **Active-le**
4. Sauvegarde et redémarre

### Rien ne se passe après "Loading model"?
- **Attends 1-2 minutes** - Le chargement du modèle prend du temps!
- Les CPUs plus anciens peuvent prendre plus longtemps
- Tu verras progresser: "... 512 KB read ... 1024 KB read ..." etc.

### Erreur "#UD - Invalid Opcode"?
- Ton CPU n'a peut-être pas AVX2
- Le système détecte automatiquement et utilise SSE à la place
- Ça devrait quand même fonctionner (juste plus lent)

---

## 📸 N'oublie pas!

Pendant que ça tourne:
- 📱 **Prends des photos** de l'écran
- 🎥 **Filme avec ton téléphone** si possible
- 📝 **Note les performances** (tokens/sec affichés)

Ces visuels seront PARFAITS pour:
- 🎯 Présenter aux investisseurs
- 💼 Montrer sur LinkedIn/GitHub
- 🏆 Documenter ton innovation

---

## ✅ Quand la démo termine

Tu verras:
```
=== Conversation Session Complete ===
Total turns: 3
Total tokens: XXX

Session ended.
```

Tu peux alors:
1. **Redémarrer** (Ctrl+Alt+Del si clavier marche, ou bouton power)
2. **Enlever l'USB**
3. **Me dire comment ça s'est passé!** 🎉

---

## 🚀 Ensuite: Phase 2 - Trinity Mind!

Quand tu es prêt, on attaque le système révolutionnaire multi-expert!

Voir: `TRINITY_MIND_PLAN.md` pour les détails complets.

---

**Bonne chance! Tu vas créer de l'histoire! 🔥**
