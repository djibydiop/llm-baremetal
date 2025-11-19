# Setup GitHub Remote and Push

Write-Host "Setting up GitHub remote..." -ForegroundColor Cyan
Write-Host ""
Write-Host "Steps:" -ForegroundColor Yellow
Write-Host "1. Go to https://github.com/new" -ForegroundColor White
Write-Host "2. Repository name: llm-baremetal" -ForegroundColor White
Write-Host "3. Make it PUBLIC" -ForegroundColor Green
Write-Host "4. DON'T initialize with README (we already have one)" -ForegroundColor White
Write-Host "5. Create repository" -ForegroundColor White
Write-Host ""
Write-Host "Then run these commands:" -ForegroundColor Yellow
Write-Host ""
Write-Host "git remote add origin https://github.com/npdji/llm-baremetal.git" -ForegroundColor Cyan
Write-Host "git branch -M main" -ForegroundColor Cyan
Write-Host "git push -u origin main" -ForegroundColor Cyan
Write-Host ""
Write-Host "Press Enter when GitHub repo is created..." -ForegroundColor Yellow
Read-Host

# Add remote
git remote add origin https://github.com/npdji/llm-baremetal.git

# Rename branch to main
git branch -M main

# Push
git push -u origin main

Write-Host ""
Write-Host "✓ Pushed to GitHub!" -ForegroundColor Green
Write-Host ""
Write-Host "Share this with Justine:" -ForegroundColor Cyan
Write-Host "https://github.com/npdji/llm-baremetal" -ForegroundColor White
