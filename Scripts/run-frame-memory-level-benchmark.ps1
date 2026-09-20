[CmdletBinding()]
param(
    [double] $Seconds = 20.0,
    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BenchmarkTools.ps1')
$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$runner = Get-BenchmarkToolsPath -RepositoryRoot $repo
$arguments = @('frame-memory-level', '--seconds', $Seconds.ToString('R', [Globalization.CultureInfo]::InvariantCulture))
if ($SkipBuild) { $arguments += '--skip-build' }
& $runner @arguments
exit $LASTEXITCODE
