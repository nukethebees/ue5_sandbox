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

function Stop-UbtProcessTree {
    param(
        [Parameter(Mandatory)]
        [uint32]$process_id,
        [Parameter(Mandatory)]
        [object[]]$processes
    )

    $children = @($processes | Where-Object { $_.ParentProcessId -eq $process_id })
    foreach ($child in $children) {
        Stop-UbtProcessTree ([uint32]$child.ProcessId) $processes
    }

    Stop-Process -Id $process_id -Force -ErrorAction SilentlyContinue
}

function reset-ubt-build-state {
    [CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'High')]
    param(
        [switch]$Force
    )

    $engine_root = Get-UbtEngineRoot
    $processes = Get-UbtProcessSnapshot
    $active_roots = @($processes | Where-Object {
            (Test-SandboxCMakeProcess $_) -or (Test-UbtProcess $_ $engine_root)
        })

    if ($active_roots.Count -gt 0 -and -not $Force) {
        $active_ids = $active_roots.ProcessId -join ', '
        throw "Active Sandbox CMake or UBT processes were found ($active_ids). Wait for them to finish or rerun with -Force."
    }

    if (-not $PSCmdlet.ShouldProcess($engine_root, 'Reset UE build processes')) {
        return
    }

    if ($Force) {
        foreach ($process in $active_roots) {
            Stop-UbtProcessTree ([uint32]$process.ProcessId) $processes
        }
    }

    $dotnet_path = Join-Path $engine_root 'Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe'
    & $dotnet_path build-server shutdown --msbuild
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "The UE-bundled build-server shutdown exited with code $LASTEXITCODE."
    }

    $remaining_processes = Get-UbtProcessSnapshot
    $remaining_ids = [System.Collections.Generic.HashSet[uint32]]::new()
    $remaining_processes.ProcessId | ForEach-Object { $null = $remaining_ids.Add([uint32]$_) }
    $orphaned_workers = @($remaining_processes | Where-Object {
            (Test-UbtMsBuildWorker $_ $engine_root) -and
            -not $remaining_ids.Contains([uint32]$_.ParentProcessId)
        })

    foreach ($worker in $orphaned_workers) {
        Stop-Process -Id $worker.ProcessId -Force -ErrorAction SilentlyContinue
    }

    Write-Host "Stopped $($orphaned_workers.Count) orphaned UE MSBuild worker(s)."
}

function cbuild {
    param(
        [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
        [ValidateSet('native-tests', 'native-core-tests', 'native-simulation-tests', 'debug', 'debug-game', 'development', 'shipping', 'test')]
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

    $staged_tools_directory = Join-Path $script:dev_project_root 'tools\bin'

    Push-Location -LiteralPath $script:dev_project_root
    try {
        if (Test-Path -LiteralPath $staged_tools_directory -PathType Container) {
            Write-Host 'Clearing staged C# developer tools.'
            Remove-Item -LiteralPath $staged_tools_directory -Recurse -Force -ErrorAction Stop
        }

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

    $installer = Join-Path $script:dev_project_root 'PowerShell\Install-AgentGit.ps1'
    & $installer -Repository $script:dev_project_root -BaseBranch $BaseBranch
    if ($LASTEXITCODE -ne 0) {
        throw "agent-git installation exited with code $LASTEXITCODE."
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
        if ($requires_unreal_setup.Count -gt 0) {
            ctools
        }

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
