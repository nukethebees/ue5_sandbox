param(
    [ValidateRange(1, 100)]
    [int]$Samples = 7,
    [string]$OutputDirectory = ".local/benchmarks/level-telemetry"
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
$output = Join-Path $repository $OutputDirectory
New-Item -ItemType Directory -Force -Path $output | Out-Null

cmake --preset benchmark "-DSANDBOX_TELEMETRY_BENCHMARK_SAMPLES=$Samples"
cmake --build --preset benchmark --target editor

$log = Join-Path $output "telemetry-benchmark.log"
ctest --test-dir (Join-Path $repository "out/build/benchmark") `
    -L "^telemetry-benchmark$" `
    --output-on-failure `
    --verbose 2>&1 | Tee-Object -FilePath $log

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
