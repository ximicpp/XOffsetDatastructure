# CI Monitor - PowerShell Wrapper
# Usage: 
#   .\scripts\Start-CIMonitor.ps1           # Start monitoring
#   .\scripts\Start-CIMonitor.ps1 -ShowLog  # Show current log
#   .\scripts\Start-CIMonitor.ps1 -Stop     # Stop monitoring

param(
    [switch]$ShowLog,
    [switch]$Stop,
    [string]$Branch = "next_cpp26"
)

$ScriptPath = "$PSScriptRoot\monitor-ci.sh"
$LogFile = "ci-monitor.log"
$PidFile = ".ci-monitor.pid"

if ($ShowLog) {
    Write-Host "📋 Current CI Monitor Log:" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    if (Test-Path $LogFile) {
        Get-Content $LogFile -Tail 50
    } else {
        Write-Host "⚠️  Log file not found. Monitor may not be running." -ForegroundColor Yellow
    }
    exit
}

if ($Stop) {
    Write-Host "🛑 Stopping CI Monitor..." -ForegroundColor Yellow
    wsl bash -c "pkill -f 'monitor-ci.sh' && echo 'Monitor stopped' || echo 'No monitor process found'"
    if (Test-Path $PidFile) {
        Remove-Item $PidFile
    }
    exit
}

# Start monitoring
Write-Host "🚀 Starting CI Monitor for branch: $Branch" -ForegroundColor Green
Write-Host "📝 Log file: $LogFile" -ForegroundColor Cyan
Write-Host ""

# Kill any existing monitor
wsl bash -c "pkill -f 'monitor-ci.sh'" 2>$null

# Start new monitor
$job = Start-Job -ScriptBlock {
    param($branch, $logFile)
    wsl bash -c "cd /mnt/g/workspace/XOffsetDatastructure && ./scripts/monitor-ci.sh '$branch' '$logFile'"
} -ArgumentList $Branch, $LogFile

Write-Host "✓ Monitor started (Job ID: $($job.Id))" -ForegroundColor Green
$job.Id | Out-File $PidFile

Write-Host ""
Write-Host "Commands:" -ForegroundColor Cyan
Write-Host "  - View log:  .\scripts\Start-CIMonitor.ps1 -ShowLog"
Write-Host "  - Stop:      .\scripts\Start-CIMonitor.ps1 -Stop"
Write-Host ""

# Show initial log content
Start-Sleep -Seconds 3
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "📋 Initial Log Output:" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
if (Test-Path $LogFile) {
    Get-Content $LogFile
}

Write-Host ""
Write-Host "✓ Monitor is running in background" -ForegroundColor Green
Write-Host "  Use -ShowLog to check progress" -ForegroundColor Gray
