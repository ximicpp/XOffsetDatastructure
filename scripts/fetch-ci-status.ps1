# Fetch CI Status from GitHub API (No Auth Required for Public Repos)
# Usage: .\scripts\fetch-ci-status.ps1

param(
    [string]$Owner = "ximicpp",
    [string]$Repo = "XOffsetDatastructure",
    [string]$Branch = "next_cpp26"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   Fetching CI Status from GitHub API" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Get local commit SHA
$gitLog = git log --format="%H" -1 origin/$Branch 2>$null
if (-not $gitLog) {
    Write-Host "ERROR: Cannot get commit SHA" -ForegroundColor Red
    exit 1
}
$commitSHA = $gitLog.Trim()

Write-Host "Repository: $Owner/$Repo" -ForegroundColor White
Write-Host "Branch: $Branch" -ForegroundColor White
Write-Host "Commit: $($commitSHA.Substring(0,7))" -ForegroundColor White
Write-Host ""

# Try to fetch workflow runs
Write-Host "Fetching latest workflow runs..." -ForegroundColor Yellow
$apiUrl = "https://api.github.com/repos/$Owner/$Repo/actions/runs?per_page=5&branch=$Branch"

try {
    $response = Invoke-RestMethod -Uri $apiUrl -Method Get -Headers @{
        "Accept" = "application/vnd.github.v3+json"
        "User-Agent" = "PowerShell-CI-Monitor"
    } -ErrorAction Stop
    
    Write-Host "SUCCESS: Got API response" -ForegroundColor Green
    Write-Host ""
    
    if ($response.total_count -eq 0) {
        Write-Host "No workflow runs found for branch '$Branch'" -ForegroundColor Yellow
        exit 0
    }
    
    Write-Host "Found $($response.total_count) workflow run(s)" -ForegroundColor Cyan
    Write-Host ""
    
    # Display each run
    $runNumber = 1
    foreach ($run in $response.workflow_runs) {
        Write-Host "--- Run #$runNumber ---" -ForegroundColor Cyan
        Write-Host "  ID: $($run.id)" -ForegroundColor White
        Write-Host "  Name: $($run.name)" -ForegroundColor White
        Write-Host "  Status: $($run.status)" -ForegroundColor $(
            if ($run.status -eq "completed") { "Green" }
            elseif ($run.status -eq "in_progress") { "Yellow" }
            else { "Gray" }
        )
        
        if ($run.conclusion) {
            Write-Host "  Conclusion: $($run.conclusion)" -ForegroundColor $(
                if ($run.conclusion -eq "success") { "Green" }
                elseif ($run.conclusion -eq "failure") { "Red" }
                else { "Yellow" }
            )
        }
        
        Write-Host "  Created: $($run.created_at)" -ForegroundColor Gray
        Write-Host "  Updated: $($run.updated_at)" -ForegroundColor Gray
        Write-Host "  Commit: $($run.head_sha.Substring(0,7))" -ForegroundColor White
        Write-Host "  URL: $($run.html_url)" -ForegroundColor Blue
        
        # Check if this is our commit
        if ($run.head_sha -eq $commitSHA) {
            Write-Host "  >>> THIS IS THE CURRENT COMMIT <<<" -ForegroundColor Magenta
            
            # Calculate runtime
            $created = [DateTime]::Parse($run.created_at)
            $now = Get-Date
            $elapsed = $now - $created
            Write-Host "  Runtime: $([math]::Floor($elapsed.TotalHours))h $($elapsed.Minutes)m" -ForegroundColor Magenta
        }
        
        Write-Host ""
        $runNumber++
    }
    
    # Find the run for our commit
    $ourRun = $response.workflow_runs | Where-Object { $_.head_sha -eq $commitSHA } | Select-Object -First 1
    
    if ($ourRun) {
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "   CURRENT COMMIT STATUS" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "Status: $($ourRun.status)" -ForegroundColor $(
            if ($ourRun.status -eq "completed") { "Green" }
            elseif ($ourRun.status -eq "in_progress") { "Yellow" }
            else { "Gray" }
        )
        
        if ($ourRun.conclusion) {
            Write-Host "Result: $($ourRun.conclusion)" -ForegroundColor $(
                if ($ourRun.conclusion -eq "success") { "Green" }
                elseif ($ourRun.conclusion -eq "failure") { "Red" }
                else { "Yellow" }
            )
        } else {
            Write-Host "Result: Still running..." -ForegroundColor Yellow
        }
        
        $created = [DateTime]::Parse($ourRun.created_at)
        $elapsed = (Get-Date) - $created
        Write-Host "Running for: $([math]::Floor($elapsed.TotalHours))h $($elapsed.Minutes)m" -ForegroundColor Magenta
        Write-Host ""
        Write-Host "View details: $($ourRun.html_url)" -ForegroundColor Blue
        Write-Host ""
        
        # Get job details
        Write-Host "Fetching job details..." -ForegroundColor Yellow
        $jobsUrl = "https://api.github.com/repos/$Owner/$Repo/actions/runs/$($ourRun.id)/jobs"
        
        try {
            $jobsResponse = Invoke-RestMethod -Uri $jobsUrl -Method Get -Headers @{
                "Accept" = "application/vnd.github.v3+json"
                "User-Agent" = "PowerShell-CI-Monitor"
            } -ErrorAction Stop
            
            Write-Host ""
            Write-Host "--- Job Details ---" -ForegroundColor Cyan
            foreach ($job in $jobsResponse.jobs) {
                Write-Host "  Job: $($job.name)" -ForegroundColor White
                Write-Host "    Status: $($job.status)" -ForegroundColor $(
                    if ($job.status -eq "completed") { "Green" }
                    elseif ($job.status -eq "in_progress") { "Yellow" }
                    else { "Gray" }
                )
                
                if ($job.conclusion) {
                    Write-Host "    Conclusion: $($job.conclusion)" -ForegroundColor $(
                        if ($job.conclusion -eq "success") { "Green" }
                        elseif ($job.conclusion -eq "failure") { "Red" }
                        else { "Yellow" }
                    )
                }
                
                if ($job.started_at) {
                    $jobStart = [DateTime]::Parse($job.started_at)
                    $jobElapsed = (Get-Date) - $jobStart
                    Write-Host "    Running: $([math]::Floor($jobElapsed.TotalHours))h $($jobElapsed.Minutes)m" -ForegroundColor Magenta
                }
                
                # Show steps if job is in progress or completed
                if ($job.steps) {
                    Write-Host "    Steps:" -ForegroundColor Gray
                    foreach ($step in $job.steps) {
                        $stepStatus = switch ($step.status) {
                            "completed" { if ($step.conclusion -eq "success") { "[OK]" } else { "[FAIL]" } }
                            "in_progress" { "[...]" }
                            default { "[ ]" }
                        }
                        $stepColor = switch ($step.status) {
                            "completed" { if ($step.conclusion -eq "success") { "Green" } else { "Red" } }
                            "in_progress" { "Yellow" }
                            default { "Gray" }
                        }
                        Write-Host "      $stepStatus $($step.name)" -ForegroundColor $stepColor
                    }
                }
                Write-Host ""
            }
        } catch {
            Write-Host "Could not fetch job details: $($_.Exception.Message)" -ForegroundColor Red
        }
        
    } else {
        Write-Host "WARNING: No workflow run found for commit $($commitSHA.Substring(0,7))" -ForegroundColor Yellow
    }
    
} catch {
    Write-Host "ERROR: Failed to fetch from GitHub API" -ForegroundColor Red
    Write-Host "Message: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    
    if ($_.Exception.Message -match "403") {
        Write-Host "This might be a private repository requiring authentication." -ForegroundColor Yellow
    } elseif ($_.Exception.Message -match "404") {
        Write-Host "Repository not found or no workflow runs available." -ForegroundColor Yellow
    }
    
    Write-Host ""
    Write-Host "Please check manually at:" -ForegroundColor Cyan
    Write-Host "https://github.com/$Owner/$Repo/actions" -ForegroundColor White
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
