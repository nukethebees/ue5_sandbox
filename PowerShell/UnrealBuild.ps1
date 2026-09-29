# The cooperative jobs board does not reap MSBuild workers from interactive Unreal workflows.
$env:MSBUILDDISABLENODEREUSE = '1'

function Invoke-CMakeWorkflow {
    param(
        [Parameter(Mandatory)]
        [string]$Name,
        [Parameter(Mandatory)]
        [string]$Preset
    )

    $log_directory = Join-Path $script:dev_project_root '.local\logs'
    New-Item -ItemType Directory -Path $log_directory -Force | Out-Null
    $log_path = Join-Path $log_directory "cmake-workflow-$Preset.log"

    Write-Host $Name
    & cmake --workflow --preset $Preset 2>&1 | Tee-Object -FilePath $log_path | Out-Host
    $exit_code = $LASTEXITCODE

    [PSCustomObject]@{
        ExitCode = $exit_code
        LogPath = $log_path
    }
}

function Get-WorkflowFailureMessage {
    param(
        [Parameter(Mandatory)]
        [string]$workflow,
        [Parameter(Mandatory)]
        [int]$exit_code,
        [Parameter(Mandatory)]
        [string]$log_path
    )

    $tail = if (Test-Path -LiteralPath $log_path -PathType Leaf) {
        (Get-Content -LiteralPath $log_path -Tail 25) -join [Environment]::NewLine
    } else {
        'No CMake workflow log was written.'
    }

    "CMake workflow '$workflow' exited with code $exit_code.`n" +
    "Last CMake output:`n$tail`n" +
    "Full workflow output: $log_path"
}

function Write-GeneratedSourceWarning {
    param(
        [Parameter(Mandatory)]
        [string]$log_path
    )

    if (-not (Test-Path -LiteralPath $log_path -PathType Leaf)) {
        return
    }

    $updated_files = @(
        Select-String -LiteralPath $log_path -Pattern '^(?:Updated|Wrote) (?<path>.+)$' |
        ForEach-Object { $_.Matches[0].Groups['path'].Value } |
        Select-Object -Unique
    )
    if ($updated_files.Count -eq 0) {
        return
    }

    $message = "Generated committed source files were updated. Review and commit them:`n" +
        ($updated_files | ForEach-Object { "  $_" } | Join-String -Separator [Environment]::NewLine)
    Add-Content -LiteralPath $log_path -Value "`nWARNING: $message"
    Write-Warning $message
}

function cbuild {
    param(
        [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
        [ValidateSet('native-tests', 'native-core-tests', 'native-simulation-tests', 'tool-tests', 'debug', 'debug-game', 'development', 'shipping', 'test')]
        [string[]]$configuration = @('native-tests')
    )

    Push-Location -LiteralPath $script:dev_project_root
    try {
        foreach ($current_configuration in $configuration) {
            Write-Host "Building the project with CMake workflow '$current_configuration'."
            $workflow_result = Invoke-CMakeWorkflow `
                -Name "CMake workflow: $current_configuration" `
                -Preset $current_configuration

            if ($workflow_result.ExitCode -ne 0) {
                throw (Get-WorkflowFailureMessage `
                    -workflow $current_configuration `
                    -exit_code $workflow_result.ExitCode `
                    -log_path $workflow_result.LogPath)
            }
        }
    } finally {
        Pop-Location
    }
}

function integrate-feature {
    param([switch]$KeepBranch)

    $agent_task = (Get-Command agent-task -CommandType Application -ErrorAction Stop).Source
    $arguments = @('integrate')
    if ($KeepBranch) { $arguments += '--keep-branch' }
    & $agent_task @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Feature integration stopped (exit code $LASTEXITCODE). Follow the stage diagnostic above; nothing is retried automatically."
    }
}

function csetup {
    param(
        [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
        [ValidateSet('all', 'debug-game', 'development')]
        [string[]]$configuration = @('all')
    )

    if ($configuration -contains 'all' -and $configuration.Count -ne 1) {
        throw "Configuration 'all' cannot be combined with explicit configurations."
    }

    $configurations = if ($configuration -contains 'all') {
        @('debug-game', 'development')
    } else {
        @($configuration | Select-Object -Unique)
    }

    Push-Location -LiteralPath $script:dev_project_root
    try {
        foreach ($current_configuration in $configurations) {
            $workflow = "setup-worktree-$current_configuration"
            Write-Host "Preparing worktree with CMake workflow '$workflow'."
            $workflow_result = Invoke-CMakeWorkflow `
                -Name "CMake setup workflow: $current_configuration" `
                -Preset $workflow

            if ($workflow_result.ExitCode -ne 0) {
                throw (Get-WorkflowFailureMessage `
                    -workflow $workflow `
                    -exit_code $workflow_result.ExitCode `
                    -log_path $workflow_result.LogPath)
            }

            Write-GeneratedSourceWarning -log_path $workflow_result.LogPath
        }
    } finally {
        Pop-Location
    }
}

function cplay {
    param(
        [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
        [ValidateSet('debug-game', 'development')]
        [string[]]$configuration = @('debug-game', 'development')
    )

    $configurations = @($configuration | Select-Object -Unique)

    csetup -configuration $configurations
    cbuild -configuration $configurations
}

function cprojectfiles {
    param(
        [ValidateSet('debug-game', 'development')]
        [string]$configuration = 'debug-game'
    )

    Push-Location -LiteralPath $script:dev_project_root
    try {
        Write-Host "Regenerating Unreal project files with CMake preset '$configuration'."
        & cmake --build --preset $configuration --target generate-project-files

        if ($LASTEXITCODE -ne 0) {
            throw "Project-file generation for preset '$configuration' exited with code $LASTEXITCODE. Run 'csetup $configuration' first if the build tree has not been configured."
        }
    } finally {
        Pop-Location
    }
}

function get-jobserver-state {
    & agent-task jobs status
    if ($LASTEXITCODE -ne 0) {
        throw "Jobserver status exited with code $LASTEXITCODE."
    }
}
