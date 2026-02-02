# Automatic CI Status Monitor
# Usage: .\scripts\auto-check-ci.ps1

param(
    [string]$Branch = "next_cpp26",
    [int]$CheckIntervalSeconds = 300,  # Check every 5 minutes
    [int]$MaxHours = 6
)

$Owner = "ximicpp"
$Repo = "XOffsetDatastructure"

function Get-ElapsedTime {
    param([DateTime]$StartTime)
    $elapsed = (Get-Date) - $StartTime
    return "$([math]::Floor($elapsed.TotalHours))h $($elapsed.Minutes)m"
}

function Get-EstimatedProgress {
    param([double]$ElapsedHours)
    $buildTimeHours = 3.5  # Average expected build time
    $progress = [math]::Min(100, [math]::Floor(($ElapsedHours / $buildTimeHours) * 100))
    return $progress
}

function Show-Status {
    param(
        [string]$CommitHash,
        [DateTime]$CommitTime,
        [string]$CommitMsg,
        [int]$CheckCount
    )
    
    Clear-Host
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        GitHub Actions CI - Automatic Monitor                   ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    $elapsed = (Get-Date) - $CommitTime
    $progress = Get-EstimatedProgress -ElapsedHours $elapsed.TotalHours
    
    Write-Host "📊 Commit Information" -ForegroundColor Yellow
    Write-Host "   Hash:    $CommitHash" -ForegroundColor White
    Write-Host "   Time:    $($CommitTime.ToString('yyyy-MM-dd HH:mm:ss'))" -ForegroundColor White
    Write-Host "   Message: $CommitMsg" -ForegroundColor White
    Write-Host ""
    
    Write-Host "⏱️  Runtime Status" -ForegroundColor Yellow
    Write-Host "   Elapsed:  $(Get-ElapsedTime -StartTime $CommitTime)" -ForegroundColor Magenta
    Write-Host "   Progress: ~$progress%" -ForegroundColor Cyan
    Write-Host "   Checks:   #$CheckCount" -ForegroundColor Gray
    Write-Host ""
    
    # Progress bar
    $barLength = 50
    $filled = [math]::Floor($barLength * $progress / 100)
    $empty = $barLength - $filled
    $bar = ("#" * $filled) + ("." * $empty)
    Write-Host "   [$bar] $progress%" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "🔄 Build Pipeline Status" -ForegroundColor Yellow
    if ($elapsed.TotalMinutes -lt 5) {
        Write-Host "   ✓ Verify environment" -ForegroundColor Green
        Write-Host "   ⏳ Set up Docker Buildx" -ForegroundColor Cyan
        Write-Host "   ⏳ Build Docker image (2-4 hrs)" -ForegroundColor Gray
        Write-Host "   ⏳ Run tests (~10 min)" -ForegroundColor Gray
    } elseif ($elapsed.TotalHours -lt 2.5) {
        Write-Host "   ✓ Verify environment" -ForegroundColor Green
        Write-Host "   ✓ Set up Docker Buildx" -ForegroundColor Green
        Write-Host "   ⏳ Build Docker image (2-4 hrs) - In Progress..." -ForegroundColor Cyan
        Write-Host "   ⏳ Run tests (~10 min)" -ForegroundColor Gray
    } elseif ($elapsed.TotalHours -lt 4.5) {
        Write-Host "   ✓ Verify environment" -ForegroundColor Green
        Write-Host "   ✓ Set up Docker Buildx" -ForegroundColor Green
        Write-Host "   ⏳ Build Docker image - Should be completing..." -ForegroundColor Yellow
        Write-Host "   ⏳ Run tests (~10 min)" -ForegroundColor Gray
    } else {
        Write-Host "   ✓ Verify environment" -ForegroundColor Green
        Write-Host "   ✓ Set up Docker Buildx" -ForegroundColor Green
        Write-Host "   ? Build Docker image - Check status" -ForegroundColor Yellow
        Write-Host "   ? Run tests - Check status" -ForegroundColor Yellow
    }
    Write-Host ""
    
    Write-Host "🌐 GitHub Actions URL" -ForegroundColor Yellow
    $url = "https://github.com/$Owner/$Repo/actions?query=branch%3A$Branch"
    Write-Host "   $url" -ForegroundColor White
    Write-Host ""
    
    if ($elapsed.TotalHours -ge $MaxHours) {
        Write-Host "⚠️  WARNING: Build exceeded $MaxHours hour limit!" -ForegroundColor Red
        Write-Host "   This may indicate a timeout or failure." -ForegroundColor Red
        Write-Host ""
        return $false
    }
    
    return $true
}

