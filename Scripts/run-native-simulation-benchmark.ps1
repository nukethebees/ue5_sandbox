param(
    [Parameter(Mandatory = $true)]
    [string] $Level,
    [Parameter(Mandatory = $true)]
    [ValidateScript({
        ![double]::IsNaN($_) -and ![double]::IsInfinity($_) -and $_ -gt 0.0
    })]
    [double] $Seconds,
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

if ($env:SANDBOX_MACHINE_ACTIVITY_MODE -ne 'benchmark') {
    $module = Join-Path $repo 'cmake/machine_activity.cmake'
    $runner = Join-Path $repo 'cmake/run_with_machine_activity.cmake'
    $powershell = (Get-Process -Id $PID).Path
    $secondsText = $Seconds.ToString('R', [System.Globalization.CultureInfo]::InvariantCulture)
    & cmake `
        "-DMACHINE_ACTIVITY_MODULE=$module" `
        '-DMACHINE_ACTIVITY_MODE=benchmark' `
        '-DMACHINE_ACTIVITY_OPERATION=native simulation benchmark' `
        -P $runner -- $powershell -NoProfile -File $PSCommandPath `
        -Level $levelPath -Seconds $secondsText -BuildPreset $BuildPreset -SkipBuild
    exit $LASTEXITCODE
}

$executable = Join-Path $repo "out/build/$BuildPreset/bin/native-simulation-benchmark.exe"
$secondsText = $Seconds.ToString('R', [System.Globalization.CultureInfo]::InvariantCulture)
& $executable --level $levelPath --seconds $secondsText
exit $LASTEXITCODE
