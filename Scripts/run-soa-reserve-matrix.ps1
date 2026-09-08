param(
    [ValidateSet(4096, 65536, 1048576)]
    [int] $Rows = 65536,
    [ValidateRange(1, 100000)]
    [int] $Samples = 10,
    [ValidateSet(1, 2, 4, 8, 16, 32, 64, 128, 200, 256, 512)]
    [int[]] $Owners = @(1, 2, 4, 8, 16, 32, 64, 128, 200, 256, 512),
    [ValidateSet('TArray', 'Single', 'SingleMimalloc', 'SoAMimalloc', 'RawMalloc', 'RawRealloc', 'SoAMalloc', 'SoARealloc')]
    [string[]] $Implementations = @('TArray', 'Single', 'SingleMimalloc', 'SoAMimalloc', 'RawMalloc', 'RawRealloc', 'SoAMalloc', 'SoARealloc'),
    [string] $OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$output = Join-Path $repo '.local/benchmarks/single-allocation/reserve-matrix-per-implementation'
if ($OutputDirectory) { $output = $OutputDirectory }
$executable = Join-Path $repo 'Binaries/Win64/SandboxCoreBenchmarks/SandboxCoreBenchmarks.exe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$logs = @()
foreach ($ownerCount in $Owners) {
    foreach ($implementation in $Implementations) {
        $log = Join-Path $output "reserve-$Rows-owners-$ownerCount-$implementation.log"
        & $executable "SandboxCore.SingleAllocation.Timing.reserve_${Rows}_owners_${ownerCount}_$implementation" --benchmark-samples $Samples |
            Tee-Object -FilePath $log
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        $logs += $log
    }
}
& uv run (Join-Path $PSScriptRoot 'plot-single-allocation-soa-benchmarks.py') --input @logs --output-dir $output
exit $LASTEXITCODE
