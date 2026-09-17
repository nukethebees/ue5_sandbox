param(
    [ValidateRange(1, 100)]
    [int]$Samples = 7,
    [ValidateNotNullOrEmpty()]
    [string]$OutputDirectory = '.local/benchmarks/level-telemetry'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repository = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$output = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    [IO.Path]::GetFullPath($OutputDirectory)
} else {
    Join-Path $repository $OutputDirectory
}
New-Item -ItemType Directory -Force -Path $output | Out-Null

Push-Location -LiteralPath $repository
try {
    cmake --preset telemetry-benchmark "-DSANDBOX_TELEMETRY_BENCHMARK_SAMPLES=$Samples"
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    cmake --build --preset telemetry-benchmark
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $log = Join-Path $output 'telemetry-benchmark.log'
    ctest --preset telemetry-benchmark `
        --output-on-failure `
        --verbose 2>&1 | Tee-Object -FilePath $log

    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}
