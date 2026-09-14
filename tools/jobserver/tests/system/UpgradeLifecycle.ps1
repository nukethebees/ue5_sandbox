[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuildDirectory,

    [Parameter(Mandatory)]
    [string]$ClientPath,

    [Parameter(Mandatory)]
    [string]$BuiltClientPath,

    [Parameter(Mandatory)]
    [string]$BuiltDaemonPath
)

$ErrorActionPreference = 'Stop'
$buildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$clientPath = (Resolve-Path -LiteralPath $ClientPath).Path
$builtClientPath = (Resolve-Path -LiteralPath $BuiltClientPath).Path
$builtDaemonPath = (Resolve-Path -LiteralPath $BuiltDaemonPath).Path
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$run = [Guid]::NewGuid().ToString('N')
$binPath = Split-Path $clientPath
$installRoot = Split-Path $binPath
$backup = Join-Path $installRoot ".system-test-client-$run.exe"
$previousPath = Join-Path $installRoot 'previous'
$interruptedBinBackup = Join-Path $installRoot ".system-test-bin-$run"
$holderName = "upgrade-holder-$run"
$processes = @()

function Start-ProcessWithArguments {
    param([string]$Executable, [string[]]$Arguments)

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    foreach ($argument in $Arguments) {
        $startInfo.ArgumentList.Add($argument)
    }
    return [System.Diagnostics.Process]::Start($startInfo)
}

function Get-Status {
    return (& $clientPath status --json | ConvertFrom-Json)
}

