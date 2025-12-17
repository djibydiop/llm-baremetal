# Creation image USB complete avec LLM
Write-Host "=== Image USB Complete - LLM Baremetal ===" -ForegroundColor Cyan
Write-Host ""

$imageName = "llm-baremetal-complete.img"
$imageSize = 512  # MB - pour avoir de la marge
$label = "LLMBOOT"

# Fichiers requis
$files = @{
    "llama2.efi" = "EFI/BOOT/BOOTX64.EFI"
    "iwlwifi-cc-a0-72.ucode" = "iwlwifi-cc-a0-72.ucode"
    "stories15M.bin" = "stories15M.bin"
    "tokenizer.bin" = "tokenizer.bin"
}

# Verification
Write-Host "Verification des fichiers..." -ForegroundColor Yellow
foreach ($file in $files.Keys) {
    if (-not (Test-Path $file)) {
        Write-Host "ERREUR: $file non trouve!" -ForegroundColor Red
        exit 1
    }
    $size = (Get-Item $file).Length / 1MB
    Write-Host "  OK $file ($($size.ToString('0.00')) MB)" -ForegroundColor Green
}
Write-Host ""

# Suppression ancienne image
if (Test-Path $imageName) {
    Remove-Item $imageName -Force
    Write-Host "Ancienne image supprimee" -ForegroundColor Yellow
}

Write-Host "Etape 1/6: Creation image ${imageSize}MB..." -ForegroundColor Cyan
wsl bash -c "dd if=/dev/zero of=/mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal/$imageName bs=1M count=$imageSize status=progress"

Write-Host ""
Write-Host "Etape 2/6: Table partition GPT..." -ForegroundColor Cyan
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && sgdisk -Z $imageName"
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && sgdisk -n 1:2048:0 -t 1:EF00 -c 1:'EFI System' $imageName"

Write-Host ""
Write-Host "Etape 3/6: Formatage FAT32..." -ForegroundColor Cyan
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mformat -i $imageName@@1M -F -v $label ::"

Write-Host ""
Write-Host "Etape 4/6: Creation structure EFI..." -ForegroundColor Cyan
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mmd -i $imageName@@1M ::/EFI"
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mmd -i $imageName@@1M ::/EFI/BOOT"

Write-Host ""
Write-Host "Etape 5/6: Copie des fichiers..." -ForegroundColor Cyan
Write-Host "  - BOOTX64.EFI (llama2.efi)..." -ForegroundColor Gray
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mcopy -i $imageName@@1M llama2.efi ::/EFI/BOOT/BOOTX64.EFI"

Write-Host "  - Firmware WiFi (1.27 MB)..." -ForegroundColor Gray
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mcopy -i $imageName@@1M iwlwifi-cc-a0-72.ucode ::/"

Write-Host "  - Modele LLM stories15M.bin (58 MB)..." -ForegroundColor Gray
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mcopy -i $imageName@@1M stories15M.bin ::/"

Write-Host "  - Tokenizer BPE (tokenizer.bin)..." -ForegroundColor Gray
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mcopy -i $imageName@@1M tokenizer.bin ::/"

Write-Host ""
Write-Host "Etape 6/6: Verification..." -ForegroundColor Cyan
wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/llm-baremetal && mdir -i $imageName@@1M ::/ -/"

Write-Host ""
Write-Host "=== IMAGE COMPLETE CREEE ===" -ForegroundColor Green
Write-Host ""
$finalSize = (Get-Item $imageName).Length / 1MB
Write-Host "Fichier: $imageName ($($finalSize.ToString('0.00')) MB)" -ForegroundColor Cyan
Write-Host ""
Write-Host "Contenu:" -ForegroundColor Yellow
Write-Host "  - BOOTX64.EFI (llama2.efi avec DRC v5.1 + WiFi)" -ForegroundColor Gray
Write-Host "  - iwlwifi-cc-a0-72.ucode (firmware Intel AX200)" -ForegroundColor Gray
Write-Host "  - stories15M.bin (modele Transformer 15M)" -ForegroundColor Gray
Write-Host ""
Write-Host "Test QEMU:" -ForegroundColor Cyan
Write-Host '  & "C:\Program Files\qemu\qemu-system-x86_64.exe" -drive "if=pflash,format=raw,readonly=on,file=C:\Program Files\qemu\share\edk2-x86_64-code.fd" -drive "file=llm-baremetal-complete.img,format=raw,if=ide" -m 512 -cpu qemu64 -vga std -serial stdio' -ForegroundColor Gray
Write-Host ""
Write-Host "Flash USB:" -ForegroundColor Cyan
Write-Host "  1. balenaEtcher: https://etcher.balena.io/" -ForegroundColor Gray
Write-Host "  2. wsl sudo dd if=$imageName of=/dev/sdX bs=4M status=progress" -ForegroundColor Gray
Write-Host ""
Write-Host "BIOS Setup:" -ForegroundColor Red
Write-Host "  - Secure Boot: DISABLED" -ForegroundColor Gray
Write-Host "  - Boot Mode: UEFI" -ForegroundColor Gray
Write-Host "  - Boot Order: USB First (ou F12)" -ForegroundColor Gray
Write-Host ""
