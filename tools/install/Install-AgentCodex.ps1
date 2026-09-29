[CmdletBinding()]
param(
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'NukeTheBees/agent-codex'),
    [ValidateSet('ReleaseNoLTO', 'Release', 'Debug')][string]$Configuration,
    [string]$LinkDirectory
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. "$PSScriptRoot/ToolLinks.ps1"
$links = Get-ToolLinkDirectory $InstallRoot $LinkDirectory
Assert-ToolLinkSupport $links
if (-not $Configuration) {
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
    cmake --build $build --target codex-scheduler
    if ($LASTEXITCODE -ne 0) { throw 'Scheduler/Codex build failed.' }
    cmake --install $build --prefix $candidate
    if ($LASTEXITCODE -ne 0) { throw 'Scheduler/Codex candidate install failed.' }

    & "$candidate/bin/codex-scheduler.exe" --version
    if ($LASTEXITCODE -ne 0) { throw 'Scheduler/Codex candidate smoke test failed.' }

    if (Test-Path -LiteralPath $bin) { [IO.Directory]::Move($bin, $backup) }
    try {
        [IO.Directory]::Move((Join-Path $candidate 'bin'), $bin)
    } catch {
        if (Test-Path -LiteralPath $backup) { [IO.Directory]::Move($backup, $bin) }
        throw
    }
    Publish-ToolLinks $bin @('agent-codex.ps1', 'codex-scheduler.exe', 'codex-code-mode-host.exe') $links
    foreach ($previous in Get-ChildItem -LiteralPath $root -Directory -Filter '.previous-*' -Force) {
        try {
            Remove-PrivateOutput $previous.FullName
        } catch {
            Write-Warning "Codex was installed successfully, but the previous installation at '$($previous.FullName)' could not be removed. A running Codex session may still be using it. Close old agent-codex sessions and rerun the installer to retry cleanup. Windows reported: $($_.Exception.Message)"
        }
    }
    Write-Host 'Installed scheduler-enabled Codex. Launch agent-codex.ps1. Normal codex is unchanged.'
    Write-Host "Reference rules: '$bin/scheduling.default.rules'. Keep active rules in '%LOCALAPPDATA%\NukeTheBees\config\agent-scheduler\scheduling.rules'."
} finally {
    Remove-PrivateOutput $candidate
}
