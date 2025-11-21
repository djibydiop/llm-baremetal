# Test script with output capture
Write-Host "Building and testing LLaMA2..."

wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && make llama2-disk"

Write-Host "`nRunning QEMU (30 seconds)...`n"

$output = wsl bash -c "cd /mnt/c/Users/djibi/Desktop/yama_oo/yama_oo/llm-baremetal && timeout 30 qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=llama2-disk.img,format=raw -m 512M -nographic -serial mon:stdio 2>&1"

Write-Host $output

# Save to file
$output | Out-File -FilePath "qemu_output_captured.txt" -Encoding UTF8

Write-Host "`n`nOutput saved to qemu_output_captured.txt"
