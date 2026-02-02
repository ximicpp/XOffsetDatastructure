# Auto-monitor CI build progress
# Usage: .\scripts\auto-monitor-ci.ps1

param(
    [int]$CheckIntervalMinutes = 5,
    [int]$MaxChecks = 60  # 5 hours max
)

$Owner = "ximicpp"
$Repo = "XOffsetDatastructure"
$Branch = "next_cpp26"

function Get-CIStatus {
    try {
        $apiUrl = "https://api.github.com/repos/$Owner/$Repo/actions/runs?per_page=1&branch=$Branch"
        $response = Invoke-RestMethod -Uri $apiUrl -Method Get -Headers @{
            "Accept" = "application/vnd.github.v3+json"
            "User-Agent" = "PowerShell-CI-Monitor"
        } -ErrorAction Stop
        
        $run = $response.workflow_runs[0]
        
        # Get job details
        $jobsUrl = "https://api.github.com/repos/$Owner/$Repo/actions/runs/$($run.id)/jobs"
        $jobsResponse = Invoke-RestMethod -Uri $jobsUrl -Method Get -Headers @{
            "Accept" = "application/vnd.github.v3+json"
            "User-Agent" = "PowerShell-CI-Monitor"
        } -ErrorAction Stop
        
        return @{
            Run = $run
            Jobs = $jobsResponse.jobs
            Success = $true
        }
    } catch {
        return @{
            Success = $false
            Error = $_.Exception.Message
        }
    }
}

function Show-Status {
    param($Data, $CheckNum)
    
    Clear-Host
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "   CI Build Monitor - Check #$CheckNum" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
    Write-Host ""
    
    if (-not $Data.Success) {
        Write-Host "ERROR: Cannot fetch CI status" -ForegroundColor Red
        Write-Host "Message: $($Data.Error)" -ForegroundColor Red
        return $false
    }
    
    $run = $Data.Run
    $job = $Data.Jobs[0]
    
    # Calculate elapsed time
    $created = [DateTime]::Parse($run.created_at)
    $elapsed = (Get-Date) - $created
    $hours = [math]::Floor($elapsed.TotalHours)
    $mins = $elapsed.Minutes
    
    Write-Host "Repository: $Owner/$Repo" -ForegroundColor White
    Write-Host "Branch: $Branch" -ForegroundColor White
    Write-Host "Commit: $($run.head_sha.Substring(0,7))" -ForegroundColor White
    Write-Host ""
    
    Write-Host "Status: " -NoNewline
    if ($run.status -eq "completed") {
        if ($run.conclusion -eq "success") {
            Write-Host "COMPLETED - SUCCESS" -ForegroundColor Green
        } else {
            Write-Host "COMPLETED - $($run.conclusion.ToUpper())" -ForegroundColor Red
        }
    } elseif ($run.status -eq "in_progress") {
        Write-Host "IN PROGRESS" -ForegroundColor Yellow
    } else {
        Write-Host $run.status.ToUpper() -ForegroundColor Gray
    }
    
    Write-Host "Elapsed: ${hours}h ${mins}m" -ForegroundColor Magenta
    Write-Host ""
    
    # Progress estimation
    if ($run.status -eq "in_progress") {
        $progress = [math]::Min(100, [math]::Floor(($elapsed.TotalHours / 3.5) * 100))
        $barLength = 40
        $filled = [math]::Floor($barLength * $progress / 100)
        $empty = $barLength - $filled
        $bar = "=" * $filled + "-" * $empty
        Write-Host "Progress: [$bar] ~$progress%" -ForegroundColor Cyan
        Write-Host ""
    }
    
    # Show steps
    Write-Host "Build Steps:" -ForegroundColor Yellow
    if ($job.steps) {
        foreach ($step in $job.steps) {
            $icon = switch ($step.status) {
                "completed" { 
                    if ($step.conclusion -eq "success") { "[OK]" } 
                    else { "[FAIL]" } 
                }
                "in_progress" { "[...]" }
                default { "[ ]" }
            }
            $color = switch ($step.status) {
                "completed" { 
                    if ($step.conclusion -eq "success") { "Green" } 
                    else { "Red" } 
                }
                "in_progress" { "Yellow" }
                default { "Gray" }
            }
            
            $stepName = $step.name
            if ($step.status -eq "in_progress") {
                $stepStart = [DateTime]::Parse($step.started_at)
                $stepElapsed = (Get-Date) - $stepStart
                $stepMins = [math]::Floor($stepElapsed.TotalMinutes)
                $stepName += " (running ${stepMins}m)"
            }
            
            Write-Host "  $icon $stepName" -ForegroundColor $color
        }
    } else {
        Write-Host "  No step information available" -ForegroundColor Gray
    }
    Write-Host ""
    
    Write-Host "URL: https://github.com/$Owner/$Repo/actions/runs/$($run.id)" -ForegroundColor Blue
    Write-Host ""
    
    # Return true if still running, false if completed
    return ($run.status -ne "completed")
}

# Main loop
Write-Host "Starting automatic CI monitor..." -ForegroundColor Green
Write-Host "Checking every $CheckIntervalMinutes minutes" -ForegroundColor Gray
Write-Host "Press Ctrl+C to stop" -ForegroundColor Gray
Write-Host ""
Start-Sleep -Seconds 2

$checkCount = 0
$stillRunning = $true

while ($checkCount -lt $MaxChecks -and $stillRunning) {
    $checkCount++
    
    $data = Get-CIStatus
    $stillRunning = Show-Status -Data $data -CheckNum $checkCount
    
    if (-not $stillRunning) {
        # Build completed
        if ($data.Run.conclusion -eq "success") {
            Write-Host "========================================" -ForegroundColor Green
            Write-Host "   BUILD SUCCESSFUL!" -ForegroundColor Green
            Write-Host "========================================" -ForegroundColor Green
            Write-Host ""
            Write-Host "All tests passed!" -ForegroundColor Green
            Write-Host ""
        } else {
            Write-Host "========================================" -ForegroundColor Red
            Write-Host "   BUILD FAILED" -ForegroundColor Red
            Write-Host "========================================" -ForegroundColor Red
            Write-Host ""
            Write-Host "Conclusion: $($data.Run.conclusion)" -ForegroundColor Red
            Write-Host "Please check logs at:" -ForegroundColor Yellow
            Write-Host "https://github.com/$Owner/$Repo/actions/runs/$($data.Run.id)" -ForegroundColor Blue
            Write-Host ""
        }
        break
    }
    
    # Wait before next check
    $nextCheck = (Get-Date).AddMinutes($CheckIntervalMinutes)
    Write-Host "Next check at: $($nextCheck.ToString('HH:mm:ss'))" -ForegroundColor Gray
    Write-Host "Waiting $CheckIntervalMinutes minutes..." -ForegroundColor Gray
    Write-Host ""
    
    Start-Sleep -Seconds ($CheckIntervalMinutes * 60)
}

if ($checkCount -ge $MaxChecks) {
    Write-Host "Reached maximum check limit ($MaxChecks checks)" -ForegroundColor Yellow
    Write-Host "Please check manually if build is still running." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Monitoring stopped." -ForegroundColor Cyan
Write-Host ""
