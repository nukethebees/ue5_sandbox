[CmdletBinding()]
param(
    [ValidateRange(1, 100)]
    [int] $Pairs = 3,

    [ValidateRange(0.1, 86400.0)]
    [double] $Seconds = 60.0,

    [ValidateRange(1, [uint32]::MaxValue)]
    [uint32] $GameSpeed = 100,

    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$preset = 'telemetry-integration-benchmark'
$level = Join-Path $repo 'LevelScripts/SparkRendererShowcase.scm'

if (-not $SkipBuild) {
    Push-Location $repo
    try {
        & cmake --preset $preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & cmake --build --preset $preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    finally {
        Pop-Location
    }
}

if ($env:SANDBOX_MACHINE_ACTIVITY_MODE -ne 'benchmark') {
    $module = Join-Path $repo 'cmake/machine_activity.cmake'
    $runner = Join-Path $repo 'cmake/run_with_machine_activity.cmake'
    $powershell = (Get-Process -Id $PID).Path
    $secondsText = $Seconds.ToString('R', [System.Globalization.CultureInfo]::InvariantCulture)
    & cmake `
        "-DMACHINE_ACTIVITY_MODULE=$module" `
        '-DMACHINE_ACTIVITY_MODE=benchmark' `
        '-DMACHINE_ACTIVITY_OPERATION=level telemetry integration benchmark' `
        -P $runner -- $powershell -NoProfile -File $PSCommandPath `
        -Pairs $Pairs -Seconds $secondsText -GameSpeed $GameSpeed -SkipBuild
    exit $LASTEXITCODE
}

$executable = Join-Path $repo "out/build/$preset/bin/native-simulation-benchmark.exe"
$secondsText = $Seconds.ToString('R', [System.Globalization.CultureInfo]::InvariantCulture)

function Invoke-Trial {
    param([bool] $DetailedTiming)

    $arguments = @(
        '--level', $level,
        '--seconds', $secondsText,
        '--game-speed', $GameSpeed,
        '--telemetry'
    )
    if ($DetailedTiming) { $arguments += '--detailed-timing' }

    $output = @(& $executable @arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "Native telemetry benchmark failed with exit code $LASTEXITCODE."
    }
    $jsonLines = @($output | Where-Object { $_.TrimStart().StartsWith('{') })
    if ($jsonLines.Count -ne 1) {
        throw "Expected one native simulation benchmark JSON result, found $($jsonLines.Count)."
    }
    $result = $jsonLines[0] | ConvertFrom-Json
    if ($result.level.id -ne 'spark-renderer-showcase') {
        throw "Unexpected telemetry benchmark level id: $($result.level.id)"
    }
    if ($result.workload.completed_ticks -ne $result.workload.requested_ticks) {
        throw 'Telemetry benchmark did not complete the requested deterministic workload.'
    }
    if ($result.workload.game_speed -ne $GameSpeed) {
        throw "Unexpected telemetry benchmark game speed: $($result.workload.game_speed)"
    }
    if (-not $result.telemetry.enabled -or $result.telemetry.rows -le 0) {
        throw 'Telemetry benchmark did not capture level telemetry.'
    }
    if ($result.telemetry.detailed_timing -ne $DetailedTiming) {
        throw 'Telemetry benchmark reported the wrong detailed-timing mode.'
    }
    if ($DetailedTiming -and ($result.telemetry.performance_windows -le 0 -or
                              $result.telemetry.simulation_cpu_ms -le 0.0)) {
        throw 'Detailed telemetry timing did not produce performance samples.'
    }
    return $result
}

# Preserve the old benchmark's untimed warmup pair.
Invoke-Trial -DetailedTiming $false | Out-Null
Invoke-Trial -DetailedTiming $true | Out-Null

$timingOffThroughput = @()
$timingOnThroughput = @()
for ($pair = 0; $pair -lt $Pairs; ++$pair) {
    if (($pair -band 1) -eq 0) {
        $timingOffThroughput += (Invoke-Trial -DetailedTiming $false).timing.ticks_per_second
        $timingOnThroughput += (Invoke-Trial -DetailedTiming $true).timing.ticks_per_second
    }
    else {
        $timingOnThroughput += (Invoke-Trial -DetailedTiming $true).timing.ticks_per_second
        $timingOffThroughput += (Invoke-Trial -DetailedTiming $false).timing.ticks_per_second
    }
}

$timingOffThroughput = @($timingOffThroughput | Sort-Object)
$timingOnThroughput = @($timingOnThroughput | Sort-Object)
$medianIndex = [int][Math]::Floor($Pairs / 2)
$timingOffMedian = $timingOffThroughput[$medianIndex]
$timingOnMedian = $timingOnThroughput[$medianIndex]
$impactPercent = ($timingOffMedian - $timingOnMedian) / $timingOffMedian * 100.0

if ($impactPercent -ge 2.0) {
    throw "Detailed timing median throughput impact was $impactPercent%, expected less than 2%."
}

[PSCustomObject]@{
    schema_version = 1
    benchmark = 'level-telemetry-integration'
    level = 'spark-renderer-showcase'
    workload = [PSCustomObject]@{
        requested_seconds = $Seconds
        game_speed = $GameSpeed
        pairs = $Pairs
    }
    timing = [PSCustomObject]@{
        timing_off_median_ticks_per_second = $timingOffMedian
        timing_on_median_ticks_per_second = $timingOnMedian
        detailed_timing_impact_percent = $impactPercent
    }
} | ConvertTo-Json -Depth 4 -Compress
