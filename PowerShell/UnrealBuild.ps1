$env:MSBUILDDISABLENODEREUSE = '1'

function Get-JobserverPath {
    Join-Path $env:LOCALAPPDATA 'NukeTheBees\jobserver\bin\jobserver.exe'
}

function Invoke-JobserverWorkflow {
    param(
        [Parameter(Mandatory)]
        [string]$Name,
        [Parameter(Mandatory)]
        [string]$Preset
    )

    $jobserver = Get-JobserverPath
    if (-not (Test-Path -LiteralPath $jobserver -PathType Leaf)) {
        throw "The per-user jobserver is not installed. Run 'csetup' first."
    }

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

function Update-WorktreeSubmodules {
    Write-Host 'Synchronizing Git submodule URLs.'
    & git submodule sync --recursive
    if ($LASTEXITCODE -ne 0) {
        throw "Git submodule synchronization exited with code $LASTEXITCODE."
    }

    Write-Host 'Initializing pinned Git submodules.'
    & git submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) {
        throw "Git submodule update exited with code $LASTEXITCODE."
    }
}

function Remove-RetiredVcpkgBuildDirectory {
    param(
        [Parameter(Mandatory)]
        [string]$configuration
    )

    $manifest_path = Join-Path $script:dev_project_root 'vcpkg.json'
    if (Test-Path -LiteralPath $manifest_path -PathType Leaf) {
        return
    }

    $build_directory = Join-Path $script:dev_project_root "out\build\$configuration"
    $cmake_files_directory = Join-Path $build_directory 'CMakeFiles'
    if (-not (Test-Path -LiteralPath $cmake_files_directory -PathType Container)) {
        return
    }

    $system_files = Get-ChildItem -LiteralPath $cmake_files_directory -Filter 'CMakeSystem.cmake' -Recurse -File
    $uses_retired_vcpkg = $system_files | Where-Object {
        Select-String -LiteralPath $_.FullName -SimpleMatch 'vcpkg/scripts/buildsystems/vcpkg.cmake' -Quiet
    }

    if ($null -eq $uses_retired_vcpkg) {
        return
    }

    Write-Host "Removing obsolete vcpkg CMake build directory: $build_directory"
    Remove-Item -LiteralPath $build_directory -Recurse -Force
}

function Get-UbtEngineRoot {
    if ([string]::IsNullOrWhiteSpace($env:UE_ROOT)) {
        throw 'UE_ROOT is not set. Set it to the Unreal Engine installation root.'
    }

    $engine_root = [System.IO.Path]::GetFullPath($env:UE_ROOT)
    $dotnet_path = Join-Path $engine_root 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'

    if (-not (Test-Path -LiteralPath $dotnet_path -PathType Leaf)) {
        throw "UE_ROOT '$engine_root' does not contain the expected bundled .NET executable: $dotnet_path"
    }

    $engine_root
}

function Get-UbtProcessSnapshot {
    try {
        @(Get-CimInstance Win32_Process -ErrorAction Stop)
    } catch {
        throw "Unable to inspect Windows processes: $($_.Exception.Message)"
    }
}

function Test-SandboxCMakeProcess {
    param(
        [Parameter(Mandatory)]
        $process
    )

    $process.Name -eq 'cmake.exe' -and
        $process.CommandLine -match '--preset\s+(debug-game|debug-game-unit-tests|debug-game-tests|debug-game-level-tests|generate-project-files|resave-assets|setup-worktree-(debug-game|development))'
}

