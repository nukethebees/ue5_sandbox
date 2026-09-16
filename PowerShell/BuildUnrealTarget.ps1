[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$build_script,

    [Parameter(Mandatory)]
    [string]$target,

    [Parameter(Mandatory)]
    [string]$platform,

    [Parameter(Mandatory)]
    [string]$configuration,

    [Parameter(Mandatory)]
    [string]$project,

    [Parameter(Mandatory)]
    [string]$native_toolchain,

    [switch]$verify_editor_modules
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-ModuleManifestName {
    param(
        [Parameter(Mandatory)]
        [string]$build_configuration
    )

    if ($build_configuration -eq 'Development') {
        return 'UnrealEditor.modules'
    }

    "UnrealEditor-Win64-$build_configuration.modules"
}

function Get-TargetReceiptName {
    param(
        [Parameter(Mandatory)]
        [string]$editor_target,

        [Parameter(Mandatory)]
        [string]$build_configuration
    )

    if ($build_configuration -eq 'Development') {
        return "$editor_target.target"
    }

    "$editor_target-Win64-$build_configuration.target"
}

function Get-EditorModuleManifestPaths {
    param(
        [Parameter(Mandatory)]
        [string]$project_root,

        [Parameter(Mandatory)]
        [string]$editor_target,

        [Parameter(Mandatory)]
        [string]$build_configuration
    )

    $manifest_name = Get-ModuleManifestName $build_configuration
    $target_receipt_name = Get-TargetReceiptName $editor_target $build_configuration
    $target_receipt = Join-Path $project_root "Binaries\\Win64\\$target_receipt_name"
    if (-not (Test-Path -LiteralPath $target_receipt -PathType Leaf)) {
        return [PSCustomObject]@{
            Paths = @()
            Problem = "The editor target receipt is missing: $target_receipt"
        }
    }

    $receipt = Get-Content -LiteralPath $target_receipt -Raw | ConvertFrom-Json
    $manifest_paths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $project_manifest = Join-Path $project_root "Binaries\\Win64\\$manifest_name"
    $null = $manifest_paths.Add($project_manifest)

    foreach ($build_product in $receipt.BuildProducts) {
        $product_path = $build_product.Path
        if ($null -eq $product_path) {
            continue
        }

        $expanded_path = $product_path.Replace('$(ProjectDir)', $project_root).Replace('/', '\\')
        $plugin_root = Join-Path $project_root 'Plugins'
        if (-not $expanded_path.StartsWith($plugin_root, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        $binary_directory = Split-Path -Parent $expanded_path
        $null = $manifest_paths.Add((Join-Path $binary_directory $manifest_name))
    }

    [PSCustomObject]@{
        Paths = @($manifest_paths)
        Problem = $null
    }
}

function Get-EditorModuleCompatibilityProblems {
    param(
        [Parameter(Mandatory)]
        [string]$engine_root,

        [Parameter(Mandatory)]
        [string[]]$manifest_paths
    )

    $version_path = Join-Path $engine_root 'Engine\\Binaries\\Win64\\UnrealEditor.version'
    if (-not (Test-Path -LiteralPath $version_path -PathType Leaf)) {
        throw "The editor version file is missing: $version_path"
    }

    $expected_build_id = (Get-Content -LiteralPath $version_path -Raw | ConvertFrom-Json).BuildId
    if ([string]::IsNullOrWhiteSpace($expected_build_id)) {
        throw "The editor version file does not contain a BuildId: $version_path"
    }

    $problems = [System.Collections.Generic.List[string]]::new()
    foreach ($manifest_path in $manifest_paths) {
        if (-not (Test-Path -LiteralPath $manifest_path -PathType Leaf)) {
            $problems.Add("missing '$manifest_path'")
            continue
        }

        $actual_build_id = (Get-Content -LiteralPath $manifest_path -Raw | ConvertFrom-Json).BuildId
        if ($actual_build_id -ne $expected_build_id) {
            $problems.Add("'$manifest_path' has BuildId $actual_build_id; editor expects $expected_build_id")
        }
    }

    [PSCustomObject]@{
        ExpectedBuildId = $expected_build_id
        Problems = @($problems)
    }
}

$resolved_build_script = (Resolve-Path -LiteralPath $build_script).Path
$project_root = Split-Path -Parent (Resolve-Path -LiteralPath $project).Path
$engine_root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $resolved_build_script)))
$force_rebuild = $false
$manifest_paths = @()

if ($verify_editor_modules) {
    $manifest_result = Get-EditorModuleManifestPaths $project_root $target $configuration
    if ($null -ne $manifest_result.Problem) {
        Write-Warning "$($manifest_result.Problem) Forcing an editor rebuild."
        $force_rebuild = $true
    } else {
        $manifest_paths = $manifest_result.Paths
        $compatibility = Get-EditorModuleCompatibilityProblems $engine_root $manifest_paths
        if ($compatibility.Problems.Count -gt 0) {
            Write-Warning "Editor module BuildId mismatch detected. Forcing '$target $platform $configuration' to resynchronize modules."
            $compatibility.Problems | ForEach-Object { Write-Warning "  $_" }
            $force_rebuild = $true
        }
    }
}

$ubt_arguments = @(
    $target,
    $platform,
    $configuration,
    "-Project=$project",
    '-WaitMutex'
)
if ($force_rebuild) {
    $ubt_arguments += '-Force'
}

$previous_toolchain = $env:SANDBOX_NATIVE_TOOLCHAIN
$env:SANDBOX_NATIVE_TOOLCHAIN = $native_toolchain
try {
    & $resolved_build_script @ubt_arguments
    if ($LASTEXITCODE -ne 0) {
        throw "UnrealBuildTool exited with code $LASTEXITCODE."
    }
} finally {
    $env:SANDBOX_NATIVE_TOOLCHAIN = $previous_toolchain
}

if ($verify_editor_modules) {
    $manifest_result = Get-EditorModuleManifestPaths $project_root $target $configuration
    if ($null -ne $manifest_result.Problem) {
        throw $manifest_result.Problem
    }

    $compatibility = Get-EditorModuleCompatibilityProblems $engine_root $manifest_result.Paths
    if ($compatibility.Problems.Count -gt 0) {
        $details = $compatibility.Problems -join [Environment]::NewLine
        throw "Editor module BuildIds remain incompatible after the forced rebuild:`n$details"
    }
}
