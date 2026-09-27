function install-agent-task {
    $ErrorActionPreference = 'Stop'
    $PSNativeCommandUseErrorActionPreference = $false
    $install_root = Join-Path $env:LOCALAPPDATA 'NukeTheBees\agent-task'
    $installed_tool = Join-Path $install_root 'bin\agent-task.exe'

    # Run Cargo inside the workspace so rustup selects its pinned toolchain.
    Push-Location -LiteralPath (Join-Path $PSScriptRoot '..\tools\rust')
    try {
        & cargo test --package agent-task --locked
        if ($LASTEXITCODE -ne 0) {
            throw "agent-task tests exited with code $LASTEXITCODE."
        }

        & cargo install --path crates/agent-task --root $install_root --locked --force
        if ($LASTEXITCODE -ne 0) {
            throw "agent-task installation exited with code $LASTEXITCODE."
        }

        & $installed_tool --help
        if ($LASTEXITCODE -ne 0) {
            throw "Installed agent-task smoke test exited with code $LASTEXITCODE."
        }
    } finally {
        Pop-Location
    }

    Write-Host "`nInstalled agent-task:`n  $installed_tool"
    Write-Host "`nThe maintainer manages PATH. Ensure '$(Split-Path -Parent $installed_tool)' is on PATH."
    Write-Host "`nOn a fresh setup, install the shared project tools with:`n  agent-task install-central-tools"
    Write-Host "`nPrepare tasks with 'agent-task prepare-worktree'; use 'agent-task git <args...>' for feature work."
    Write-Host "After validation and explicit user authorization, run 'integrate-feature' for dev integration."
    Write-Host "`nRun 'agent-task --help' to see the available commands."
}
