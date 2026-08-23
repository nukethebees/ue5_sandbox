$env:MSBUILDDISABLENODEREUSE = '1'

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
        $process.CommandLine -match '--preset\s+(debug-game|debug-game-unit-tests|debug-game-tests|debug-game-level-tests|generate-project-files|resave-assets)'
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

enable-ubt-build-safety
