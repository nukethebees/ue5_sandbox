[CmdletBinding()]
param(
    [string[]] $FighterCaps = @('2000', '4000'),
    [double] $Seconds = 10.0,
    [double] $WarmupSeconds = 5.0,
    [double] $SaturationTimeoutSeconds = 60.0,
    [string] $OutputDirectory = '',
    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BenchmarkTools.ps1')
$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$runner = Get-BenchmarkToolsPath -RepositoryRoot $repo
$arguments = @('fighter-simulation', '--fighter-caps', ($FighterCaps -join ','), '--seconds', $Seconds.ToString('R', [Globalization.CultureInfo]::InvariantCulture), '--warmup-seconds', $WarmupSeconds.ToString('R', [Globalization.CultureInfo]::InvariantCulture), '--saturation-timeout-seconds', $SaturationTimeoutSeconds.ToString('R', [Globalization.CultureInfo]::InvariantCulture))
if ($OutputDirectory) { $arguments += @('--output-dir', $OutputDirectory) }
if ($SkipBuild) { $arguments += '--skip-build' }
& $runner @arguments
exit $LASTEXITCODE
