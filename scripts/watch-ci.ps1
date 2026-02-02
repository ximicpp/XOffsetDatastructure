# ============================================================================
# CI Watch Script - Continuous Monitoring
# ============================================================================

$ErrorActionPreference = "SilentlyContinue"

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  CI Continuous Monitor" -ForegroundColor Cyan
Write-Host "  Press Ctrl+C to stop" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

$lastStatus = ""
$checkCount = 0

while ($true) {
    $checkCount++
    
    Clear-Host
    
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  CI Monitor (Check #$checkCount)" -ForegroundColor Cyan
    Write-Host "  $(Get-Date -Format 'HH:mm:ss')" -ForegroundColor Cyan
    Write-Host "========================================`n" -ForegroundColor Cyan
    
    try {
        $response = Invoke-RestMethod -Uri "https://api.github.com/repos/ximicpp/XOffsetDatastructure/actions/runs/21581552096" -Headers @{
            'Accept' = 'application/vnd.github.v3+json'
            'User-Agent' = 'PowerShell'
        }
        
        $status = $response.status
        $conclusion = $response.conclusion
        $createdAt = [DateTime]::Parse($response.created_at)
        $duration = (Get-Date) - $createdAt
        
        Write-Host "Status: " -NoNewline
        if ($status -eq "completed") {
            if ($conclusion -eq "success") {
                Write-Host "✓ COMPLETED - SUCCESS" -ForegroundColor Green
            } else {
                Write-Host "✗ COMPLETED - FAILED" -ForegroundColor Red
            }
        } else {
            Write-Host "⏳ IN PROGRESS" -ForegroundColor Yellow
        }
        
        Write-Host "Duration: $([int]$duration.TotalMinutes)m $($duration.Seconds)s" -ForegroundColor Cyan
        Write-Host ""
        
        # Get job details
        $jobsUrl = "https://api.github.com/repos/ximicpp/XOffsetDatastructure/actions/runs/21581552096/jobs"
        $jobsResponse = Invoke-RestMethod -Uri $jobsUrl -Headers @{
            'Accept' = 'application/vnd.github.v3+json'
            'User-Agent' = 'PowerShell'
        }
        
        if ($jobsResponse.jobs.Count -gt 0) {
            $job = $jobsResponse.jobs[0]
            Write-Host "Job Steps:" -ForegroundColor Yellow
            
            foreach ($step in $job.steps) {
                $stepName = $step.name
                $stepStatus = $step.status
                $stepConclusion = $step.conclusion
                
                $icon = switch ($stepStatus) {
                    "completed" {
                        if ($stepConclusion -eq "success") { "✓" } else { "✗" }
                    }
                    "in_progress" { "⏳" }
                    default { "⏹" }
                }
                
                $color = switch ($stepStatus) {
                    "completed" {
                        if ($stepConclusion -eq "success") { "Green" } else { "Red" }
                    }
                    "in_progress" { "Yellow" }
                    default { "Gray" }
                }
                
                Write-Host "  $icon $stepName" -ForegroundColor $color
            }
        }
        
        Write-Host ""
        
        # Status change notification
        if ($status -ne $lastStatus -and $lastStatus -ne "") {
            Write-Host ">>> STATUS CHANGED: $lastStatus -> $status <<<" -ForegroundColor Magenta
            [Console]::Beep(1000, 500)
        }
        $lastStatus = $status
        
        # Exit if completed
        if ($status -eq "completed") {
            Write-Host "`n========================================" -ForegroundColor Cyan
            if ($conclusion -eq "success") {
                Write-Host "  ✓ CI COMPLETED SUCCESSFULLY!" -ForegroundColor Green
                Write-Host "========================================`n" -ForegroundColor Cyan
                Write-Host "Next steps:" -ForegroundColor Yellow
                Write-Host "  1. Run: bash scripts/complete-proposal.sh" -ForegroundColor Cyan
                Write-Host "  2. Archive: openspec archive add-ci-cd-docker-support --yes" -ForegroundColor Cyan
                Write-Host "  3. Push: git push origin next_cpp26`n" -ForegroundColor Cyan
            } else {
                Write-Host "  ✗ CI FAILED" -ForegroundColor Red
                Write-Host "========================================`n" -ForegroundColor Cyan
                Write-Host "Check logs at:" -ForegroundColor Yellow
                Write-Host "  https://github.com/ximicpp/XOffsetDatastructure/actions/runs/21581552096`n" -ForegroundColor Cyan
            }
            break
        }
        
        # Estimate completion
        $totalMinutes = 120  # 2 hours total
        $elapsedMinutes = [int]$duration.TotalMinutes
        $remainingMinutes = $totalMinutes - $elapsedMinutes
        
        if ($remainingMinutes -gt 0) {
            $eta = (Get-Date).AddMinutes($remainingMinutes).ToString("HH:mm")
            Write-Host "Estimated completion: $eta (~$remainingMinutes minutes)" -ForegroundColor Cyan
        }
        
    } catch {
        Write-Host "Error fetching CI status: $($_.Exception.Message)" -ForegroundColor Red
    }
    
    Write-Host "`nNext check in 60 seconds... (Ctrl+C to stop)" -ForegroundColor Gray
    Start-Sleep -Seconds 60
}
