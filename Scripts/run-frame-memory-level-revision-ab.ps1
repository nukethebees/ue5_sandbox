[CmdletBinding()]
param(
    [int] $Iterations = 5,
    [int] $WarmupIterations = 0,
    [string] $Baseline = 'HEAD',
    [string] $OutputDirectory = '',
    [string] $BaselineWorktree = '',
    [switch] $SkipBuild,
    [switch] $PrepareOnly,
    [switch] $ValidateOnly,
    [switch] $KeepBaselineWorktree
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BenchmarkTools.ps1')
$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$runner = Get-BenchmarkToolsPath -RepositoryRoot $repo
$arguments = @('frame-memory-revision-ab', '--iterations', $Iterations, '--warmup-iterations', $WarmupIterations, '--baseline', $Baseline)
if ($OutputDirectory) { $arguments += @('--output-dir', $OutputDirectory) }
if ($BaselineWorktree) { $arguments += @('--baseline-worktree', $BaselineWorktree) }
if ($SkipBuild) { $arguments += '--skip-build' }
if ($PrepareOnly) { $arguments += '--prepare-only' }
if ($ValidateOnly) { $arguments += '--validate-only' }
if ($KeepBaselineWorktree) { $arguments += '--keep-baseline-worktree' }
& $runner @arguments
exit $LASTEXITCODE
