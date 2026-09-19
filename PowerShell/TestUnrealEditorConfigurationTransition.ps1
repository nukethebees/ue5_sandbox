[CmdletBinding()]
param(
    [switch]$UnderEngineLease
)

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

function Get-EngineResource {
    $cache_path = Join-Path $project_root 'out\build\debug-game\CMakeCache.txt'
    $cache_entry = Select-String -LiteralPath $cache_path `
        -Pattern '^UE_ENGINE_JOBSERVER_RESOURCE:INTERNAL=(?<resource>.+)$'
    if ($null -eq $cache_entry) {
        throw "The configured engine jobserver resource was not found in '$cache_path'."
    }

    $cache_entry.Matches[0].Groups['resource'].Value
}

Push-Location -LiteralPath $project_root
try {
    if (-not $UnderEngineLease) {
        Invoke-CMake @('--preset', 'debug-game')
        Invoke-CMake @('--preset', 'development')

        $jobserver = Join-Path $env:LOCALAPPDATA 'NukeTheBees\jobserver\bin\jobserver.exe'
        if (-not (Test-Path -LiteralPath $jobserver -PathType Leaf)) {
            throw "The per-user jobserver is not installed: '$jobserver'."
        }

        $engine_resource = Get-EngineResource
        & $jobserver run `
            --name 'Unreal editor configuration transition' `
            --kind test `
            --worktree $project_root `
            --shared machine `
            --exclusive $engine_resource `
            -- `
            pwsh -NoProfile -File $PSCommandPath -UnderEngineLease
        if ($LASTEXITCODE -ne 0) {
            throw "The editor configuration transition exited with code $LASTEXITCODE."
        }

        return
    }

    Invoke-ModuleLoadSmoke 'debug-game'
    Invoke-ModuleLoadSmoke 'development'
    Invoke-ModuleLoadSmoke 'debug-game'
} finally {
    Pop-Location
}

Write-Host 'DebugGame -> Development -> DebugGame editor module transition passed.'
