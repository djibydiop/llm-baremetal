# TÉLÉCHARGEMENT MANUEL DU FIRMWARE - INSTRUCTIONS SIMPLES

## ⚠️ Le téléchargement automatique ne fonctionne pas

Les serveurs kernel.org bloquent les téléchargements directs. **Vous devez télécharger manuellement**.

## 📥 MÉTHODE FACILE (5 minutes)

### Étape 1: Ouvrir le navigateur
Copier-coller cette URL dans votre navigateur:
```
https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/
```

### Étape 2: Chercher le firmware
- Sur la page, appuyez sur `Ctrl+F`
- Tapez: `iwlwifi-cc-a0-72.ucode`
- Cliquez sur le fichier dans la liste

### Étape 3: Télécharger
- Vous verrez les infos du fichier
- En haut à droite, cliquez sur le bouton **"plain"**
- Le fichier se télécharge (464 KB)

### Étape 4: Déplacer le fichier
Déplacer `iwlwifi-cc-a0-72.ucode` dans:
```
C:\Users\djibi\Desktop\baremetal\llm-baremetal\
```

### Étape 5: Vérifier
Dans PowerShell:
```powershell
Get-Item C:\Users\djibi\Desktop\baremetal\llm-baremetal\iwlwifi-cc-a0-72.ucode
```

Le fichier doit faire **~464 KB** (pas 4 KB!)

---

## 🔄 ALTERNATIVE: GitHub Mirror (Plus rapide)

Le firmware est aussi disponible sur GitHub:

### URL directe:
```
https://github.com/torvalds/linux-firmware/raw/main/iwlwifi-cc-a0-72.ucode
```

### Téléchargement PowerShell:
```powershell
cd C:\Users\djibi\Desktop\baremetal\llm-baremetal
Invoke-WebRequest -Uri "https://github.com/torvalds/linux-firmware/raw/main/iwlwifi-cc-a0-72.ucode" -OutFile "iwlwifi-cc-a0-72.ucode"
```

---

## ✅ APRÈS LE TÉLÉCHARGEMENT

Une fois le firmware téléchargé (464 KB), ajouter à l'image USB:

```powershell
# Créer nouvelle image avec firmware
cd C:\Users\djibi\Desktop\baremetal\llm-baremetal

# 1. Créer l'image
wsl bash -c 'make disk'

# 2. Monter l'image (Windows)
$img = Mount-DiskImage -ImagePath "$PWD\qemu-test.img" -PassThru
$drive = ($img | Get-Volume).DriveLetter

# 3. Copier firmware
Copy-Item iwlwifi-cc-a0-72.ucode "${drive}:\"

# 4. Démonter
Dismount-DiskImage -ImagePath "$PWD\qemu-test.img"

Write-Host "✓ Firmware ajouté à l'image!" -ForegroundColor Green
```

---

**ENSUITE**: Testez dans QEMU ou créez l'image USB pour hardware réel!
