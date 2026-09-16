[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [int[]] $FighterCaps = @(2000, 4000),

    [ValidateRange(0.1, 86400.0)]
    [double] $Seconds = 10.0,

    [ValidateRange(0.0, 86400.0)]
    [double] $WarmupSeconds = 5.0,

    [ValidateRange(0.1, 86400.0)]
    [double] $SaturationTimeoutSeconds = 60.0,

    [string] $OutputDirectory = '',

    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$runner = Join-Path $PSScriptRoot 'run-native-simulation-benchmark.ps1'
$level = Join-Path $repo 'LevelScripts/FighterSchedulingBenchmark.scm'

if ($FighterCaps.Count -eq 0 -or @($FighterCaps | Where-Object { $_ -le 0 }).Count -ne 0) {
    throw 'FighterCaps must contain positive values.'
}

if (-not $OutputDirectory) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $OutputDirectory = Join-Path $repo ".local/benchmarks/fighter-simulation/$timestamp"
} else {
    $OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$results = [Collections.Generic.List[object]]::new()
$summary = [Collections.Generic.List[object]]::new()
$expectedTicks = [int][Math]::Ceiling($Seconds * 60.0)
$arguments = @{
    Level = $level
    Seconds = $Seconds
    FighterStressCaps = $FighterCaps -join ','
    WarmupSeconds = $WarmupSeconds
    SaturationTimeoutSeconds = $SaturationTimeoutSeconds
    SkipBuild = $SkipBuild.IsPresent
}
Write-Host "Running fighter caps $($FighterCaps -join ', ') in one native benchmark process..."
$output = @(& $runner @arguments)
if ($LASTEXITCODE -ne 0) {
    throw "Fighter benchmark failed with exit code $LASTEXITCODE."
}
$jsonLines = @($output | Where-Object { $_.TrimStart().StartsWith('{') })
if ($jsonLines.Count -ne $FighterCaps.Count) {
    throw "Expected $($FighterCaps.Count) benchmark JSON results, found $($jsonLines.Count)."
}

for ($index = 0; $index -lt $FighterCaps.Count; ++$index) {
    $cap = $FighterCaps[$index]
    $result = $jsonLines[$index] | ConvertFrom-Json

    if ($result.level.id -ne 'fighter-scheduling-benchmark') {
        throw "Unexpected benchmark level id for cap ${cap}: $($result.level.id)"
    }
    if (-not $result.fighter_stress.enabled -or $result.fighter_stress.configured_cap -ne $cap) {
        throw "Fighter stress configuration was not applied for cap $cap."
    }
    if ($result.workload.measured_ticks -ne $expectedTicks) {
        throw "Cap $cap measured $($result.workload.measured_ticks) ticks; expected $expectedTicks."
    }
    if ($result.fighter_stress.steady_state_fighters -ne $cap -or
        $result.fighter_stress.minimum_measured_fighters -ne $cap -or
        $result.fighter_stress.maximum_measured_fighters -ne $cap) {
        throw "Fighter population was not steady at cap $cap."
    }
    if ($result.fighter_stress.fighter_spawns_during_measurement -ne 0) {
        throw "Cap $cap accepted replacement fighters during measurement."
    }
    if ($result.fighter_stress.task_counts.attack -ne $cap) {
        throw "Cap $cap ended with only $($result.fighter_stress.task_counts.attack) attacking fighters."
    }
    if ($result.fighter_stress.lasers_spawned_during_measurement -le 0) {
        throw "Cap $cap did not exercise fighter firing during measurement."
    }
    if ($result.memory.frame_overflow_count -ne 0) {
        throw "Cap $cap overflowed frame memory $($result.memory.frame_overflow_count) times."
    }

    $results.Add($result)
    $summary.Add([PSCustomObject]@{
        fighter_cap = $cap
        steady_state_fighters = $result.fighter_stress.steady_state_fighters
        measured_ticks = $result.workload.measured_ticks
        elapsed_seconds = $result.timing.elapsed_seconds
        mean_tick_us = $result.timing.mean_tick_microseconds
        median_tick_us = $result.timing.median_tick_microseconds
        p95_tick_us = $result.timing.p95_tick_microseconds
        p99_tick_us = $result.timing.p99_tick_microseconds
        ticks_per_second = $result.timing.ticks_per_second
        realtime_factor = $result.timing.realtime_factor
        lasers_spawned = $result.fighter_stress.lasers_spawned_during_measurement
    })
}

$resultsPath = Join-Path $OutputDirectory 'results.json'
$summaryPath = Join-Path $OutputDirectory 'summary.csv'
ConvertTo-Json -InputObject @($results) -Depth 8 | Set-Content -LiteralPath $resultsPath
$summary | Export-Csv -NoTypeInformation -LiteralPath $summaryPath

$summary | Format-Table -AutoSize
Write-Host "Results written to $OutputDirectory"
