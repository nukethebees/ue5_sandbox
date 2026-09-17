param(
    [Parameter(Mandatory = $true)]
    [string] $Level,
    [Parameter(Mandatory = $true)]
    [ValidateScript({
        ![double]::IsNaN($_) -and ![double]::IsInfinity($_) -and $_ -gt 0.0
    })]
    [double] $Seconds,
    [ValidateRange(1, [uint32]::MaxValue)]
    [uint32] $GameSpeed = 1,
    [switch] $Telemetry,
    [string] $FighterStressCap = '',
    [string] $FighterStressCaps = '',
    [ValidateRange(0.0, 86400.0)]
    [double] $WarmupSeconds = 5.0,
    [ValidateRange(0.1, 86400.0)]
    [double] $SaturationTimeoutSeconds = 60.0,
    [string] $BuildPreset = 'native-simulation-benchmark',
    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$levelPath = (Resolve-Path -LiteralPath $Level).Path
function ConvertTo-FighterStressCaps {
    param(
        [string] $Value,
        [string] $ParameterName
    )

    $caps = [Collections.Generic.List[int]]::new()
    foreach ($capText in $Value.Split(',', [StringSplitOptions]::None)) {
        $cap = 0
        if (-not [int]::TryParse($capText.Trim(), [Globalization.NumberStyles]::None,
                [Globalization.CultureInfo]::InvariantCulture, [ref] $cap) -or $cap -le 0) {
            throw "$ParameterName must contain unique positive comma-separated 32-bit integers."
        }
        if ($caps.Contains($cap)) {
            throw "$ParameterName must contain unique positive comma-separated 32-bit integers."
        }
        $caps.Add($cap)
    }

    return $caps.ToArray()
}

$fighterStressCapValue = $null
if ($FighterStressCap -ne '') {
    $fighterStressCapValues = @(ConvertTo-FighterStressCaps $FighterStressCap 'FighterStressCap')
    if ($fighterStressCapValues.Count -ne 1) {
        throw 'FighterStressCap must contain one positive 32-bit integer.'
    }
    $fighterStressCapValue = $fighterStressCapValues[0]
}

$fighterStressCapValues = @()
if ($FighterStressCaps -ne '') {
    $fighterStressCapValues = @(ConvertTo-FighterStressCaps $FighterStressCaps 'FighterStressCaps')
}

if ($null -ne $fighterStressCapValue -and $fighterStressCapValues.Count -gt 0) {
    throw 'FighterStressCap and FighterStressCaps are mutually exclusive.'
}

if (-not $SkipBuild) {
    Push-Location $repo
    try {
        & cmake --preset $BuildPreset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & cmake --build --preset $BuildPreset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    finally {
        Pop-Location
    }
}

$secondsText = $Seconds.ToString('R', [System.Globalization.CultureInfo]::InvariantCulture)
$warmupSecondsText = $WarmupSeconds.ToString('R', [System.Globalization.CultureInfo]::InvariantCulture)
$saturationTimeoutSecondsText = $SaturationTimeoutSeconds.ToString(
    'R',
    [System.Globalization.CultureInfo]::InvariantCulture)

if (-not $env:NUKETHEBEES_JOBSERVER_JOB) {
    $jobserver = Join-Path $env:LOCALAPPDATA 'NukeTheBees/jobserver/bin/jobserver.exe'
    $powershell = (Get-Process -Id $PID).Path
    $activityArguments = @(
        'run'
        '--name'
        'native simulation benchmark'
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
        '-Level'
        $levelPath
        '-Seconds'
        $secondsText
        '-GameSpeed'
        $GameSpeed
        '-BuildPreset'
        $BuildPreset
        '-SkipBuild'
    )
    if ($Telemetry) { $activityArguments += '-Telemetry' }
    if ($null -ne $fighterStressCapValue) {
        $activityArguments += @(
            '-FighterStressCap'
            $fighterStressCapValue
            '-WarmupSeconds'
            $warmupSecondsText
            '-SaturationTimeoutSeconds'
            $saturationTimeoutSecondsText
        )
    }
    if ($fighterStressCapValues.Count -gt 0) {
        $activityArguments += '-FighterStressCaps'
        $activityArguments += ($fighterStressCapValues -join ',')
        $activityArguments += @(
            '-WarmupSeconds'
            $warmupSecondsText
            '-SaturationTimeoutSeconds'
            $saturationTimeoutSecondsText
        )
    }
    & $jobserver @activityArguments
    exit $LASTEXITCODE
}

$executable = Join-Path $repo "out/build/$BuildPreset/bin/native-simulation-benchmark.exe"
$arguments = @('--level', $levelPath, '--seconds', $secondsText, '--game-speed', $GameSpeed)
if ($Telemetry) { $arguments += '--telemetry' }
if ($null -ne $fighterStressCapValue) {
    $arguments += @(
        '--fighter-stress-cap', $fighterStressCapValue,
        '--warmup-seconds', $warmupSecondsText,
        '--saturation-timeout-seconds', $saturationTimeoutSecondsText
    )
}
if ($fighterStressCapValues.Count -gt 0) {
    $arguments += '--fighter-stress-caps'
    $arguments += $fighterStressCapValues
    $arguments += @(
        '--warmup-seconds', $warmupSecondsText,
        '--saturation-timeout-seconds', $saturationTimeoutSecondsText
    )
}
& $executable @arguments
exit $LASTEXITCODE
