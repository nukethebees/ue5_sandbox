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
    [switch] $DetailedTiming,
    [string] $BuildPreset = 'native-simulation-benchmark',
    [switch] $SkipBuild
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$levelPath = (Resolve-Path -LiteralPath $Level).Path

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

if (-not $env:NUKETHEBEES_JOBSERVER_JOB) {
    $jobserver = Join-Path $env:LOCALAPPDATA 'NukeTheBees/jobserver/bin/jobserver.exe'
    $powershell = (Get-Process -Id $PID).Path
    $secondsText = $Seconds.ToString('R', [System.Globalization.CultureInfo]::InvariantCulture)
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
    if ($DetailedTiming) { $activityArguments += '-DetailedTiming' }
    & $jobserver @activityArguments
    exit $LASTEXITCODE
}

$executable = Join-Path $repo "out/build/$BuildPreset/bin/native-simulation-benchmark.exe"
$secondsText = $Seconds.ToString('R', [System.Globalization.CultureInfo]::InvariantCulture)
$arguments = @('--level', $levelPath, '--seconds', $secondsText, '--game-speed', $GameSpeed)
if ($Telemetry) { $arguments += '--telemetry' }
if ($DetailedTiming) { $arguments += '--detailed-timing' }
& $executable @arguments
exit $LASTEXITCODE
