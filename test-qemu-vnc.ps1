# Test QEMU avec WSL (méthode VNC)
# Lance QEMU en arrière-plan avec VNC, puis connectez-vous avec un client VNC

Write-Host "=== LANCEMENT QEMU VIA WSL (VNC) ===" -ForegroundColor Cyan
Write-Host ""

# Copier le binaire dans qemu-mini
Copy-Item "llama2.efi" "..\qemu-mini\llama2.efi" -Force
Write-Host "✓ Binaire copié vers qemu-mini" -ForegroundColor Green

# Lancer QEMU avec VNC
Write-Host "Lancement de QEMU avec VNC sur port 5900..." -ForegroundColor Yellow

wsl bash -c "cd /mnt/c/Users/djibi/Desktop/baremetal/qemu-mini && qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -hda fat:rw:. -m 2048M -cpu qemu64,+sse2 -smp 2 -vnc :0 > /tmp/qemu.log 2>&1 &"

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "=== QEMU LANCÉ ===" -ForegroundColor Green
Write-Host ""
Write-Host "Connectez-vous avec un client VNC:" -ForegroundColor Cyan
Write-Host "  Adresse: localhost:5900" -ForegroundColor White
Write-Host ""
Write-Host "Clients VNC recommandés:" -ForegroundColor Yellow
Write-Host "  - TightVNC Viewer (Windows)" -ForegroundColor Gray
Write-Host "  - RealVNC Viewer" -ForegroundColor Gray
Write-Host "  - UltraVNC" -ForegroundColor Gray
Write-Host ""
Write-Host "Pour arrêter QEMU:" -ForegroundColor Cyan
Write-Host "  wsl bash -c 'pkill qemu-system-x86_64'" -ForegroundColor White
