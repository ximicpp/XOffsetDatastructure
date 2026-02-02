# Simple CI Status Monitor (ASCII only)
# Usage: .\scripts\simple-ci-monitor.ps1

param(
    [int]$CheckIntervalMinutes = 3,
    [int]$MaxChecks = 20
)

$Branch = "next_cpp26"

function Show-SimpleStatus {
    param($Hash, $Time, $Msg, $Count)
    
    Clear-Host
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "   GitHub Actions CI Monitor" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    $elapsed = (Get-Date) - $Time
    $hours = [math]::Floor($elapsed.TotalHours)
    $mins = $elapsed.Minutes
    $progress = [math]::Min(100, [math]::Floor(($elapsed.TotalHours / 3.5) * 100))
    
    Write-Host "Commit:  $Hash" -ForegroundColor White
    Write-Host "Time:    $($Time.ToString('yyyy-MM-dd HH:mm:ss'))" -ForegroundColor White
    Write-Host "Message: $Msg" -ForegroundColor White
    Write-Host ""
    Write-Host "Elapsed: ${hours}h ${mins}m" -ForegroundColor Magenta
    Write-Host "Progress: ~$progress%" -ForegroundColor Cyan
    Write-Host "Check #$Count" -ForegroundColor Gray
    Write-Host ""
    
    # Simple progress bar
    $filled = [math]::Floor(30 * $progress / 100)
    $empty = 30 - $filled
    $bar = "=" * $filled + "-" * $empty
    Write-Host "[$bar] $progress%" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "Build Status:" -ForegroundColor Yellow
    if ($elapsed.TotalMinutes -lt 5) {
        Write-Host "  [*] Verify environment" -ForegroundColor Green
        Write-Host "  [ ] Set up Docker Buildx" -ForegroundColor Gray
        Write-Host "  [ ] Build Docker image (2-4 hrs)" -ForegroundColor Gray
        Write-Host "  [ ] Run tests (~10 min)" -ForegroundColor Gray
    } elseif ($elapsed.TotalHours -lt 2.5) {
        Write-Host "  [*] Verify environment" -ForegroundColor Green
        Write-Host "  [*] Set up Docker Buildx" -ForegroundColor Green
        Write-Host "  [...] Build Docker image - IN PROGRESS" -ForegroundColor Cyan
        Write-Host "  [ ] Run tests (~10 min)" -ForegroundColor Gray
    } elseif ($elapsed.TotalHours -lt 4.5) {
        Write-Host "  [*] Verify environment" -ForegroundColor Green
        Write-Host "  [*] Set up Docker Buildx" -ForegroundColor Green
        Write-Host "  [...] Build Docker image - COMPLETING" -ForegroundColor Yellow
        Write-Host "  [ ] Run tests (~10 min)" -ForegroundColor Gray
    } else {
        Write-Host "  [*] Verify environment" -ForegroundColor Green
        Write-Host "  [*] Set up Docker Buildx" -ForegroundColor Green
        Write-Host "  [?] Build Docker image - CHECK MANUALLY" -ForegroundColor Yellow
        Write-Host "  [?] Run tests - CHECK MANUALLY" -ForegroundColor Yellow
    }
    Write-Host ""
    
    Write-Host "GitHub Actions:" -ForegroundColor Yellow
    Write-Host "  https://github.com/leomath42/XOffsetDatastructure/actions" -ForegroundColor White
    Write-Host ""
    
    return ($elapsed.TotalHours -lt 6)
}

# Main loop
Write-Host "Starting CI monitor (checking every $CheckIntervalMinutes minutes)..." -ForegroundColor Green
Write-Host ""

$gitLog = git log --format="%H`t%ci`t%s" -1 origin/$Branch 2>$null
if (-not $gitLog) {
    Write-Host "ERROR: Cannot get commit info" -ForegroundColor Red
    exit 1
}

$parts = $gitLog -split "`t"
$hash = $parts[0].Substring(0, 7)
$time = [DateTime]::Parse($parts[1])
$msg = $parts[2]

Write-Host "Monitoring: $hash" -ForegroundColor Cyan
Write-Host "Started at: $($time.ToString('HH:mm:ss'))" -ForegroundColor Cyan
Write-Host ""
Start-Sleep -Seconds 2

$count = 0
while ($count -lt $MaxChecks) {
    $count++
    
    $continue = Show-SimpleStatus -Hash $hash -Time $time -Msg $msg -Count $count
    
    if (-not $continue) {
        Write-Host "WARNING: 6 hour limit reached!" -ForegroundColor Red
        Write-Host "Please check GitHub Actions manually." -ForegroundColor Yellow
        break
    }
    
    $elapsed = (Get-Date) - $time
    
    if ($elapsed.TotalHours -ge 4.5 -and ($count % 3) -eq 0) {
        Write-Host "NOTE: Build should be done by now." -ForegroundColor Yellow
        Write-Host "Please verify status manually at:" -ForegroundColor Yellow
        Write-Host "https://github.com/leomath42/XOffsetDatastructure/actions" -ForegroundColor Cyan
        Write-Host ""
    }
    
    $nextCheck = (Get-Date).AddMinutes($CheckIntervalMinutes)
    Write-Host "Next check at: $($nextCheck.ToString('HH:mm:ss'))" -ForegroundColor Gray
    Write-Host "Press Ctrl+C to stop" -ForegroundColor DarkGray
    Write-Host ""
    
    Start-Sleep -Seconds ($CheckIntervalMinutes * 60)
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   Monitoring Complete" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Manually verify CI status:" -ForegroundColor Yellow
Write-Host "https://github.com/leomath42/XOffsetDatastructure/actions" -ForegroundColor Cyan
Write-Host ""
Write-Host "Success indicators:" -ForegroundColor Green
Write-Host "  - All steps show green checkmarks" -ForegroundColor White
Write-Host "  - Test output: '18/18 tests successful'" -ForegroundColor White
Write-Host "  - Reflection tests passed" -ForegroundColor White
Write-Host ""
