[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$agent_task_root = Join-Path $env:LOCALAPPDATA 'NukeTheBees\agent-task'

# Run Cargo inside the workspace so rustup selects its pinned toolchain.
Push-Location -LiteralPath (Join-Path $PSScriptRoot 'tools\rust')
try {
    Write-Host '[1/3] Installing agent-task'
    & cargo install --path crates/agent-task --root $agent_task_root --locked --force
    if ($LASTEXITCODE -ne 0) {
        throw "agent-task installation exited with code $LASTEXITCODE."
    }

    Set-Location -LiteralPath $PSScriptRoot
    . .\dev.ps1

    Write-Host '[2/3] Installing trusted agent-git'
    install-agent-git

    Write-Host '[3/3] Installing canonical jobserver'
    & cmake --preset native
    if ($LASTEXITCODE -ne 0) {
        throw "Native configuration exited with code $LASTEXITCODE."
    }
    & cmake --build --preset native --target install-jobserver
    if ($LASTEXITCODE -ne 0) {
        throw "Jobserver installation exited with code $LASTEXITCODE."
    }

    Write-Host "Developer tools installed. Ensure '$(Join-Path $agent_task_root 'bin')' is on PATH."
} finally {
    Pop-Location
}
