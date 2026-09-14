[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuiltClientPath,

    [Parameter(Mandatory)]
    [string]$BuiltDaemonPath,

    [Parameter(Mandatory)]
    [string]$InstallRoot,

    [Parameter(Mandatory)]
    [string]$RegisterScript
)

$ErrorActionPreference = 'Stop'
$builtClient = (Resolve-Path -LiteralPath $BuiltClientPath).Path
$builtDaemon = (Resolve-Path -LiteralPath $BuiltDaemonPath).Path
$registerScript = (Resolve-Path -LiteralPath $RegisterScript).Path
$root = [IO.Path]::GetFullPath($InstallRoot)
$bin = Join-Path $root 'bin'
$previous = Join-Path $root 'previous'
$staging = Join-Path $root ".staging-$PID-$([Guid]::NewGuid().ToString('N'))"
$stagingBin = Join-Path $staging 'bin'
$installedClient = Join-Path $bin 'jobserver.exe'
$installedDaemon = Join-Path $bin 'jobserverd.exe'
$taskName = 'NukeTheBeesJobserver'
$swapped = $false
$userSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$mutexName = "Global\NukeTheBees.Jobserver.Install.$userSid"
$mutex = [Threading.Mutex]::new($false, $mutexName)
$ownsMutex = $false

function Wait-DaemonExit {
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        $running = Get-Process jobserverd -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $installedDaemon } |
            Select-Object -First 1
        if ($null -eq $running) {
            return
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'The canonical jobserver did not exit during installation.'
}