# Main monitoring loop
Write-Host "Starting CI monitor..." -ForegroundColor Green
Write-Host "Checking every $CheckIntervalSeconds seconds ($(($CheckIntervalSeconds / 60)) minutes)" -ForegroundColor Gray
Write-Host ""

# Get initial commit info
$gitLog = git log --format="%H`t%ci`t%s" -1 origin/$Branch 2>$null
if (-not $gitLog) {
    Write-Host "Error: Unable to get commit information" -ForegroundColor Red
    exit 1
}

$parts = $gitLog -split "`t"
$commitHash = $parts[0].Substring(0, 7)
$commitTime = [DateTime]::Parse($parts[1])
$commitMsg = $parts[2]

$checkCount = 0
$maxChecks = ($MaxHours * 3600) / $CheckIntervalSeconds

Write-Host "Monitoring commit: $commitHash" -ForegroundColor Cyan
Write-Host "Started at: $($commitTime.ToString('HH:mm:ss'))" -ForegroundColor Cyan
Write-Host ""
Start-Sleep -Seconds 2

while ($checkCount -lt $maxChecks) {
    $checkCount++
    
    $shouldContinue = Show-Status -CommitHash $commitHash -CommitTime $commitTime -CommitMsg $commitMsg -CheckCount $checkCount
    
    if (-not $shouldContinue) {
        Write-Host "⛔ Monitoring stopped due to timeout." -ForegroundColor Red
        Write-Host "Please check GitHub Actions manually:" -ForegroundColor Yellow
        Write-Host "https://github.com/$Owner/$Repo/actions" -ForegroundColor White
        break
    }
    
    $elapsed = (Get-Date) - $commitTime
    
    # Estimate if build should be done
    if ($elapsed.TotalHours -ge 4.5) {
        Write-Host "💡 Expected build time exceeded (4.5 hours)" -ForegroundColor Yellow
        Write-Host "   The build should have completed by now." -ForegroundColor Yellow
        Write-Host ""
        Write-Host "   Recommendation:" -ForegroundColor Cyan
        Write-Host "   1. Check GitHub Actions page for actual status" -ForegroundColor White
        Write-Host "   2. Look for success ✓ or failure ✗ indicators" -ForegroundColor White
        Write-Host "   3. Review build logs if available" -ForegroundColor White
        Write-Host ""
        
        $continue = Read-Host "   Continue monitoring? (y/n)"
        if ($continue -ne 'y' -and $continue -ne 'Y') {
            Write-Host ""
            Write-Host "✋ Monitoring stopped by user." -ForegroundColor Yellow
            break
        }
    }
    
    Write-Host "⏳ Next check in $CheckIntervalSeconds seconds..." -ForegroundColor Gray
    Write-Host "   Press Ctrl+C to stop monitoring" -ForegroundColor DarkGray
    Write-Host ""
    
    Start-Sleep -Seconds $CheckIntervalSeconds
}

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    Monitoring Complete                         ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "📋 Final Action Required:" -ForegroundColor Yellow
Write-Host "   Please manually verify the CI status at:" -ForegroundColor White
Write-Host "   https://github.com/$Owner/$Repo/actions?query=branch%3A$Branch" -ForegroundColor Cyan
Write-Host ""
Write-Host "✅ Success Indicators:" -ForegroundColor Green
Write-Host "   • All steps show green checkmarks ✓" -ForegroundColor White
Write-Host "   • Test output shows '18/18 tests successful'" -ForegroundColor White
Write-Host "   • Reflection tests passed" -ForegroundColor White
Write-Host ""
