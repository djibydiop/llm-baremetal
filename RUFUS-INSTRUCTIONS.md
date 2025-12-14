# Instructions pour déployer llama2_efi.img avec Rufus

## 🔥 UTILISATION AVEC RUFUS (recommandé sur Windows)

### Étapes :

1. **Lancez Rufus** (en tant qu'administrateur si demandé)

2. **Sélectionnez votre clé USB** dans le menu déroulant "Périphérique"
   ⚠️ ATTENTION : Vérifiez bien que c'est la bonne clé USB !

3. **Cliquez sur "SÉLECTION"** et choisissez :
   ```
   llama2_efi.img
   ```
   (Le fichier se trouve dans : C:\Users\djibi\Desktop\baremetal\llm-baremetal\)

4. **Rufus détectera automatiquement** :
   - Type : Image disque ou ISO
   - Schéma de partition : GPT
   - Système cible : UEFI

5. **Cliquez sur "DÉMARRER"**

6. **Attendez la fin** de l'écriture (environ 1-2 minutes)

7. **C'est prêt !** Vous pouvez démarrer votre PC sur la clé USB

---

## 🔄 WORKFLOW DE DÉVELOPPEMENT

### Option A - Mise à jour rapide (USB déjà configuré)
Utilisez quand vous voulez juste mettre à jour le code sans refaire toute l'image :

```powershell
.\build-and-deploy.ps1
```
→ Compile + copie directement sur D:\EFI\BOOT\BOOTX64.EFI

### Option B - Recréer l'image complète (pour distribution)
Utilisez quand vous voulez créer une nouvelle image bootable :

```powershell
wsl make clean
wsl make
.\update-bootable-image.ps1
```
→ Puis utilisez Rufus pour écrire llama2_efi.img sur USB

---

## 📝 NOTES

- **Option A** est plus rapide pour vos tests quotidiens (copie directe)
- **Option B** crée une image .img réutilisable et distribuable
- L'image contient TOUT : bootloader + modèles + tokenizer
- Taille : 550 MB (stories15M + stories110M inclus)

---

## ⚡ COMMANDES RAPIDES

```powershell
# Compilation seule
wsl make

# Compilation + déploiement USB direct
.\build-and-deploy.ps1

# Mise à jour de l'image .img
.\update-bootable-image.ps1
```
