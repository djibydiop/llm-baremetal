# Lance create-vhd-image.ps1 avec droits administrateur
Start-Process pwsh -Verb RunAs -ArgumentList "-NoExit", "-Command", "cd '$PWD'; .\create-vhd-image.ps1"
Write-Host "`n✅ PowerShell Admin lancé!" -ForegroundColor Green
Write-Host "   Le script s'exécute dans la nouvelle fenêtre...`n" -ForegroundColor Yellow
