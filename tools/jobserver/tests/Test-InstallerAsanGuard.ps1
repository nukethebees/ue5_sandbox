$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$installer = Join-Path $PSScriptRoot '../install/Install-Jobserver.ps1'
$probe = @{ PathResolutionReached = $false }

# The first path lookup is before mutex creation or any installation mutation.
# Always stop there so even a regressed guard cannot reach the installed jobserver.
function Resolve-Path {
    param([string]$LiteralPath)
    $probe.PathResolutionReached = $true
    throw 'Installer probe reached path resolution'
}

foreach ($asan in @(0, 1)) {
    $probe.PathResolutionReached = $false
    $failure = $null
    try {
        & $installer -BuiltClientPath 'unused-client' -BuiltDaemonPath 'unused-daemon' `
            -InstallRoot 'unused-install' -RegisterScript 'unused-register' -AsanEnabled $asan
    } catch {
        $failure = $_.Exception.Message
    }

    if ($asan) {
        if ($probe.PathResolutionReached -or
            $failure -notlike '*ASAN jobserver build is for local validation*must not replace the installed machine jobserver*IOJ_ENABLE_ASAN=OFF*') {
            throw "ASAN installation was not rejected before path resolution: $failure"
        }
    } elseif (-not $probe.PathResolutionReached -or $failure -ne 'Installer probe reached path resolution') {
        throw "Normal installation did not pass the ASAN guard: $failure"
    }
}
Write-Host 'Normal installation passes the guard; ASAN installation is rejected before mutation.'
