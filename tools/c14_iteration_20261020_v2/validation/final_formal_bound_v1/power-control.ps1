# Payload parameters are supplied as task-prefixed variables by the Linux caller.
$ErrorActionPreference = 'Stop'
function Write-TaskJson([string]$taskName, $taskValue) {
    $taskPath = Join-Path $taskStateDir $taskName
    $taskTemporary = $taskPath + '.publishing'
    $taskStream = [IO.File]::Open($taskTemporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    try {
        $taskWriter = [IO.StreamWriter]::new($taskStream, [Text.UTF8Encoding]::new($false))
        $taskWriter.WriteLine(($taskValue | ConvertTo-Json -Depth 8))
        $taskWriter.Flush()
    } finally { $taskStream.Dispose() }
    # Publish only after the JSON is complete. Move refuses an existing target.
    [IO.File]::Move($taskTemporary, $taskPath)
}
function Read-TaskPower {
    $taskText = ((& powercfg.exe /getactivescheme) | Out-String)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read active power scheme' }
    $taskMatch = [regex]::Match($taskText, '[0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}')
    if (-not $taskMatch.Success) { throw 'Power scheme GUID missing' }
    return $taskMatch.Value.ToLowerInvariant()
}
$taskHigh = '8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c'
$taskOriginal = $null
$taskRequested = $false
$taskErrorText = $null
$taskRestored = $false
$taskReason = 'setup_failure'
$taskStarted = [DateTimeOffset]::UtcNow
try {
    $taskOriginal = Read-TaskPower
    $taskRequested = $true
    & powercfg.exe /setactive $taskHigh
    if ($LASTEXITCODE -ne 0) { throw 'Cannot select existing high-performance power scheme' }
    if ((Read-TaskPower) -ne $taskHigh) { throw 'Power scheme readback differs' }
    Write-TaskJson 'ready.json' ([ordered]@{status='ready'; original=$taskOriginal; active=$taskHigh;
        pid=$PID; started_utc=$taskStarted.ToString('o'); maximum_seconds=$taskMaximumSeconds;
        scope='Temporary existing scheme selection; no scheme settings, WSL configuration or services changed.'})
    $taskStop = Join-Path $taskStateDir 'stop.json'
    while (-not (Test-Path -LiteralPath $taskStop)) {
        if (([DateTimeOffset]::UtcNow - $taskStarted).TotalSeconds -ge $taskMaximumSeconds) {
            $taskReason = 'watchdog_deadline'; break
        }
        Start-Sleep -Milliseconds 500
    }
    if (Test-Path -LiteralPath $taskStop) { $taskReason = 'requested_stop' }
} catch {
    $taskErrorText = $_.Exception.Message
    if (-not (Test-Path -LiteralPath (Join-Path $taskStateDir 'ready.json'))) {
        Write-TaskJson 'ready.json' ([ordered]@{status='failed'; error=$taskErrorText; original=$taskOriginal; pid=$PID})
    }
} finally {
    if ($taskRequested -and $taskOriginal) {
        try {
            $taskCurrent = Read-TaskPower
            if ($taskCurrent -eq $taskHigh) {
                & powercfg.exe /setactive $taskOriginal
                if ($LASTEXITCODE -ne 0) { throw 'Cannot restore original power scheme' }
                $taskRestored = (Read-TaskPower) -eq $taskOriginal
            } elseif ($taskCurrent -eq $taskOriginal) {
                $taskRestored = $true
            } else {
                throw 'External power-scheme change observed; refusing to overwrite it'
            }
        } catch { $taskErrorText = $taskErrorText + '; restore: ' + $_.Exception.Message }
    }
    $taskStatus = if ($taskRestored -and -not $taskErrorText -and $taskReason -eq 'requested_stop') {'passed'} else {'failed'}
    Write-TaskJson 'completed.json' ([ordered]@{status=$taskStatus; original=$taskOriginal; restored=$taskRestored;
        reason=$taskReason; error=$taskErrorText; pid=$PID; finished_utc=[DateTimeOffset]::UtcNow.ToString('o')})
}
