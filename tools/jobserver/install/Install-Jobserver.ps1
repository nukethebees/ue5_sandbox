[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$BuiltDaemonPath,
    [string]$InstallRoot,
    [Parameter(Mandatory)] [string]$RegisterScript,
    [Parameter(Mandatory)] [ValidateSet(0, 1)] [int]$AsanEnabled,
    [string]$LinkDirectory
)

$ErrorActionPreference = 'Stop'
if ($AsanEnabled) {
    throw 'An ASAN jobserver build is for local validation and must not replace the installed machine jobserver. Use a build configured with IOJ_ENABLE_ASAN=OFF for canonical installation.'
}

$builtDaemon = (Resolve-Path -LiteralPath $BuiltDaemonPath).Path
$registerScript = (Resolve-Path -LiteralPath $RegisterScript).Path
. "$PSScriptRoot/../../install/ToolLinks.ps1"
if (-not $InstallRoot) { $InstallRoot = Join-Path (Get-IojRoot) 'tools/jobserver' }
$links = Get-ToolLinkDirectory $InstallRoot $LinkDirectory
Assert-ToolLinkSupport $links
$bin = Join-Path ([IO.Path]::GetFullPath($InstallRoot)) 'bin'
$installedDaemon = Join-Path $bin 'jobserverd.exe'

# Wait only for the daemon being replaced, never for jobs on the board.
$registeredTask = Get-ScheduledTask -TaskName 'NukeTheBeesJobserver' -ErrorAction SilentlyContinue
$registeredPaths = @()
if ($registeredTask) { $registeredPaths = @($registeredTask.Actions.Execute) }
$running = Get-Process jobserverd -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq $installedDaemon -or $_.Path -in $registeredPaths }
if ($running) {
    & coj jobs shutdown
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not shut down the installed board. Clear active tickets first. For a protocol upgrade, shut down the empty board with its matching client before replacing either component.'
    }
    foreach ($daemon in $running) {
        if (-not $daemon.WaitForExit(15000)) {
            throw 'The installed jobserver did not exit.'
        }
    }
}

New-Item -ItemType Directory -Path $bin -Force | Out-Null
Copy-Item -LiteralPath $builtDaemon -Destination $installedDaemon -Force
& pwsh -NoProfile -File $registerScript -DaemonPath $installedDaemon
if ($LASTEXITCODE -ne 0) {
    throw "Jobserver task registration failed with exit code $LASTEXITCODE."
}

$deadline = [DateTime]::UtcNow.AddSeconds(20)
do {
    & coj jobs ping
    if ($LASTEXITCODE -eq 0) {
        Publish-ToolLinks $bin @('jobserverd.exe') $links
        Write-Host "Installed the per-user jobs board at '$bin'."
        return
    }
    Start-Sleep -Milliseconds 100
} while ([DateTime]::UtcNow -lt $deadline)
throw 'The installed jobserver did not become responsive.'
