function Get-BenchmarkToolsPath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot,

        [string]$DotnetExecutable = 'dotnet'
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
    $previous_node_reuse = $env:MSBUILDDISABLENODEREUSE
    $env:MSBUILDDISABLENODEREUSE = '1'
    Push-Location -LiteralPath $repository_root
    try {
        & $DotnetExecutable build $project -m:1 2>&1 | Out-Host
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

    if (-not (Test-Path -LiteralPath $benchmark_tools -PathType Leaf)) {
        throw "BenchmarkTools was not staged after building: $benchmark_tools"
    }

    $benchmark_tools
}