function Test-UbtProcess {
    param(
        [Parameter(Mandatory)]
        $process,
        [Parameter(Mandatory)]
        [string]$engine_root
    )

    $escaped_engine_root = [regex]::Escape($engine_root.TrimEnd('\'))
    $process.Name -eq 'dotnet.exe' -and
        $process.CommandLine -match "^`"?$escaped_engine_root\\Engine\\" -and
        $process.CommandLine -match 'UnrealBuildTool(?:\.dll)?'
}

function Test-UbtMsBuildWorker {
    param(
        [Parameter(Mandatory)]
        $process,
        [Parameter(Mandatory)]
        [string]$engine_root
    )

    $escaped_engine_root = [regex]::Escape($engine_root.TrimEnd('\'))
    $process.Name -eq 'dotnet.exe' -and
        $process.CommandLine -match "^`"?$escaped_engine_root\\Engine\\" -and
        $process.CommandLine -match 'MSBuild\.dll' -and
        $process.CommandLine -match '/nodemode:1'
}

function ConvertTo-UbtProcessState {
    param(
        [Parameter(Mandatory)]
        $process,
        [Parameter(Mandatory)]
        [System.Collections.Generic.HashSet[uint32]]$process_ids,
        [Parameter(Mandatory)]
        [string]$kind
    )

    [PSCustomObject]@{
        Kind = $kind
        ProcessId = $process.ProcessId
        ParentProcessId = $process.ParentProcessId
        ParentAlive = $process_ids.Contains([uint32]$process.ParentProcessId)
        Created = $process.CreationDate
        CommandLine = $process.CommandLine
    }
}

function enable-ubt-build-safety {
    $value = '1'
    $env:MSBUILDDISABLENODEREUSE = $value
    $persisted_value = [Environment]::GetEnvironmentVariable(
        'MSBUILDDISABLENODEREUSE',
        [EnvironmentVariableTarget]::User)

    if ($persisted_value -ne $value) {
        [Environment]::SetEnvironmentVariable(
            'MSBUILDDISABLENODEREUSE',
            $value,
            [EnvironmentVariableTarget]::User)

        Write-Host 'Persisted MSBUILDDISABLENODEREUSE=1 for the current user.'
        Write-Host 'Sign out and back in before using Explorer or already-running development tools.'
    }
}

function get-ubt-build-state {
    $engine_root = Get-UbtEngineRoot
    $processes = Get-UbtProcessSnapshot
    $process_ids = [System.Collections.Generic.HashSet[uint32]]::new()
    $processes.ProcessId | ForEach-Object { $null = $process_ids.Add([uint32]$_) }

    foreach ($process in $processes) {
        if (Test-SandboxCMakeProcess $process) {
            ConvertTo-UbtProcessState $process $process_ids 'CMake workflow'
        } elseif (Test-UbtProcess $process $engine_root) {
            ConvertTo-UbtProcessState $process $process_ids 'UnrealBuildTool'
        } elseif (Test-UbtMsBuildWorker $process $engine_root) {
            ConvertTo-UbtProcessState $process $process_ids 'UE MSBuild worker'
        }
    }
}

function reset-ubt-build-state {
    $jobserver = Get-JobserverPath
    if (-not (Test-Path -LiteralPath $jobserver -PathType Leaf)) {
        throw "The per-user jobserver is not installed. Run 'csetup' first."
    }

    & $jobserver kill-owned --kind unreal-build
    if ($LASTEXITCODE -ne 0) {
        throw "Jobserver owned Unreal-build cleanup exited with code $LASTEXITCODE."
    }
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
            $workflow_result = Invoke-JobserverWorkflow `
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

function ctools {
    $tools_solution = Join-Path $script:dev_project_root 'tools\Tools.slnx'
    if (-not (Test-Path -LiteralPath $tools_solution -PathType Leaf)) {
        throw "C# tools solution was not found: $tools_solution"
    }

    Push-Location -LiteralPath $script:dev_project_root
    try {
        Write-Host 'Building standalone C# developer tools.'
        & dotnet build $tools_solution -m:1

        if ($LASTEXITCODE -ne 0) {
            throw "C# tools build exited with code $LASTEXITCODE."
        }
    } finally {
        Pop-Location
    }
}

function install-agent-git {
    param(
        [string]$BaseBranch = 'dev'
    )

    $installer_project = Join-Path $script:dev_project_root 'tools\AgentGitInstaller\AgentGitInstaller.csproj'
    if (-not (Test-Path -LiteralPath $installer_project -PathType Leaf)) {
        throw "The AgentGit installer project was not found: '$installer_project'."
    }

    $dotnet_command = Get-Command dotnet.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1
    $bootstrap_root = [System.IO.Directory]::CreateTempSubdirectory('AgentGitInstaller-').FullName
    try {
        $bootstrap_output = Join-Path $bootstrap_root 'bin'
        $bootstrap_artifacts = Join-Path $bootstrap_root 'artifacts'
        $private_stage = Join-Path $bootstrap_root 'standalone-tools'
        $stage_property = "-p:StandaloneToolsBinDirectory=$private_stage$([System.IO.Path]::DirectorySeparatorChar)"

        Write-Host 'Building the AgentGit installer into a private bootstrap directory.'
        & $dotnet_command.Source publish $installer_project `
            --configuration Release `
            --artifacts-path $bootstrap_artifacts `
            --output $bootstrap_output `
            $stage_property `
            -m:1 -nr:false
        if ($LASTEXITCODE -ne 0) {
            throw "AgentGit installer bootstrap build exited with code $LASTEXITCODE."
        }

        $installer = Join-Path $bootstrap_output 'agent-git-installer.exe'
        if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
            throw "The privately built AgentGit installer was not found: '$installer'."
        }

        & $installer `
            --source-root $script:dev_project_root `
            --repository $script:dev_project_root `
            --base-branch $BaseBranch
        if ($LASTEXITCODE -ne 0) {
            throw "agent-git installation exited with code $LASTEXITCODE."
        }
    } finally {
        if (Test-Path -LiteralPath $bootstrap_root) {
            Remove-Item -LiteralPath $bootstrap_root -Recurse -Force
        }
    }
}

function integrate-feature {
    param(
        [switch]$KeepBranch,
        [switch]$ToolTests,
        [switch]$MaintainerOverride,
        [string]$OverrideReason
    )

    $agent_git = Join-Path $env:LOCALAPPDATA 'NukeTheBees\agent-git\bin\agent-git.exe'
    $jobserver = Get-JobserverPath
    if (-not (Test-Path -LiteralPath $agent_git -PathType Leaf)) {
        throw "The trusted agent-git executable is not installed. Run 'install-agent-git' first."
    }
    if (-not (Test-Path -LiteralPath $jobserver -PathType Leaf)) {
        throw "The per-user jobserver is not installed. Run 'csetup' first."
    }

    if (($MaintainerOverride -and [string]::IsNullOrWhiteSpace($OverrideReason)) -or
        (-not $MaintainerOverride -and -not [string]::IsNullOrWhiteSpace($OverrideReason))) {
        throw '-MaintainerOverride requires one non-empty -OverrideReason and reasons are accepted only with the override.'
    }

    $integration_info_json = & $agent_git integration-info --json 2>$null
    $supports_integration_info = $LASTEXITCODE -eq 0
    if ($supports_integration_info) {
        try {
            $integration_info = $integration_info_json | ConvertFrom-Json -ErrorAction Stop
        } catch {
            throw 'agent-git returned invalid integration candidate metadata.'
        }
        $worktree = $integration_info.worktree
        $branch = $integration_info.branch
        $base_branch = $integration_info.baseBranch
        $candidate = $integration_info.patchFingerprint
    } else {
        Write-Warning 'Installed agent-git predates stable candidate metadata; using compatibility inspection.'
        $status = & $agent_git status
        if ($LASTEXITCODE -ne 0) {
            throw 'Unable to inspect the current feature worktree with agent-git.'
        }
        $branch_info = & $agent_git branch-info
        if ($LASTEXITCODE -ne 0) {
            throw 'Unable to inspect the configured integration branch with agent-git.'
        }
        $worktree_line = $status | Where-Object { $_ -match '^Worktree: ' } | Select-Object -First 1
        $branch_line = $status | Where-Object { $_ -match '^Current branch: ' } | Select-Object -First 1
        $base_line = $branch_info | Where-Object { $_ -match '^Base branch: ' } | Select-Object -First 1
        if ($null -eq $worktree_line -or $null -eq $branch_line -or $null -eq $base_line) {
            throw 'agent-git returned incomplete compatibility metadata.'
        }
        $worktree = $worktree_line.Substring('Worktree: '.Length)
        $branch = $branch_line.Substring('Current branch: '.Length)
        $base_branch = $base_line.Substring('Base branch: '.Length)
        $candidate = 'legacy-candidate'
    }
    if ([string]::IsNullOrWhiteSpace($worktree) -or
        [string]::IsNullOrWhiteSpace($branch) -or
        [string]::IsNullOrWhiteSpace($base_branch) -or
        [string]::IsNullOrWhiteSpace($candidate)) {
        throw 'agent-git returned incomplete integration candidate metadata.'
    }

    if ($MaintainerOverride -and -not $supports_integration_info) {
        throw 'The installed agent-git does not support auditable maintainer overrides. Install the reviewed version or follow an explicitly authorized fallback.'
    }

    $candidate_short = if ($candidate.Length -gt 12) { $candidate.Substring(0, 12) } else { $candidate }
    $resource = "integration/$base_branch"
    $arguments = @(
        'lease',
        '--name', "Integrate $branch into $base_branch [$candidate_short]",
        '--kind', 'integration',
        '--task', "$branch candidate $candidate_short",
        '--worktree', $worktree,
        '--exclusive', $resource,
        '--',
        $agent_git,
        'integrate',
        '--authorized'
    )
    if ($KeepBranch) {
        $arguments += '--keep-branch'
    }
    if ($ToolTests) {
        $arguments += '--tool-tests'
    }
    if ($MaintainerOverride) {
        $arguments += @('--maintainer-override', '--override-reason', $OverrideReason)
    }

    Write-Host "Queueing '$branch' candidate $candidate for the exclusive '$resource' integration reservation."
    if ($MaintainerOverride) {
        Write-Warning "Maintainer override requested: $OverrideReason"
    }
    & $jobserver @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Feature integration stopped during its reported stage (exit code $LASTEXITCODE). Review the named blocker above; nothing is retried automatically."
    }
}

function csetup {
    param(
        [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
        [ValidateSet('all', 'native', 'debug-game', 'development')]
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
        Update-WorktreeSubmodules

        $requires_unreal_setup = @($configurations | Where-Object { $_ -ne 'native' })

        $preset_generator = Join-Path $script:dev_project_root 'cmake\presets\generate.py'
        if (-not (Test-Path -LiteralPath $preset_generator -PathType Leaf)) {
            throw "CMake preset generator was not found: $preset_generator"
        }

        Write-Host 'Generating CMake presets.'
        & python $preset_generator

        if ($LASTEXITCODE -ne 0) {
            throw "CMake preset generation exited with code $LASTEXITCODE."
        }

        foreach ($current_configuration in $requires_unreal_setup) {
            Remove-RetiredVcpkgBuildDirectory -configuration $current_configuration
        }

        $jobserver = Get-JobserverPath
        if (-not (Test-Path -LiteralPath $jobserver -PathType Leaf)) {
            Write-Host 'Bootstrapping the canonical per-user jobserver.'
            & cmake --preset native
            if ($LASTEXITCODE -ne 0) {
                throw "Jobserver bootstrap configuration exited with code $LASTEXITCODE."
            }
            & cmake --build --preset native --target install-jobserver
            if ($LASTEXITCODE -ne 0) {
                throw "Jobserver installation exited with code $LASTEXITCODE."
            }
        }

        foreach ($current_configuration in $configurations) {
            if ($current_configuration -eq 'native') {
                Write-Host 'Prepared native-only development prerequisites.'
                continue
            }

            $workflow = "setup-worktree-$current_configuration"
            Write-Host "Preparing worktree with CMake workflow '$workflow'."
            $workflow_result = Invoke-JobserverWorkflow `
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
    $jobserver = Get-JobserverPath
    & $jobserver status
    if ($LASTEXITCODE -ne 0) {
        throw "Jobserver status exited with code $LASTEXITCODE."
    }
}

enable-ubt-build-safety
