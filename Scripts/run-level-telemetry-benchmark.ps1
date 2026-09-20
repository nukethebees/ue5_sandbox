param(
    [int]$Samples = 7,
    [string]$OutputDirectory = '.local/benchmarks/level-telemetry'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BenchmarkTools.ps1')
$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$runner = Get-BenchmarkToolsPath -RepositoryRoot $repo
& $runner level-telemetry --samples $Samples --output-dir $OutputDirectory
exit $LASTEXITCODE
