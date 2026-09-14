param(
    [ValidateRange(1, 100000)]
    [int] $Samples = 100,
    [string] $OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (-not $env:NUKETHEBEES_JOBSERVER_JOB) {
    $jobserver = Join-Path $env:LOCALAPPDATA 'NukeTheBees/jobserver/bin/jobserver.exe'
    $powershell = (Get-Process -Id $PID).Path
    $activityArguments = @(
        'run'
        '--name'
        'single-allocation SoA reserve benchmark'
        '--kind'
        'benchmark'
        '--worktree'
        $repo
        '--exclusive'
        'machine'
        '--exclusive'
        'benchmark'
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
    & $jobserver @activityArguments
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
