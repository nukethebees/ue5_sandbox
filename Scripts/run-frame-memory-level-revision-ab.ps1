[CmdletBinding()]
param(
    [ValidateRange(1, 100)]
    [int] $Iterations = 5,

    [ValidateRange(0, 10)]
    [int] $WarmupIterations = 0,

    [string] $Baseline = 'HEAD',

    [string] $OutputDirectory = '',

    [string] $BaselineWorktree = '',

    [switch] $SkipBuild,

    [switch] $PrepareOnly,

    [switch] $ValidateOnly,

    [switch] $KeepBaselineWorktree
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$benchmarkName = 'NativeFrameMemoryLevel'
$benchmarkRoot = Join-Path $repo '.local/benchmarks/frame-memory-revision-ab'
$worktreeParent = Join-Path $repo '.local/abw'
$runTimestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if ($OutputDirectory) {
    $output = [IO.Path]::GetFullPath($OutputDirectory)
} else {
    $output = Join-Path $benchmarkRoot $runTimestamp
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)]
        [string] $Executable,

        [Parameter(Mandatory)]
        [string[]] $Arguments,

        [Parameter(Mandatory)]
        [string] $WorkingDirectory
    )

    Push-Location $WorkingDirectory
    try {
        & $Executable @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Executable failed with exit code $LASTEXITCODE in $WorkingDirectory"
        }
    } finally {
        Pop-Location
    }
}

function Resolve-GitCommit {
    param([string] $Revision)

    $commit = & git -C $repo rev-parse --verify "$Revision`^{commit}"
    if ($LASTEXITCODE -ne 0) {
        throw "Could not resolve baseline revision '$Revision'."
    }
    return $commit.Trim()
}

