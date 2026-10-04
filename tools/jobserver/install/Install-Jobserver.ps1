[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$BuiltDaemonPath,
    [string]$InstallRoot,
    [Parameter(Mandatory)] [string]$RegisterScript,
    [Parameter(Mandatory)] [ValidateSet(0, 1)] [int]$AsanEnabled,
    [string]$LinkDirectory,
    [switch]$Force
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
    if ($Force) {
        $statusJson = & coj jobs status --json
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not read active tickets for forced installation. No tickets were cleared.'
        }
        $board = $statusJson | ConvertFrom-Json
        if ($board.type -ne 'status' -or $null -eq $board.tickets) {
            throw 'Invalid jobs-board status response. No tickets were cleared.'
        }

        $closedCount = 0
        foreach ($ticket in $board.tickets) {
            $clearedJson = & coj jobs clear $ticket.id --json
            if ($LASTEXITCODE -ne 0) {
                throw "Could not force-close ticket $($ticket.id). Already force-closed $closedCount tickets; installation stopped."
            }
            $cleared = $clearedJson | ConvertFrom-Json
            if ($cleared.type -ne 'cleared' -or $null -eq $cleared.tickets) {
                throw "Invalid response while force-closing ticket $($ticket.id); installation stopped."
            }
            foreach ($closed in $cleared.tickets) {
                Write-Host ("Force-closed ticket {0}: {1}, {2}, owner '{3}', {4}  [{5}]" -f
                    $closed.id, $closed.mode, $closed.state, $closed.owner, $closed.name, $closed.worktree)
                $closedCount++
            }
        }
        Write-Host "Force-closed $closedCount tickets. Their processes were not stopped."
    }

    & coj jobs shutdown
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not shut down the installed board. Clear active tickets or retry with coj install jobserver --force; new tickets may have arrived during installation. For a protocol upgrade, shut down the empty board with its matching client before replacing either component.'
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
