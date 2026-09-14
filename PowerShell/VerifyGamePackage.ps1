[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$ProjectRoot,

    [Parameter(Mandatory)]
    [string]$PackageRoot,

    [Parameter(Mandatory)]
    [string]$UnrealPak,

    [Parameter(Mandatory)]
    [string]$VerificationDirectory,

    [Parameter(Mandatory)]
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$project_root = [System.IO.Path]::GetFullPath($ProjectRoot)
$package_root = [System.IO.Path]::GetFullPath($PackageRoot)
$verification_directory = [System.IO.Path]::GetFullPath($VerificationDirectory)

function Assert-FileExists {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required package file is missing: $Path"
    }
}

function ConvertTo-UnrealPackagePath {
    param(
        [Parameter(Mandatory)]
        [System.IO.FileInfo]$File
    )

    $content_directory = $File.Directory
    while ($null -ne $content_directory -and $content_directory.Name -ne 'Content') {
        $content_directory = $content_directory.Parent
    }
    if ($null -eq $content_directory) {
        throw "Asset is not below a Content directory: $($File.FullName)"
    }

    $relative_path = [System.IO.Path]::GetRelativePath(
        $content_directory.FullName,
        $File.FullName)
    $relative_path = $relative_path.Substring(
        0,
        $relative_path.Length - $File.Extension.Length).Replace('\', '/')

    if ($content_directory.FullName -eq (Join-Path $project_root 'Content')) {
        return "/Game/$relative_path"
    }

    $plugin_descriptor = Get-ChildItem -LiteralPath $content_directory.Parent.FullName `
        -Filter '*.uplugin' -File | Select-Object -First 1
    if ($null -eq $plugin_descriptor) {
        throw "Could not find the plugin descriptor for '$($File.FullName)'."
    }

    "/$($plugin_descriptor.BaseName)/$relative_path"
}

function Test-InventoryContains {
    param(
        [Parameter(Mandatory)]
        [string]$Value
    )

    $script:inventory.IndexOf($Value, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
}

function ConvertTo-StagedAssetPath {
    param(
        [Parameter(Mandatory)]
        [string]$PackagePath
    )

    $parts = $PackagePath.Trim('/').Split('/', 2)
    if ($parts.Count -ne 2) {
        throw "Invalid Unreal package path: $PackagePath"
    }

    if ($parts[0] -eq 'Game') {
        return "../../../Sandbox/Content/$($parts[1])"
    }

    "../../../Sandbox/Plugins/$($parts[0])/Content/$($parts[1])"
}

function Assert-PackagePresent {
    param(
        [Parameter(Mandatory)]
        [string]$PackagePath
    )

    $staged_path = ConvertTo-StagedAssetPath $PackagePath
    if (-not (Test-InventoryContains $PackagePath) -and
        -not (Test-InventoryContains $staged_path)) {
        throw "Required Unreal package is missing from the containers: $PackagePath"
    }
}

Assert-FileExists (Join-Path $package_root 'Sandbox.exe')
$game_binary_name = if ($Configuration -eq 'Development') {
    'Sandbox.exe'
} else {
    "Sandbox-Win64-$Configuration.exe"
}
Assert-FileExists (Join-Path $package_root "Sandbox\Binaries\Win64\$game_binary_name")
Assert-FileExists (Join-Path $package_root 'Engine\Extras\Redist\en-us\vc_redist.x64.exe')
Assert-FileExists $UnrealPak

$pak_directory = Join-Path $package_root 'Sandbox\Content\Paks'
$pak_files = @(Get-ChildItem -LiteralPath $pak_directory -Filter '*.pak' -File)
$utoc_files = @(Get-ChildItem -LiteralPath $pak_directory -Filter '*.utoc' -File)
if ($pak_files.Count -eq 0) {
    throw "No pak files were found under '$pak_directory'."
}
if ($utoc_files.Count -eq 0) {
    throw "No IoStore containers were found under '$pak_directory'."
}

$null = New-Item -ItemType Directory -Path $verification_directory -Force
$pak_inventory_path = Join-Path $verification_directory 'pak-files.txt'
$io_store_inventory_path = Join-Path $verification_directory 'iostore.csv'
if (Test-Path -LiteralPath $io_store_inventory_path -PathType Leaf) {
    Remove-Item -LiteralPath $io_store_inventory_path -Force
}

$pak_inventory_lines = foreach ($pak_file in $pak_files) {
    & $UnrealPak $pak_file.FullName -List 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "UnrealPak failed to list '$($pak_file.FullName)' with code $LASTEXITCODE."
    }
}
$pak_inventory_lines | Set-Content -LiteralPath $pak_inventory_path -Encoding utf8

$utoc_pattern = Join-Path $pak_directory '*.utoc'
& $UnrealPak "-ListContainer=$utoc_pattern" "-Csv=$io_store_inventory_path"
if ($LASTEXITCODE -ne 0) {
    throw "UnrealPak failed to list the IoStore containers with code $LASTEXITCODE."
}
Assert-FileExists $io_store_inventory_path

$script:inventory = (($pak_inventory_lines -join "`n") + "`n" +
    (Get-Content -LiteralPath $io_store_inventory_path -Raw)).Replace('\', '/')

$level_scripts = @(Get-ChildItem -LiteralPath (Join-Path $project_root 'LevelScripts') `
    -Filter '*.scm' -File -Recurse)
foreach ($level_script in $level_scripts) {
    $relative_path = [System.IO.Path]::GetRelativePath(
        (Join-Path $project_root 'LevelScripts'),
        $level_script.FullName).Replace('\', '/')
    if (-not (Test-InventoryContains "LevelScripts/$relative_path")) {
        throw "Required level script is missing from the pak: $relative_path"
    }
}

$required_map_paths = @(
    '/SpaceGame/Levels/MainMenu',
    '/SpaceGame/Levels/GameRuntime'
)
foreach ($required_map_path in $required_map_paths) {
    Assert-PackagePresent $required_map_path
}
Assert-PackagePresent '/Game/UI/DA_ui_data'

$map_files = @(
    Get-ChildItem -LiteralPath (Join-Path $project_root 'Content') -Filter '*.umap' -File -Recurse
    Get-ChildItem -LiteralPath (Join-Path $project_root 'Plugins') -Filter '*.umap' -File -Recurse
)
foreach ($map_file in $map_files) {
    $package_path = ConvertTo-UnrealPackagePath $map_file
    if ($package_path -in $required_map_paths) {
        continue
    }
    if ((Test-InventoryContains $package_path) -or
        (Test-InventoryContains (ConvertTo-StagedAssetPath $package_path))) {
        throw "Unexpected project map was packaged: $package_path"
    }
}

$required_asset_directories = @(
    'Plugins\SpaceGame\Content\UI',
    'Plugins\SpaceGame\Content\Input',
    'Plugins\SandboxShaders\Content\GpuStarfield',
    'Plugins\SandboxShaders\Content\CelestialBackdrop'
)
foreach ($relative_directory in $required_asset_directories) {
    $directory = Join-Path $project_root $relative_directory
    foreach ($asset in Get-ChildItem -LiteralPath $directory -Filter '*.uasset' -File -Recurse) {
        Assert-PackagePresent (ConvertTo-UnrealPackagePath $asset)
    }
}

$audio_directory = Join-Path $project_root 'Plugins\SpaceGame\Content\Audio\Generated'
if (Test-Path -LiteralPath $audio_directory -PathType Container) {
    foreach ($asset in Get-ChildItem -LiteralPath $audio_directory -Filter '*.uasset' -File -Recurse) {
        Assert-PackagePresent (ConvertTo-UnrealPackagePath $asset)
    }
}

Write-Host "Verified $($level_scripts.Count) level scripts, two maps, runtime-loaded assets, binaries, containers, and prerequisites."
