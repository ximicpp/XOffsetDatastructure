# GitHub CI Status Checker (No Token Required)
# Usage: .\scripts\check-ci-status.ps1

param(
    [string]$Owner = "ximicpp",
    [string]$Repo = "XOffsetDatastructure",
    [string]$Branch = "next_cpp26"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "GitHub Actions CI Status Checker" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Get local commit info
Write-Host "[1] Local Repository Status" -ForegroundColor Yellow
$gitLog = git log --format="%H`t%ci`t%s" -1 origin/$Branch 2>$null
if ($gitLog) {
    $parts = $gitLog -split "`t"
    $commitHash = $parts[0].Substring(0, 7)
    $commitTime = $parts[1]
    $commitMsg = $parts[2]
    
    Write-Host "   Branch: $Branch" -ForegroundColor White
    Write-Host "   Commit: $commitHash" -ForegroundColor White
    Write-Host "   Time:   $commitTime" -ForegroundColor White
    Write-Host "   Message: $commitMsg" -ForegroundColor White
    
    # Calculate elapsed time
    $commitDate = [DateTime]::Parse($commitTime)
    $now = Get-Date
    $elapsed = $now - $commitDate
    Write-Host "   Elapsed: $([math]::Floor($elapsed.TotalHours))h $($elapsed.Minutes)m" -ForegroundColor Magenta
} else {
    Write-Host "   Unable to get local commit info" -ForegroundColor Red
}

Write-Host "`n[2] GitHub Actions URLs" -ForegroundColor Yellow
$actionsUrl = "https://github.com/$Owner/$Repo/actions"
$branchUrl = "https://github.com/$Owner/$Repo/actions?query=branch%3A$Branch"
$commitUrl = "https://github.com/$Owner/$Repo/commit/$($parts[0])"

Write-Host "   All Actions:    $actionsUrl" -ForegroundColor White
Write-Host "   Branch Runs:    $branchUrl" -ForegroundColor White
Write-Host "   Commit Details: $commitUrl" -ForegroundColor White

Write-Host "`n[3] Expected CI Timeline" -ForegroundColor Yellow
Write-Host "   Step 1: Verify environment      (~1 min)   ✓" -ForegroundColor Gray
Write-Host "   Step 2: Set up Docker Buildx    (~1 min)   ✓" -ForegroundColor Gray
Write-Host "   Step 3: Build Docker image      (2-4 hrs)  ⏳" -ForegroundColor Cyan
Write-Host "   Step 4: Run tests in Docker     (~10 min)  ⏳" -ForegroundColor Gray

Write-Host "`n[4] Current Estimation" -ForegroundColor Yellow
if ($elapsed.TotalHours -lt 2) {
    Write-Host "   Status: Likely still building Docker image..." -ForegroundColor Cyan
    Write-Host "   Progress: ~$([math]::Floor(($elapsed.TotalHours / 4) * 100))% estimated" -ForegroundColor Cyan
} elseif ($elapsed.TotalHours -lt 4) {
    Write-Host "   Status: Docker build should be near completion" -ForegroundColor Yellow
    Write-Host "   Progress: ~$([math]::Floor(($elapsed.TotalHours / 4) * 100))% estimated" -ForegroundColor Yellow
} elseif ($elapsed.TotalHours -lt 5) {
    Write-Host "   Status: Should be running tests or completed" -ForegroundColor Green
} else {
    Write-Host "   Status: May have timed out or completed" -ForegroundColor Red
    Write-Host "   Note: GitHub Actions timeout is 6 hours" -ForegroundColor Red
}

Write-Host "`n[5] Manual Check Instructions" -ForegroundColor Yellow
Write-Host "   1. Open: $actionsUrl" -ForegroundColor White
Write-Host "   2. Look for the latest 'CI' workflow run" -ForegroundColor White
Write-Host "   3. Check if status shows:" -ForegroundColor White
Write-Host "      • Green ✓ = Success" -ForegroundColor Green
Write-Host "      • Yellow ● = In Progress" -ForegroundColor Yellow
Write-Host "      • Red ✗ = Failed" -ForegroundColor Red

Write-Host "`n[6] Quick Browser Open" -ForegroundColor Yellow
$openBrowser = Read-Host "   Open GitHub Actions in browser? (y/n)"
if ($openBrowser -eq 'y' -or $openBrowser -eq 'Y') {
    Start-Process $branchUrl
    Write-Host "   Browser opened!" -ForegroundColor Green
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Check complete. Please verify manually." -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan
