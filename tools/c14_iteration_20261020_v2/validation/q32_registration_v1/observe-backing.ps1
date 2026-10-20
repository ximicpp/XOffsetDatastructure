param([Parameter(Mandatory=$true)][string]$Out,
      [string]$Distribution='Ubuntu-22.04')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$taskUtilityModule = Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1'
Import-Module -Name $taskUtilityModule -ErrorAction Stop
if (-not [IO.Path]::IsPathRooted($Out) -or (Test-Path -LiteralPath $Out)) { throw 'new absolute output required' }
$entries = @(Get-ChildItem -LiteralPath 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Lxss' |
    ForEach-Object { Get-ItemProperty -LiteralPath $_.PSPath } |
    Where-Object { $_.DistributionName -eq $Distribution })
if ($entries.Count -ne 1) { throw 'exact registered WSL distribution required' }
$base = [string]$entries[0].BasePath
$normalized = $base -replace '^\\\\\?\\',''
if (-not $normalized.StartsWith('C:\',[StringComparison]::OrdinalIgnoreCase)) { throw 'backing registry is not C:' }
$vhd = Join-Path $normalized 'ext4.vhdx'
if (-not (Test-Path -LiteralPath $vhd -PathType Leaf)) { throw 'registered VHD absent' }
$taskAncestor = $vhd
while ($taskAncestor) {
    $taskItem = Get-Item -LiteralPath $taskAncestor -Force
    if (($taskItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'backing VHD/ancestor is a reparse point; C: attribution not established'
    }
    $taskAncestor = [IO.Path]::GetDirectoryName($taskAncestor)
}
$drive = [IO.DriveInfo]::new('C:\')
$taskSha256 = [Security.Cryptography.SHA256]::Create()
try {
    $taskDigest = $taskSha256.ComputeHash([IO.File]::ReadAllBytes($PSCommandPath))
    $taskHelperSha256 = [BitConverter]::ToString($taskDigest).Replace('-','').ToLowerInvariant()
} finally {
    $taskSha256.Dispose()
}
$record = [ordered]@{captured_at=[DateTimeOffset]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ss.ffffffzzz',[Globalization.CultureInfo]::InvariantCulture);distribution=$Distribution;
    registered_base_path=$base;registered_vhd_path=$vhd;windows_backing_root='C:\';
    linux_backing_probe='/mnt/c';volume_size_bytes=$drive.TotalSize;
    volume_free_bytes=$drive.AvailableFreeSpace;read_only_inspection=$true;
    no_reparse_points_to_backing_root=$true;
    powershell_home=$PSHOME;utility_module_path=$taskUtilityModule;
    helper_sha256=$taskHelperSha256}
$bytes = [Text.UTF8Encoding]::new($false).GetBytes(($record|ConvertTo-Json -Depth 8)+"`n")
$stream = [IO.File]::Open($Out,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try { $stream.Write($bytes,0,$bytes.Length);$stream.Flush($true) } finally { $stream.Dispose() }
