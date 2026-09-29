$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$project_root = Split-Path -Parent $PSScriptRoot

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

function Invoke-ModuleLoadSmoke {
    param(
        [Parameter(Mandatory)]
        [string]$Preset
    )

    Invoke-CMake @('--preset', $Preset)
    Invoke-CMake @('--build', '--preset', $Preset, '--target', 'editor')

    $build_directory = Join-Path $project_root "out\build\$Preset"
    Write-Host "ctest --test-dir $build_directory -R ^SandboxISMC\.ModuleLoadSmoke$ --output-on-failure"
    & ctest --test-dir $build_directory -R '^SandboxISMC\.ModuleLoadSmoke$' --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "SandboxISMC module-load smoke for '$Preset' exited with code $LASTEXITCODE."
    }
}

Push-Location -LiteralPath $project_root
try {
    Invoke-ModuleLoadSmoke 'debug-game'
    Invoke-ModuleLoadSmoke 'development'
    Invoke-ModuleLoadSmoke 'debug-game'
} finally {
    Pop-Location
}

Write-Host 'DebugGame -> Development -> DebugGame editor module transition passed.'
