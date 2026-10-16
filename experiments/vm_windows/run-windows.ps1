param(
    [Parameter(Mandatory = $true)][string]$OutDirectory,
    [ValidateSet("NMake Makefiles", "Ninja")][string]$Generator = "NMake Makefiles",
    [ValidateRange(1, 256)][int]$Jobs = 1
)
Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

# Windows/Python are the bootstrap prerequisites. The driver records and checks
# CMake, CTest, cl/link, generator tools and the x64 MSVC developer environment
# before configuring anything. Each failed run retains result.json and logs.
try {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw "Run this handoff in native Windows x64 Developer PowerShell."
    }
    $Python = Get-Command python -CommandType Application -ErrorAction Stop
    & $Python.Source -c "import sys; sys.exit(0 if sys.version_info >= (3, 9) else 1)"
    if ($LASTEXITCODE -ne 0) { throw "Python 3.9 or newer is required." }
    $env:PYTHONDONTWRITEBYTECODE = "1"
    & $Python.Source (Join-Path $PSScriptRoot "validate_windows.py") --bundle $PSScriptRoot --out $OutDirectory --generator $Generator --jobs $Jobs
    exit $LASTEXITCODE
} catch {
    Write-Error $_
    exit 1
}
