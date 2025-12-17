# Instructions Upload GitHub

## Étape 1: Créer le repository

1. Aller sur https://github.com/new
2. Repository name: `llm-baremetal`
3. Description: "Bare-metal LLM with network boot - Made in Senegal 🇸🇳"
4. Public
5. Ne PAS initialiser avec README (on l'a déjà)

## Étape 2: Initialiser Git localement

```powershell
cd C:\Users\djibi\Desktop\baremetal\llm-baremetal

# Initialiser Git
git init

# Configurer remote
git remote add origin https://github.com/djibydiop/llm-baremetal.git

# Configurer Git LFS (pour les gros fichiers)
git lfs install
```

## Étape 3: Configurer Git LFS

GitHub limite les fichiers à 100 MB. Nos fichiers:
- ✅ llama2.efi: 720 KB (OK)
- ✅ tokenizer.bin: 434 KB (OK)
- ⚠️ stories15M.bin: 58 MB (OK mais limite)
- ❌ llm-baremetal-complete.img: 512 MB (TROP GROS)

**Solution: Git LFS**

```powershell
# Tracker les gros fichiers
git lfs track "*.bin"
git lfs track "*.img"
git lfs track "*.efi"

# Add .gitattributes
git add .gitattributes
```

## Étape 4: Add et Commit

```powershell
# Ajouter tous les fichiers
git add llama2.efi
git add stories15M.bin
git add tokenizer.bin
git add llm-baremetal-complete.img
git add llm-network-boot.img
git add README.md
git add *.ps1
git add *.c
git add *.h
git add Makefile

# Commit
git commit -m "Initial commit: Baremetal LLM with GitHub network boot"
```

## Étape 5: Push

```powershell
# Premier push
git branch -M main
git push -u origin main
```

## Étape 6: Vérifier les URLs

Après upload, vérifier que ces URLs fonctionnent:

```
https://raw.githubusercontent.com/djibydiop/llm-baremetal/main/stories15M.bin
https://raw.githubusercontent.com/djibydiop/llm-baremetal/main/tokenizer.bin
https://raw.githubusercontent.com/djibydiop/llm-baremetal/main/llama2.efi
```

## Étape 7: Tester le boot réseau

1. Flasher `llm-network-boot.img` (2 MB)
2. Connecter Ethernet/WiFi
3. Boot
4. Le système télécharge automatiquement depuis GitHub!

## Alternative: GitHub Releases

Si Git LFS pose problème, utiliser Releases:

1. Aller sur https://github.com/djibydiop/llm-baremetal/releases/new
2. Tag: `v1.0`
3. Title: "Baremetal LLM v1.0 - Network Boot"
4. Upload les fichiers binaires
5. Publish release

URLs seront alors:
```
https://github.com/djibydiop/llm-baremetal/releases/download/v1.0/stories15M.bin
```

## Notes

- GitHub LFS gratuit: 1 GB storage + 1 GB bandwidth/mois
- Au-delà: $5/mois pour 50 GB storage + 50 GB bandwidth
- Alternative: Héberger les gros fichiers ailleurs (Dropbox, Google Drive)

---

**Made in Senegal 🇸🇳**