function Build-NativeBenchmark {
    param([string] $SourceDirectory)

    Invoke-Checked -Executable 'git' `
        -Arguments @('-C', $SourceDirectory, 'submodule', 'update', '--init', '--depth', '1',
                     'native/third_party/googletest',
                     'native/third_party/cpu_features',
                     'native/third_party/tracy',
                     'native/third_party/cli11') `
        -WorkingDirectory $SourceDirectory
    Invoke-Checked -Executable 'cmake' -Arguments @('--preset', 'frame-memory-level-benchmark') `
        -WorkingDirectory $SourceDirectory
    Invoke-Checked -Executable 'cmake' `
        -Arguments @('--build', '--preset', 'frame-memory-level-benchmark') `
        -WorkingDirectory $SourceDirectory
}

function Convert-BenchmarkResult {
    param(
        [object] $Result,
        [int] $Pair,
        [int] $Sequence,
        [string] $State,
        [string] $Commit
    )

    return [PSCustomObject]@{
        pair = $Pair
        sequence = $Sequence
        state = $State
        commit = $Commit
        variant = 'native'
        mean_tick_us = [double]$Result.timing.mean_tick_microseconds
        peak_claimed_bytes = [uint64]$Result.memory.frame_peak_claimed_bytes
        peak_payload_bytes = [uint64]$Result.memory.frame_peak_payload_bytes
        total_padding_bytes = [uint64]$Result.memory.frame_total_padding_bytes
        total_root_claims = [uint64]$Result.memory.frame_total_root_claims
    }
}

function Invoke-Benchmark {
    param(
        [string] $SourceDirectory,
        [int] $Pair,
        [int] $Sequence,
        [string] $State,
        [string] $Commit
    )

    $executable = Join-Path $SourceDirectory `
        'out/build/frame-memory-level-benchmark/bin/native-simulation-benchmark.exe'
    if (!(Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Native simulation benchmark executable was not built: $executable"
    }
    $level = Join-Path $SourceDirectory 'LevelScripts/Benchmarks/Batch_benchmark.scm'
    $activityModule = Join-Path $repo 'cmake/machine_activity.cmake'
    $activityRunner = Join-Path $repo 'cmake/run_with_machine_activity.cmake'
    $activityArguments = @(
        "-DMACHINE_ACTIVITY_MODULE=$activityModule"
        '-DMACHINE_ACTIVITY_MODE=benchmark'
        "-DMACHINE_ACTIVITY_OPERATION=$benchmarkName revision comparison"
        '-P'
        $activityRunner
        '--'
    )
    $benchmarkCommand = @($executable, '--level', $level, '--seconds', '20')
    $output = @(Invoke-Checked -Executable 'cmake' `
        -Arguments @($activityArguments + $benchmarkCommand) `
        -WorkingDirectory $SourceDirectory)
    $jsonLines = @($output | Where-Object { $_.TrimStart().StartsWith('{') })
    if ($jsonLines.Count -ne 1) {
        throw "$benchmarkName produced $($jsonLines.Count) JSON results."
    }
    $result = $jsonLines[0] | ConvertFrom-Json
    return Convert-BenchmarkResult -Result $result -Pair $Pair -Sequence $Sequence `
        -State $State -Commit $Commit
}

function Get-Median {
    param([double[]] $Values)

    $sorted = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1) {
        return $sorted[$middle]
    }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2.0
}

$baselineCommit = Resolve-GitCommit -Revision $Baseline
$candidateCommit = (& git -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Could not resolve the candidate HEAD commit.'
}
if ($SkipBuild -and !$BaselineWorktree) {
    throw '-SkipBuild requires -BaselineWorktree.'
}

New-Item -ItemType Directory -Force -Path $output | Out-Null
New-Item -ItemType Directory -Force -Path $worktreeParent | Out-Null
$worktree = if ($BaselineWorktree) {
    [IO.Path]::GetFullPath($BaselineWorktree)
} else {
    $worktreeName = [Guid]::NewGuid().ToString('N').Substring(0, 8)
    [IO.Path]::GetFullPath((Join-Path $worktreeParent $worktreeName))
}
$allowedWorktreeParent = [IO.Path]::GetFullPath($worktreeParent) + [IO.Path]::DirectorySeparatorChar
if (!$BaselineWorktree -and
    !$worktree.StartsWith($allowedWorktreeParent, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to create a benchmark worktree outside $worktreeParent"
}

$worktreeAdded = $false
$records = [Collections.Generic.List[object]]::new()
try {
    if ($BaselineWorktree) {
        if (!(Test-Path -LiteralPath $worktree -PathType Container)) {
            throw "Prepared baseline worktree does not exist: $worktree"
        }
        $preparedCommit = (& git -C $worktree rev-parse HEAD).Trim()
        if ($LASTEXITCODE -ne 0 -or $preparedCommit -ne $baselineCommit) {
            throw "Prepared worktree is not at baseline commit ${baselineCommit}: $worktree"
        }
    } else {
        Invoke-Checked -Executable 'git' `
            -Arguments @('-C', $repo, 'worktree', 'add', '--detach', $worktree, $baselineCommit) `
            -WorkingDirectory $repo
        $worktreeAdded = $true
    }

    if (!$SkipBuild) {
        Write-Host 'Building candidate working tree...'
        Build-NativeBenchmark -SourceDirectory $repo
        Write-Host "Building baseline $baselineCommit..."
        Build-NativeBenchmark -SourceDirectory $worktree
    }

    if ($PrepareOnly) {
        Write-Host "Candidate and baseline benchmark builds are ready. Baseline: $worktree"
        return
    }

    if ($ValidateOnly) {
        Invoke-Benchmark -SourceDirectory $worktree -Pair 0 -Sequence 1 `
            -State 'baseline' -Commit $baselineCommit | Out-Null
        Invoke-Benchmark -SourceDirectory $repo -Pair 0 -Sequence 2 `
            -State 'candidate' -Commit $candidateCommit | Out-Null
        Write-Host 'Candidate and baseline benchmark dry runs passed.'
        return
    }

    $sequence = 0
    foreach ($state in @('baseline', 'candidate')) {
        if ($state -eq 'baseline') {
            $sourceDirectory = $worktree
            $commit = $baselineCommit
        } else {
            $sourceDirectory = $repo
            $commit = $candidateCommit
        }

        for ($warmup = 1; $warmup -le $WarmupIterations; ++$warmup) {
            ++$sequence
            Write-Host "$state warmup $warmup of $WarmupIterations"
            Invoke-Benchmark -SourceDirectory $sourceDirectory -Pair 0 -Sequence $sequence `
                -State $state -Commit $commit | Out-Null
        }

        for ($iteration = 1; $iteration -le $Iterations; ++$iteration) {
            ++$sequence
            Write-Host "$state iteration $iteration of $Iterations"
            $results = Invoke-Benchmark -SourceDirectory $sourceDirectory -Pair $iteration `
                -Sequence $sequence -State $state -Commit $commit
            foreach ($result in $results) {
                $records.Add($result)
            }
        }
    }

    $rawResultsPath = Join-Path $output 'raw-results.csv'
    $records | Export-Csv -NoTypeInformation -LiteralPath $rawResultsPath

    $paired = [Collections.Generic.List[object]]::new()
    foreach ($pair in 1..$Iterations) {
        foreach ($variant in @('native')) {
            $baselineResult = @($records | Where-Object {
                $_.pair -eq $pair -and $_.state -eq 'baseline' -and $_.variant -eq $variant
            })
            $candidateResult = @($records | Where-Object {
                $_.pair -eq $pair -and $_.state -eq 'candidate' -and $_.variant -eq $variant
            })
            if ($baselineResult.Count -ne 1 -or $candidateResult.Count -ne 1) {
                throw "Pair $pair did not contain one baseline and candidate result for $variant."
            }

            $delta = $candidateResult.mean_tick_us - $baselineResult.mean_tick_us
            $paired.Add([PSCustomObject]@{
                pair = $pair
                variant = $variant
                baseline_tick_us = $baselineResult.mean_tick_us
                candidate_tick_us = $candidateResult.mean_tick_us
                delta_tick_us = $delta
                delta_percent = 100.0 * $delta / $baselineResult.mean_tick_us
            })
        }
    }

    $pairedResultsPath = Join-Path $output 'paired-results.csv'
    $paired | Export-Csv -NoTypeInformation -LiteralPath $pairedResultsPath

    Write-Host ''
    Write-Host "Results written to $output"
    foreach ($variant in @('native')) {
        $percentages = [double[]]@($paired | Where-Object variant -eq $variant |
            ForEach-Object { $_.delta_percent })
        $mean = ($percentages | Measure-Object -Average).Average
        $median = Get-Median -Values $percentages
        Write-Host ("{0}: mean delta {1:N3}%, median delta {2:N3}%" -f `
            $variant, $mean, $median)
    }
} finally {
    if ($worktreeAdded -and !$KeepBaselineWorktree) {
        Invoke-Checked -Executable 'git' `
            -Arguments @('-C', $repo, 'worktree', 'remove', '--force', $worktree) `
            -WorkingDirectory $repo
        Invoke-Checked -Executable 'git' -Arguments @('-C', $repo, 'worktree', 'prune') `
            -WorkingDirectory $repo
    }
}
