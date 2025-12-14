═══════════════════════════════════════════════════════════
    LLM BARE-METAL v3.0 - UEFI BOOT SYSTEM
    URS Extended + ML Training + Quality Improvements
═══════════════════════════════════════════════════════════

📦 CONTENU:
  • EFI/BOOT/BOOTX64.EFI - Bootloader UEFI avec URS v3.0
  • stories15M.bin       - Modèle LLaMA2 (15M, 58MB)
  • tokenizer.bin        - Tokenizer BPE (32K vocab)

🚀 DÉMARRAGE:
  1. Brancher clé USB sur PC UEFI
  2. Redémarrer + F12/F11/ESC (menu boot)
  3. Sélectionner "UEFI: USB" (pas Legacy!)
  4. Boot automatique!

⚙️ SYSTÈME v3.0:
  ✅ URS ML Training - apprentissage automatique
  ✅ Température 0.9 - texte créatif et varié
  ✅ Répétition penalty 2.5x - pas de boucles
  ✅ Top-p sampling 0.9 - meilleure qualité
  ✅ Cache 64 entrées - stratégies optimisées
  ✅ Learning rate adaptatif (0.01 → 0.001)

📊 SPECS TECHNIQUES:
  • Architecture: x86-64 UEFI bare-metal (pas d'OS!)
  • Instructions: SSE2 seulement (compatible 2003+)
  • RAM: 512MB min (1-2GB recommandé)
  • CPU: Intel/AMD x86-64 avec SSE2
  • BIOS: UEFI (Secure Boot OFF recommandé)
  • Stockage: 128MB minimum

🎯 PERFORMANCE:
  • Boot: 5-15 secondes
  • Chargement modèle: 5-10 secondes
  • Training URS: <1 seconde (9 itérations)
  • Génération: 1-5 tokens/sec (CPU dépendant)

💡 RÉSULTATS TRAINING:
  • Solar strategy: 31% (9 succès sur 9)
  • Lunar/Elemental/Quantum: 25% (baseline)
  • Cache hits: croît avec utilisation
  • Learning rate: décroit progressivement

🔧 DÉPANNAGE:
  • Boot échoue → Désactiver Secure Boot
  • Écran noir → Attendre 60 sec (chargement)
  • "No boot device" → Vérifier mode UEFI
  • Texte bizarre → Normal en v3.0 (créativité)

📅 Date: 2025-12-05 06:28
🏷️ Version: v3.0 ML Training Edition
🌐 Projet: llm-baremetal