function Wait-DaemonReady {
    param([string]$ClientPath, [string]$DaemonPath)

    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        try {
            $status = & $ClientPath status --json 2> $null | ConvertFrom-Json
            if ($LASTEXITCODE -eq 0 -and $null -ne $status.daemon.process_id) {
                $process = Get-Process -Id $status.daemon.process_id -ErrorAction SilentlyContinue
                if ($null -ne $process -and $process.Path -eq $DaemonPath) {
                    return
                }
            }
        } catch {
            # The daemon may still be starting or may not own the pipe yet.
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'The expected installed jobserver did not become the responsive pipe authority.'
}

function Register-Daemon {
    param([string]$DaemonPath)

    & pwsh -NoProfile -File $registerScript -DaemonPath $DaemonPath
    if ($LASTEXITCODE -ne 0) {
        throw "Jobserver task registration failed with exit code $LASTEXITCODE."
    }
}

function Test-StagedBinaries {
    $client = Join-Path $stagingBin 'jobserver.exe'
    $daemon = Join-Path $stagingBin 'jobserverd.exe'
    $testPipe = "\\.\pipe\NukeTheBees.Jobserver.Install.$PID.$([Guid]::NewGuid().ToString('N'))"
    $testData = Join-Path $staging 'data'
    $previousPipe = $env:NUKETHEBEES_JOBSERVER_TEST_PIPE
    $previousData = $env:NUKETHEBEES_JOBSERVER_TEST_DATA
    $process = $null
    try {
        $env:NUKETHEBEES_JOBSERVER_TEST_PIPE = $testPipe
        $env:NUKETHEBEES_JOBSERVER_TEST_DATA = $testData
        $process = Start-Process -FilePath $daemon -PassThru -WindowStyle Hidden
        $deadline = [DateTime]::UtcNow.AddSeconds(10)
        do {
            & $client ping *> $null
            if ($LASTEXITCODE -eq 0) {
                & $client shutdown *> $null
                if ($LASTEXITCODE -ne 0 -or -not $process.WaitForExit(5000)) {
                    throw 'The staged jobserver did not shut down cleanly.'
                }
                return
            }
            if ($process.HasExited) {
                throw "The staged daemon exited with code $($process.ExitCode)."
            }
            Start-Sleep -Milliseconds 100
        } while ([DateTime]::UtcNow -lt $deadline)
        throw 'The staged client could not reach the staged daemon.'
    } finally {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
        $env:NUKETHEBEES_JOBSERVER_TEST_PIPE = $previousPipe
        $env:NUKETHEBEES_JOBSERVER_TEST_DATA = $previousData
    }
}

try {
    try {
        $ownsMutex = $mutex.WaitOne([TimeSpan]::FromSeconds(30))
    } catch [Threading.AbandonedMutexException] {
        $ownsMutex = $true
    }
    if (-not $ownsMutex) {
        throw 'Another jobserver installation is already in progress.'
    }

    New-Item -ItemType Directory -Path $root -Force | Out-Null
    Get-ChildItem -LiteralPath $root -Directory -Filter '.staging-*' | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName -Recurse -Force
    }
    if (-not (Test-Path -LiteralPath $bin) -and (Test-Path -LiteralPath $previous)) {
        Move-Item -LiteralPath $previous -Destination $bin
        Register-Daemon $installedDaemon
        Wait-DaemonReady $installedClient $installedDaemon
        Write-Warning 'Recovered the previous jobserver after an interrupted binary swap.'
    }

    New-Item -ItemType Directory -Path $stagingBin -Force | Out-Null
    Copy-Item -LiteralPath $builtClient -Destination (Join-Path $stagingBin 'jobserver.exe')
    Copy-Item -LiteralPath $builtDaemon -Destination (Join-Path $stagingBin 'jobserverd.exe')
    & (Join-Path $stagingBin 'jobserver.exe') version *> $null
    if ($LASTEXITCODE -ne 0) {
        throw 'The staged jobserver client failed validation.'
    }
    Test-StagedBinaries

    if (Test-Path -LiteralPath $installedClient -PathType Leaf) {
        & $installedClient shutdown
        if ($LASTEXITCODE -ne 0) {
            throw 'The running jobserver could not be drained for update.'
        }
        Wait-DaemonExit
    } elseif (Test-Path -LiteralPath $installedDaemon -PathType Leaf) {
        $running = Get-Process jobserverd -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $installedDaemon } |
            Select-Object -First 1
        if ($null -ne $running) {
            throw 'The installed daemon is running but its matching client is unavailable.'
        }
    }

    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $previous) {
        Remove-Item -LiteralPath $previous -Recurse -Force
    }
    if (Test-Path -LiteralPath $bin) {
        Move-Item -LiteralPath $bin -Destination $previous
    }
    Move-Item -LiteralPath $stagingBin -Destination $bin
    $swapped = $true

    if ($env:NUKETHEBEES_JOBSERVER_TEST_INSTALL_FAIL_AFTER_SWAP -eq '1') {
        throw 'Injected jobserver installation failure after binary swap.'
    }

    Register-Daemon $installedDaemon
    Wait-DaemonReady $installedClient $installedDaemon
    Write-Host "Installed and validated the canonical per-user jobserver at '$bin'."
} catch {
    $failure = $_
    if ($swapped) {
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
        $replacement = Get-Process jobserverd -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $installedDaemon } |
            Select-Object -First 1
        if ($null -ne $replacement) {
            $authorityPid = $null
            if (Test-Path -LiteralPath $installedClient -PathType Leaf) {
                try {
                    $status = & $installedClient status --json 2> $null | ConvertFrom-Json
                    if ($LASTEXITCODE -eq 0) {
                        $authorityPid = $status.daemon.process_id
                    }
                } catch {
                    $authorityPid = $null
                }
            }
            if ($authorityPid -eq $replacement.Id) {
                & $installedClient shutdown *> $null
                $null = $replacement.WaitForExit(5000)
            }
            if (-not $replacement.HasExited) {
                $liveReplacement = Get-Process -Id $replacement.Id -ErrorAction SilentlyContinue
                if ($null -ne $liveReplacement -and $liveReplacement.Path -eq $installedDaemon) {
                    Stop-Process -Id $liveReplacement.Id -Force
                    if (-not $liveReplacement.WaitForExit(5000)) {
                        throw 'The failed replacement daemon could not be terminated for rollback.'
                    }
                }
            }
        }
        if (Test-Path -LiteralPath $bin) {
            Remove-Item -LiteralPath $bin -Recurse -Force
        }
        if (Test-Path -LiteralPath $previous) {
            Move-Item -LiteralPath $previous -Destination $bin
            Register-Daemon $installedDaemon
            Wait-DaemonReady $installedClient $installedDaemon
            Write-Warning 'The failed jobserver update was rolled back to the previous binaries.'
        }
    }
    throw $failure
} finally {
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    if ($ownsMutex) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
