[CmdletBinding()]
param(
    [string]$RepositoryRoot,

    [string]$DotnetExecutable = 'dotnet'
)

function Get-BenchmarkToolsJobserverPath {
    param(
        [string]$JobserverExecutable
    )

    if (-not [string]::IsNullOrWhiteSpace($JobserverExecutable)) {
        return [IO.Path]::GetFullPath($JobserverExecutable)
    }

    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        throw 'LOCALAPPDATA is required to locate the per-user jobserver.'
    }

    Join-Path $env:LOCALAPPDATA 'NukeTheBees\jobserver\bin\jobserver.exe'
}

function Invoke-BenchmarkToolsDirectBuild {
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot,

        [Parameter(Mandatory)]
        [string]$Project,

        [Parameter(Mandatory)]
        [string]$DotnetExecutable
    )

    $previous_node_reuse = $env:MSBUILDDISABLENODEREUSE
    $env:MSBUILDDISABLENODEREUSE = '1'
    Push-Location -LiteralPath $RepositoryRoot
    try {
        & $DotnetExecutable build $Project -m:1 2>&1 | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "BenchmarkTools build exited with code $LASTEXITCODE."
        }
    } finally {
        Pop-Location
        if ($null -eq $previous_node_reuse) {
            Remove-Item Env:MSBUILDDISABLENODEREUSE
        } else {
            $env:MSBUILDDISABLENODEREUSE = $previous_node_reuse
        }
    }
}

function Invoke-BenchmarkToolsBuild {
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot,

        [Parameter(Mandatory)]
        [string]$Project,

        [Parameter(Mandatory)]
        [string]$DotnetExecutable,

        [string]$JobserverExecutable
    )

    if (-not [string]::IsNullOrWhiteSpace($env:NUKETHEBEES_JOBSERVER_JOB)) {
        Invoke-BenchmarkToolsDirectBuild `
            -RepositoryRoot $RepositoryRoot `
            -Project $Project `
            -DotnetExecutable $DotnetExecutable
        return
    }

    $jobserver = Get-BenchmarkToolsJobserverPath -JobserverExecutable $JobserverExecutable
    if (-not (Test-Path -LiteralPath $jobserver -PathType Leaf)) {
        throw "The per-user jobserver is not installed. Run 'csetup native' first. Expected: $jobserver"
    }

    $powerShellExecutable = Join-Path $PSHOME $(if ($PSVersionTable.PSEdition -eq 'Core') {
            'pwsh.exe'
        } else {
            'powershell.exe'
        })
    if (-not (Test-Path -LiteralPath $powerShellExecutable -PathType Leaf)) {
        throw "PowerShell executable was not found: $powerShellExecutable"
    }

    $arguments = @(
        'run'
        '--name'
        'Build BenchmarkTools'
        '--kind'
        'build'
        '--worktree'
        $RepositoryRoot
        '--shared'
        'machine'
        '--'
        $powerShellExecutable
        '-NoProfile'
        '-NonInteractive'
        '-File'
        $PSCommandPath
        '-RepositoryRoot'
        $RepositoryRoot
        '-DotnetExecutable'
        $DotnetExecutable
    )
    & $jobserver @arguments 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "BenchmarkTools jobserver build exited with code $LASTEXITCODE."
    }
}

function Get-BenchmarkToolsPath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot,

        [string]$DotnetExecutable = 'dotnet',

        [string]$JobserverExecutable
    )

    $repository_root = [IO.Path]::GetFullPath($RepositoryRoot)
    $benchmark_tools = Join-Path $repository_root 'tools/bin/BenchmarkTools.exe'
    if (Test-Path -LiteralPath $benchmark_tools -PathType Leaf) {
        return $benchmark_tools
    }

    $project = Join-Path $repository_root 'tools/BenchmarkTools/BenchmarkTools.csproj'
    if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
        throw "BenchmarkTools project was not found: $project"
    }

    Write-Host 'BenchmarkTools is not staged; building and staging it.'
    Invoke-BenchmarkToolsBuild `
        -RepositoryRoot $repository_root `
        -Project $project `
        -DotnetExecutable $DotnetExecutable `
        -JobserverExecutable $JobserverExecutable

    if (-not (Test-Path -LiteralPath $benchmark_tools -PathType Leaf)) {
        throw "BenchmarkTools was not staged after building: $benchmark_tools"
    }

    $benchmark_tools
}

if ($MyInvocation.InvocationName -ne '.') {
    if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
        throw 'RepositoryRoot is required when running BenchmarkTools.ps1 directly.'
    }

    $repository_root = [IO.Path]::GetFullPath($RepositoryRoot)
    $project = Join-Path $repository_root 'tools/BenchmarkTools/BenchmarkTools.csproj'
    if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
        throw "BenchmarkTools project was not found: $project"
    }

    Invoke-BenchmarkToolsBuild `
        -RepositoryRoot $repository_root `
        -Project $project `
        -DotnetExecutable $DotnetExecutable
}
