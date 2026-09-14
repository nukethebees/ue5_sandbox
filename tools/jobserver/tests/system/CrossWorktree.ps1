[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$SourceRoot,

    [Parameter(Mandatory)]
    [string]$ClientPath
)

$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$clientPath = (Resolve-Path -LiteralPath $ClientPath).Path
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$run = [Guid]::NewGuid().ToString('N')
$testRoot = Join-Path $sourceRoot ".local\jobserver-system\$run"
$secondary = Join-Path $testRoot 'worktree two ☃'
$buildDirectory = Join-Path $secondary '.local\fixture build'
$results = Join-Path $sourceRoot ".local\handoffs\jobserver-system-$run"
$holderName = "system-holder-$run"
$exclusiveName = "system-exclusive-$run"
$lateName = "system-late-$run"
$worktreeAdded = $false
$processes = @()

function Invoke-Checked {
    param(
        [string]$Executable,
        [string[]]$Arguments
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE"
    }
}

function Start-Client {
    param([string[]]$Arguments)

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $clientPath
    $startInfo.UseShellExecute = $false
    foreach ($argument in $Arguments) {
        $startInfo.ArgumentList.Add($argument)
    }
    return [System.Diagnostics.Process]::Start($startInfo)
}

function Get-Status {
    return (& $clientPath status --json | ConvertFrom-Json)
}

function Find-Job {
    param([object]$Status, [string]$Name)
    return $Status.jobs | Where-Object { $_.name -eq $Name } | Select-Object -First 1
}

function Wait-Status {
    param(
        [scriptblock]$Predicate,
        [string]$Description,
        [int]$TimeoutSeconds = 30
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $status = Get-Status
        if (& $Predicate $status) {
            return $status
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for $Description"
}

function Save-Status {
    param([object]$Status, [string]$Name)
    $Status | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $results $Name)
}

New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
New-Item -ItemType Directory -Path $results -Force | Out-Null

try {
    $initial = Get-Status
    if (@($initial.jobs).Count -ne 0) {
        throw 'The canonical jobserver must be idle before the system test'
    }

    Invoke-Checked git @('-C', $sourceRoot, 'worktree', 'add', '--detach', $secondary, 'HEAD')
    $worktreeAdded = $true

    $fixtureSource = Join-Path $secondary 'tools\jobserver\tests\system\fixture'
    if (-not (Test-Path -LiteralPath $fixtureSource -PathType Container)) {
        $fixtureSource = Join-Path $sourceRoot 'tools\jobserver\tests\system\fixture'
    }
    Invoke-Checked $cmake @(
        '-S', $fixtureSource,
        '-B', $buildDirectory,
        '-G', 'Ninja',
        "-DJOBSERVER_CLI=$clientPath",
        "-DJOBSERVER_WORKTREE=$secondary"
    )

    $holder = Start-Client @(
        'run', '--name', $holderName, '--kind', 'test', '--worktree', $sourceRoot,
        '--shared', 'machine', '--', $cmake, '-E', 'sleep', '5'
    )
    $processes += $holder
    $holderStatus = Wait-Status { param($status)
        $job = Find-Job $status $holderName
        $null -ne $job -and $job.state -eq 'RUNNING'
    } 'primary holder'
    $holderJob = Find-Job $holderStatus $holderName
    Save-Status $holderStatus '01-holder.json'

    $exclusive = Start-Client @(
        'run', '--name', $exclusiveName, '--kind', 'benchmark', '--worktree', $secondary,
        '--exclusive', 'machine', '--exclusive', 'benchmark', '--',
        $cmake, '--build', $buildDirectory, '--target', 'jobserver-system-fixture'
    )
    $processes += $exclusive
    $queuedStatus = Wait-Status { param($status)
        $job = Find-Job $status $exclusiveName
        $null -ne $job -and $job.state -eq 'QUEUED'
    } 'exclusive build to queue'
    $exclusiveJob = Find-Job $queuedStatus $exclusiveName
    Save-Status $queuedStatus '02-exclusive-queued.json'
    if ($exclusiveJob.worktree -ne $secondary -or
        $exclusiveJob.blockers -notcontains $holderJob.id) {
        throw "Exclusive attribution/blocker mismatch: worktree='$($exclusiveJob.worktree)', expected='$secondary', blockers='$($exclusiveJob.blockers -join ',')', expected='$($holderJob.id)'"
    }

    $late = Start-Client @(
        'run', '--name', $lateName, '--kind', 'test', '--worktree', $sourceRoot,
        '--shared', 'machine', '--', $cmake, '-E', 'sleep', '1'
    )
    $processes += $late
    $fairStatus = Wait-Status { param($status)
        $exclusiveJob = Find-Job $status $exclusiveName
        $lateJob = Find-Job $status $lateName
        $null -ne $exclusiveJob -and $exclusiveJob.state -eq 'QUEUED' -and
            $null -ne $lateJob -and $lateJob.state -eq 'QUEUED'
    } 'fairness queue'
    $lateJob = Find-Job $fairStatus $lateName
    if ($lateJob.blockers -notcontains $exclusiveJob.id) {
        throw 'Newer shared work bypassed the older exclusive request'
    }
    Save-Status $fairStatus '03-fairness.json'

    if (-not $holder.WaitForExit(15000) -or $holder.ExitCode -ne 0) {
        throw "Holder failed with exit code $($holder.ExitCode)"
    }
    $exclusiveStatus = Wait-Status { param($status)
        $job = Find-Job $status $exclusiveName
        $machine = $status.resources | Where-Object { $_.name -eq 'machine' }
        $benchmark = $status.resources | Where-Object { $_.name -eq 'benchmark' }
        $null -ne $job -and $job.state -eq 'RUNNING' -and
            $machine.exclusive -eq $true -and $benchmark.exclusive -eq $true
    } 'exclusive build to run'
    Save-Status $exclusiveStatus '04-exclusive-running.json'

    if (-not $exclusive.WaitForExit(120000) -or $exclusive.ExitCode -ne 0) {
        throw "Exclusive CMake build failed with exit code $($exclusive.ExitCode)"
    }
    if (-not $late.WaitForExit(30000) -or $late.ExitCode -ne 0) {
        throw "Late shared job failed with exit code $($late.ExitCode)"
    }

    $idle = Wait-Status { param($status) @($status.jobs).Count -eq 0 } 'idle daemon'
    Save-Status $idle '05-idle.json'
    $history = & $clientPath history --json | ConvertFrom-Json
    $completed = @($history.jobs | Where-Object {
        $_.name -in @($holderName, $exclusiveName, $lateName)
    })
    if ($completed.Count -ne 3 -or
        @($completed | Where-Object { $_.state -ne 'SUCCEEDED' }).Count -ne 0) {
        throw 'System-test history does not contain three successful jobs'
    }

    $fixture = Join-Path $buildDirectory 'jobserver-system-fixture.exe'
    $fixtureOutput = & $fixture
    if ($LASTEXITCODE -ne 0 -or $fixtureOutput -ne 'jobserver system fixture') {
        throw 'The cross-worktree CMake fixture did not execute successfully'
    }
} finally {
    foreach ($process in $processes) {
        if ($null -ne $process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
    if ($worktreeAdded) {
        & git -C $sourceRoot worktree remove --force $secondary
    }
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Force -ErrorAction SilentlyContinue
    }
}
