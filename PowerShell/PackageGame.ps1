[CmdletBinding()]
param(
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Shipping',

    [string]$ArtifactRoot,

    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$project_root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ArtifactRoot)) {
    $ArtifactRoot = Join-Path $project_root 'out\game-package'
}
$artifact_root = [System.IO.Path]::GetFullPath($ArtifactRoot)

function Invoke-CMake {
    param(
        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    Write-Host "cmake $($Arguments -join ' ')"
    & cmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "CMake exited with code $LASTEXITCODE."
    }
}

function Invoke-CTest {
    param(
        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    Write-Host "ctest $($Arguments -join ' ')"
    & ctest @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "CTest exited with code $LASTEXITCODE."
    }
}

function Configure-PackagePreset {
    param(
        [Parameter(Mandatory)]
        [string]$Preset
    )

    Invoke-CMake @(
        '--preset',
        $Preset,
        "-DSANDBOX_GAME_ARTIFACT_ROOT=$artifact_root"
    )
}

Push-Location -LiteralPath $project_root
try {
    if (-not $SkipTests) {
        Invoke-CMake @('--preset', 'debug-game')
        Invoke-CMake @('--build', '--preset', 'debug-game', '--verbose')
        Invoke-CTest @('--preset', 'debug-game-tests')
    }

    Configure-PackagePreset 'development'
    if ($Configuration -eq 'Development') {
        foreach ($preset in @(
            'development-game',
            'development-cook',
            'development-stage',
            'development-archive',
            'development-verify-package')) {
            Invoke-CMake @('--build', '--preset', $preset, '--verbose')
        }
    } else {
        Invoke-CMake @('--build', '--preset', 'development-cook', '--verbose')
        Configure-PackagePreset 'shipping'
        foreach ($preset in @(
            'shipping',
            'shipping-stage',
            'shipping-archive',
            'shipping-verify-package')) {
            Invoke-CMake @('--build', '--preset', $preset, '--verbose')
        }
    }
} finally {
    Pop-Location
}

Write-Host "Package completed under '$artifact_root'."
