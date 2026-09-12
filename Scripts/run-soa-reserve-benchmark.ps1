param(
    [ValidateRange(1, 100000)]
    [int] $Samples = 100,
    [string] $OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if ($env:SANDBOX_MACHINE_ACTIVITY_MODE -ne 'benchmark') {
    $module = Join-Path $repo 'cmake/machine_activity.cmake'
    $runner = Join-Path $repo 'cmake/run_with_machine_activity.cmake'
    $powershell = (Get-Process -Id $PID).Path
    $activityArguments = @(
        "-DMACHINE_ACTIVITY_MODULE=$module"
        '-DMACHINE_ACTIVITY_MODE=benchmark'
        '-DMACHINE_ACTIVITY_OPERATION=single-allocation SoA reserve benchmark'
        '-P'
        $runner
        '--'
        $powershell
        '-NoProfile'
        '-File'
        $PSCommandPath
        '-Samples'
        $Samples
    )
    if ($OutputDirectory) {
        $activityArguments += @('-OutputDirectory', $OutputDirectory)
    }
    & cmake @activityArguments
    exit $LASTEXITCODE
}

$output = Join-Path $repo '.local/benchmarks/single-allocation/reserve'
if ($OutputDirectory) { $output = $OutputDirectory }
$executable = Join-Path $repo 'Binaries/Win64/SandboxCoreBenchmarks/SandboxCoreBenchmarks.exe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$logs = @()
foreach ($implementation in @('TArray', 'SingleMimalloc')) {
    $log = Join-Path $output "reserve-$implementation.log"
    & $executable "SandboxCore.SingleAllocation.Timing.reserve_$implementation" --benchmark-samples $Samples |
        Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $logs += $log
}
& uv run (Join-Path $PSScriptRoot 'plot-single-allocation-soa-benchmarks.py') --input @logs --output-dir $output
exit $LASTEXITCODE
