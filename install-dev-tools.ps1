[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$agent_task_root = Join-Path $env:LOCALAPPDATA 'NukeTheBees\agent-task'
$results = [System.Collections.Generic.List[object]]::new()

function Invoke-InstallStage {
    param([string]$Name, [string]$Executable, [scriptblock]$Install)

    $result = [pscustomobject]@{
        Name = $Name
        Executable = $Executable
        Error = $null
        Output = [System.Collections.Generic.List[string]]::new()
    }
    Write-Host "Installing $Name"

    try {
        & $Install $result 2>&1 | ForEach-Object {
            $result.Output.Add($_.ToString())
            Write-Host $_
        }
    } catch {
        $result.Error = $_.Exception.Message
    }
    $results.Add($result)
}

Push-Location -LiteralPath $PSScriptRoot
try {
    Invoke-InstallStage 'agent-task' (Join-Path $agent_task_root 'bin\agent-task.exe') {
        param($result)

        # Run Cargo inside the workspace so rustup selects its pinned toolchain.
        Push-Location -LiteralPath (Join-Path $PSScriptRoot 'tools\rust')
        try {
            & cargo install --path crates/agent-task --root $agent_task_root --locked --force
            if ($LASTEXITCODE -ne 0) {
                $result.Error = "Cargo installation exited with code $LASTEXITCODE."
            }
        } finally {
            Pop-Location
        }
    }

    Invoke-InstallStage 'agent-git' (Join-Path $env:LOCALAPPDATA 'NukeTheBees\agent-git\bin\agent-git.exe') {
        . .\dev.ps1
        install-agent-git
    }

    Invoke-InstallStage 'jobserver' (Join-Path $env:LOCALAPPDATA 'NukeTheBees\jobserver\bin\jobserver.exe') {
        param($result)

        & cmake --preset native
        if ($LASTEXITCODE -ne 0) {
            $result.Error = "Native configuration exited with code $LASTEXITCODE."
            return
        }
        & cmake --build --preset native --target install-jobserver
        if ($LASTEXITCODE -ne 0) {
            $result.Error = "Jobserver installation exited with code $LASTEXITCODE."
        }
    }
} finally {
    Pop-Location
}

Write-Host "`nDeveloper tool installation summary"
foreach ($result in $results) {
    if ($result.Error) {
        Write-Host "  FAIL  $($result.Name)" -ForegroundColor Red
        Write-Host "        Target: $($result.Executable)"
    } else {
        Write-Host "  PASS  $($result.Name)" -ForegroundColor Green
        Write-Host "        Installed: $($result.Executable)"
        Write-Host "        PATH directory: $(Split-Path -Parent $result.Executable)"
    }
}

$failures = @($results | Where-Object { $_.Error })
if ($failures.Count -gt 0) {
    Write-Host "`nErrors" -ForegroundColor Red
    foreach ($failure in $failures) {
        Write-Host "  $($failure.Name): $($failure.Error)" -ForegroundColor Red
        if ($failure.Output.Count -gt 0) {
            Write-Host '  Last output:'
            $failure.Output | Select-Object -Last 12 | ForEach-Object { Write-Host "    $_" }
        }
    }
    exit 1
}
exit 0
