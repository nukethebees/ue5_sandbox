[CmdletBinding()]
param(
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'NukeTheBees/agent-codex'),
    [ValidateSet('ReleaseNoLTO', 'Release', 'Debug')][string]$Configuration
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $PSBoundParameters.ContainsKey('Configuration')) {
    $Configuration = python "$PSScriptRoot/codex_build_profile.py"
    if ($LASTEXITCODE -ne 0) { throw 'Could not read the Codex build profile from ioj.toml.' }
}
Write-Host "Codex build configuration: $Configuration"
$root = [IO.Path]::GetFullPath($InstallRoot)
$build = Join-Path $repo ".local/scheduler-install-$Configuration"
$candidate = Join-Path $root ".candidate-$([Guid]::NewGuid().ToString('N'))"
$backup = Join-Path $root ".previous-$([Guid]::NewGuid().ToString('N'))"
$bin = Join-Path $root 'bin'

function Remove-PrivateOutput([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Private output escaped installation root: $resolved"
    }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}

try {
    pwsh -NoProfile -File "$repo/tools/agent-scheduler/Prepare-Upstream.ps1"
    if ($LASTEXITCODE -ne 0) { throw 'Pinned Codex preparation failed.' }
    cmake -S "$repo/tools/agent-scheduler" -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration"
    if ($LASTEXITCODE -ne 0) { throw 'Scheduler configuration failed.' }
    cmake --build $build --target scheduler-client codex-scheduler
    if ($LASTEXITCODE -ne 0) { throw 'Scheduler/Codex build failed.' }
    cmake --install $build --prefix $candidate
    if ($LASTEXITCODE -ne 0) { throw 'Scheduler/Codex candidate install failed.' }

    pwsh -NoProfile -File "$candidate/bin/agent-codex.ps1" --version
    if ($LASTEXITCODE -ne 0) { throw 'Scheduler/Codex launcher smoke test failed.' }

    if (Test-Path -LiteralPath $bin) { [IO.Directory]::Move($bin, $backup) }
    try {
        [IO.Directory]::Move((Join-Path $candidate 'bin'), $bin)
    } catch {
        if (Test-Path -LiteralPath $backup) { [IO.Directory]::Move($backup, $bin) }
        throw
    }
    Remove-PrivateOutput $backup
    Write-Host "Installed scheduler-enabled Codex. Add '$bin' to PATH and launch agent-codex.ps1. Normal codex is unchanged."
} finally {
    Remove-PrivateOutput $candidate
}
