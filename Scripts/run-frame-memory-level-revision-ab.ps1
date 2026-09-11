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

    [switch] $KeepBaselineWorktree
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$benchmarkName = 'SandboxBenchmarks.FrameMemoryLevel'
$benchmarkPattern = '^SandboxBenchmarks\.FrameMemoryLevel$'
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

function Build-Editor {
    param([string] $SourceDirectory)

    Invoke-Checked -Executable 'cmake' -Arguments @('--preset', 'debug-game') `
        -WorkingDirectory $SourceDirectory
    Invoke-Checked -Executable 'cmake' `
        -Arguments @('--build', '--preset', 'debug-game', '--target', 'editor', 'core-tests',
                     'native-tests') `
        -WorkingDirectory $SourceDirectory
}

function Convert-BenchmarkLine {
    param(
        [string] $Line,
        [int] $Pair,
        [int] $Sequence,
        [string] $State,
        [string] $Commit
    )

    $marker = 'Frame memory level benchmark: '
    $markerIndex = $Line.IndexOf($marker, [StringComparison]::Ordinal)
    if ($markerIndex -lt 0) {
        throw 'Benchmark result marker was missing from a matched log line.'
    }

    $fields = @{}
    $payload = $Line.Substring($markerIndex + $marker.Length)
    foreach ($part in $payload.Split(', ')) {
        $separator = $part.IndexOf('=')
        if ($separator -gt 0) {
            $fields[$part.Substring(0, $separator)] = $part.Substring($separator + 1)
        }
    }

    foreach ($required in @('mean_tick_us', 'peak_claimed_bytes',
                             'peak_payload_bytes', 'total_padding_bytes',
                             'total_root_claims')) {
        if (!$fields.ContainsKey($required)) {
            throw "Benchmark result did not contain '$required': $Line"
        }
    }

    $culture = [Globalization.CultureInfo]::InvariantCulture
    return [PSCustomObject]@{
        pair = $Pair
        sequence = $Sequence
        state = $State
        commit = $Commit
        variant = if ($fields.ContainsKey('variant')) { $fields.variant } else { 'direct_root' }
        mean_tick_us = [double]::Parse($fields.mean_tick_us, $culture)
        peak_claimed_bytes = [uint64]::Parse($fields.peak_claimed_bytes, $culture)
        peak_payload_bytes = [uint64]::Parse($fields.peak_payload_bytes, $culture)
        total_padding_bytes = [uint64]::Parse($fields.total_padding_bytes, $culture)
        total_root_claims = [uint64]::Parse($fields.total_root_claims, $culture)
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

    $testDescription = (& ctest --preset debug-game-level-tests -N -R $benchmarkPattern `
        --show-only=json-v1 | Out-String | ConvertFrom-Json)
    if ($LASTEXITCODE -ne 0) {
        throw "Could not read the benchmark command in $SourceDirectory"
    }
    $tests = @($testDescription.tests)
    if ($tests.Count -ne 1) {
        throw "Expected one $benchmarkName test in $SourceDirectory"
    }
    $command = @($tests[0].command)
    Invoke-Checked -Executable $command[0] `
        -Arguments @($command[1..($command.Count - 1)] + '-notraceserver' +
                     '-traceautostart=0') `
        -WorkingDirectory $SourceDirectory

    $log = Join-Path $SourceDirectory 'Saved/Logs/Sandbox.log'
    $lines = @(Select-String -LiteralPath $log -SimpleMatch 'Frame memory level benchmark:' |
        ForEach-Object { $_.Line })
    if ($lines.Count -lt 1) {
        throw "$benchmarkName did not produce a result line in $log"
    }

    $results = @($lines | ForEach-Object {
        Convert-BenchmarkLine -Line $_ -Pair $Pair -Sequence $Sequence -State $State `
            -Commit $Commit
    })
    $directRootResults = @($results | Where-Object variant -eq 'direct_root')
    if ($directRootResults.Count -ne 1) {
        throw "$benchmarkName produced $($directRootResults.Count) direct-root results in $log"
    }
    return $directRootResults
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
        Build-Editor -SourceDirectory $repo
        Write-Host "Building baseline $baselineCommit..."
        Build-Editor -SourceDirectory $worktree
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

        Write-Host "Activating $state build..."
        Invoke-Checked -Executable 'cmake' `
            -Arguments @('--build', '--preset', 'debug-game', '--target', 'editor') `
            -WorkingDirectory $sourceDirectory

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
        foreach ($variant in @('direct_root')) {
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
    foreach ($variant in @('direct_root')) {
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
