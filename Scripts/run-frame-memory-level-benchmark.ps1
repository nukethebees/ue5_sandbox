[CmdletBinding()]
param(
    [ValidateRange(0.1, 86400.0)]
    [double] $Seconds = 20.0,

    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$runner = Join-Path $PSScriptRoot 'run-native-simulation-benchmark.ps1'
$level = Join-Path $repo 'LevelScripts/Benchmarks/Batch_benchmark.scm'
$arguments = @{
    Level = $level
    Seconds = $Seconds
    GameSpeed = 100
    BuildPreset = 'frame-memory-level-benchmark'
    SkipBuild = $SkipBuild.IsPresent
}

$output = @(& $runner @arguments)
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$json_lines = @($output | Where-Object { $_.TrimStart().StartsWith('{') })
if ($json_lines.Count -ne 1) {
    throw "Expected one native simulation benchmark JSON result, found $($json_lines.Count)."
}
$result = $json_lines[0] | ConvertFrom-Json

if ($result.level.id -ne 'batch-benchmark') {
    throw "Unexpected benchmark level id: $($result.level.id)"
}
if ($result.workload.completed_ticks -ne $result.workload.requested_ticks) {
    throw 'Frame-memory benchmark did not complete the requested deterministic workload.'
}
if ($result.workload.game_speed -ne 100) {
    throw "Unexpected frame-memory benchmark game speed: $($result.workload.game_speed)"
}
$expected_advance_calls = $result.workload.requested_ticks
if ($result.workload.advance_calls -ne $expected_advance_calls) {
    throw "Unexpected deterministic advance count: $($result.workload.advance_calls)"
}
if ($result.memory.frame_overflow_count -ne 0) {
    throw "Frame memory overflowed $($result.memory.frame_overflow_count) times."
}
if ($result.memory.frame_peak_claimed_bytes -le 0) {
    throw 'The simulation did not claim frame memory.'
}
if ($result.final_state.peak_fighters -le 0) {
    throw 'The benchmark did not reach fighter spawning.'
}

$result | ConvertTo-Json -Depth 8 -Compress
