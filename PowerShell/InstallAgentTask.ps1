[CmdletBinding()]
param(
    [string]$InstallRoot,
    [string]$LinkDirectory,
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
. "$PSScriptRoot/../tools/install/ToolLinks.ps1"
$iojRoot = Get-IojRoot
if (-not $InstallRoot) { $InstallRoot = Join-Path $iojRoot 'tools/agent-task' }
$install_root = [IO.Path]::GetFullPath($InstallRoot)
$installed_tool = Join-Path $install_root 'bin\agent-task.exe'
$links = Get-ToolLinkDirectory $install_root $LinkDirectory
Assert-ToolLinkSupport $links

$worktreeName = Split-Path -Leaf ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')))
$temporaryDirectory = Join-Path $iojRoot 'tmp' $worktreeName
New-Item -ItemType Directory -Path $temporaryDirectory -Force | Out-Null
$previousTmp = $env:TMP
$previousTemp = $env:TEMP

# Run Cargo inside the workspace so rustup selects its pinned toolchain.
Push-Location -LiteralPath (Join-Path $PSScriptRoot '..\tools\rust')
try {
    $env:TMP = $temporaryDirectory
    $env:TEMP = $temporaryDirectory
    if (-not $SkipTests) {
        & cargo test --package agent-task --locked
        if ($LASTEXITCODE -ne 0) {
            throw "agent-task tests exited with code $LASTEXITCODE."
        }
    }

    & cargo install --path crates/agent-task --root $install_root --locked --force
    if ($LASTEXITCODE -ne 0) {
        throw "agent-task installation exited with code $LASTEXITCODE."
    }

    & $installed_tool --version
    if ($LASTEXITCODE -ne 0) {
        throw "Installed agent-task smoke test exited with code $LASTEXITCODE."
    }
} finally {
    $env:TMP = $previousTmp
    $env:TEMP = $previousTemp
    Pop-Location
}

Write-Host "`nInstalled agent-task:`n  $installed_tool"
Publish-ToolLinks (Split-Path -Parent $installed_tool) @('agent-task.exe') $links
Write-Host "`nOn a fresh setup, install the shared project tools with:`n  agent-task install-central-tools"
Write-Host "`nPrepare tasks with 'agent-task prepare-worktree'; use 'agent-task git <args...>' for feature work."
Write-Host "After validation and explicit user authorization, run 'agent-task integrate' for dev integration."
Write-Host "`nRun 'agent-task --help' to see the available commands."