function Wait-Daemon {
    param([bool]$Reachable, [int]$TimeoutSeconds = 20)

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if ($Reachable) {
            & $clientPath status --json *> $null
            if ($LASTEXITCODE -eq 0) {
                return
            }
        } elseif ($null -eq (Get-DaemonProcess)) {
            return
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for daemon reachable=$Reachable"
}

function Wait-Job {
    param([string]$Name, [string]$State)

    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        $job = (Get-Status).jobs | Where-Object { $_.name -eq $Name } | Select-Object -First 1
        if ($null -ne $job -and $job.state -eq $State) {
            return $job
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Name to enter $State"
}

function Get-DaemonProcess {
    return Get-Process jobserverd -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -eq $builtDaemonPath -or $_.Path -eq (Join-Path (Split-Path $clientPath) 'jobserverd.exe') } |
        Select-Object -First 1
}

if (@((Get-Status).jobs).Count -ne 0) {
    throw 'The canonical jobserver must be idle before the upgrade system test'
}
if (Test-Path -LiteralPath $backup) {
    throw "System-test backup already exists: $backup"
}

try {
    $daemonBefore = Get-DaemonProcess
    if ($null -eq $daemonBefore) {
        throw 'The installed daemon process was not found'
    }
    $clientHashBefore = (Get-FileHash -LiteralPath $clientPath -Algorithm SHA256).Hash

    $holder = Start-ProcessWithArguments $clientPath @(
        'run', '--name', $holderName, '--kind', 'test', '--shared', 'machine', '--',
        $cmake, '-E', 'sleep', '3'
    )
    $processes += $holder
    $null = Wait-Job $holderName 'RUNNING'

    & $cmake --build $buildDirectory --target install-jobserver
    if ($LASTEXITCODE -eq 0) {
        throw 'Installation unexpectedly succeeded while a job was active'
    }
    if ((Get-FileHash -LiteralPath $clientPath -Algorithm SHA256).Hash -ne $clientHashBefore) {
        throw 'A refused update changed the installed client'
    }
    $daemonAfterRefusal = Get-DaemonProcess
    if ($null -eq $daemonAfterRefusal -or $daemonAfterRefusal.Id -ne $daemonBefore.Id) {
        throw 'A refused update replaced or stopped the active daemon'
    }
    if (-not $holder.WaitForExit(15000) -or $holder.ExitCode -ne 0) {
        throw "Upgrade holder failed with exit code $($holder.ExitCode)"
    }

    $env:NUKETHEBEES_JOBSERVER_TEST_INSTALL_FAIL_AFTER_SWAP = '1'
    & $cmake --build $buildDirectory --target install-jobserver
    $rollbackExitCode = $LASTEXITCODE
    Remove-Item Env:NUKETHEBEES_JOBSERVER_TEST_INSTALL_FAIL_AFTER_SWAP
    if ($rollbackExitCode -eq 0) {
        throw 'Installation unexpectedly succeeded after the injected swap failure'
    }
    Wait-Daemon $true
    if ((Get-FileHash -LiteralPath $clientPath -Algorithm SHA256).Hash -ne $clientHashBefore) {
        throw 'Rollback did not restore the previous installed client'
    }

    & $cmake --build $buildDirectory --target install-jobserver
    if ($LASTEXITCODE -ne 0) {
        throw "Idle installation failed with exit code $LASTEXITCODE"
    }
    Wait-Daemon $true
    if ((Get-FileHash -LiteralPath $clientPath -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $builtClientPath -Algorithm SHA256).Hash) {
        throw 'Installed client does not match the built client after update'
    }

    & $clientPath shutdown
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not stop the daemon before interrupted-swap recovery testing'
    }
    Wait-Daemon $false
    if (-not (Test-Path -LiteralPath $previousPath -PathType Container)) {
        throw 'Successful installation did not retain the previous binaries'
    }
    Move-Item -LiteralPath $binPath -Destination $interruptedBinBackup

    & $cmake --build $buildDirectory --target install-jobserver
    if ($LASTEXITCODE -ne 0) {
        throw "Interrupted-swap recovery failed with exit code $LASTEXITCODE"
    }
    Wait-Daemon $true
    Remove-Item -LiteralPath $interruptedBinBackup -Recurse -Force

    & $clientPath shutdown
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not stop the daemon before the startup race'
    }
    Wait-Daemon $false
    $starters = @()
    for ($index = 0; $index -ne 8; ++$index) {
        $starters += Start-ProcessWithArguments $clientPath @('start')
    }
    foreach ($starter in $starters) {
        if (-not $starter.WaitForExit(20000) -or $starter.ExitCode -ne 0) {
            throw "A racing daemon starter failed with exit code $($starter.ExitCode)"
        }
    }
    Wait-Daemon $true
    $installedDaemon = Join-Path (Split-Path $clientPath) 'jobserverd.exe'
    $authorities = @(Get-Process jobserverd -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -eq $installedDaemon })
    if ($authorities.Count -ne 1) {
        throw "Expected one installed daemon authority, found $($authorities.Count)"
    }

    & $clientPath shutdown
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not stop the daemon before repair testing'
    }
    Wait-Daemon $false
    Move-Item -LiteralPath $clientPath -Destination $backup
    Remove-Item -LiteralPath $builtClientPath

    & $cmake --build $buildDirectory --target install-jobserver
    if ($LASTEXITCODE -ne 0) {
        throw "Repair installation failed with exit code $LASTEXITCODE"
    }
    Wait-Daemon $true
    if (-not (Test-Path -LiteralPath $clientPath -PathType Leaf)) {
        throw 'Repair installation did not restore the canonical client'
    }
    Remove-Item -LiteralPath $backup
} finally {
    Remove-Item Env:NUKETHEBEES_JOBSERVER_TEST_INSTALL_FAIL_AFTER_SWAP -ErrorAction SilentlyContinue
    foreach ($process in $processes) {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path -LiteralPath $backup) {
        if (Test-Path -LiteralPath $clientPath) {
            Remove-Item -LiteralPath $backup
        } else {
            Move-Item -LiteralPath $backup -Destination $clientPath
        }
    }
    if (Test-Path -LiteralPath $interruptedBinBackup) {
        if (Test-Path -LiteralPath $binPath) {
            Remove-Item -LiteralPath $interruptedBinBackup -Recurse -Force
        } else {
            Move-Item -LiteralPath $interruptedBinBackup -Destination $binPath
        }
    }
    & $clientPath status --json *> $null
    if ($LASTEXITCODE -ne 0) {
        & $clientPath start *> $null
    }
}
