# 🐛 Debug Boot Issue - File System Access

## Problème
Erreur persistante: **"Failed to get loaded image protocol: Invalid Parameter"**

## Tests effectués
1. ✅ QEMU boot fonctionne
2. ✅ Bannière s'affiche
3. ✅ Messages [DEBUG] apparaissent
4. ❌ HandleProtocol retourne "Invalid Parameter"

## Code actuel
```c
EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
Status = SystemTable->BootServices->HandleProtocol(
    ImageHandle,
    &LoadedImageProtocol,
    (VOID**)&LoadedImage
);
// Retourne: Invalid Parameter
```

## Hypothèses
1. **GUID mal défini** - Possible problème avec EFI_LOADED_IMAGE_PROTOCOL_GUID
2. **ImageHandle invalide** - Mais ça semble peu probable
3. **Mauvaise syntaxe de cast** - (VOID**) vs (void **)
4. **Problème avec gnu-efi** - Incompatibilité de définition

## Solution à tester
Au lieu de définir le GUID localement, utiliser directement la constante globale gnu-efi :

```c
// Option 1: Utiliser LoadedImageProtocol (déjà défini dans gnu-efi)
Status = uefi_call_wrapper(BS->HandleProtocol, 3,
    ImageHandle,
    &LoadedImageProtocol,
    (void**)&LoadedImage
);

// Option 2: Utiliser LocateProtocol au lieu de HandleProtocol
Status = uefi_call_wrapper(BS->LocateProtocol, 3,
    &LoadedImageProtocol,
    NULL,
    (void**)&LoadedImage
);

// Option 3: Définir GUID manuellement
static EFI_GUID LoadedImageProtocol = 
    { 0x5B1B31A1, 0x9562, 0x11d2, 
      { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }};
```

## Prochaine étape
Essayer d'utiliser `uefi_call_wrapper` qui est la macro recommandée par gnu-efi.
