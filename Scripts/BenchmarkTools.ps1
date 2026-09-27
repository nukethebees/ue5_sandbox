function Get-BenchmarkToolsPath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot,
        [string]$CMakeExecutable = 'cmake'
    )

    $repository_root = [IO.Path]::GetFullPath($RepositoryRoot)
    Push-Location -LiteralPath $repository_root
    try {
        & $CMakeExecutable --preset native 2>&1 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "BenchmarkTools configure failed ($LASTEXITCODE)." }
        & $CMakeExecutable --build --preset native --target benchmark-tools-host 2>&1 | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "BenchmarkTools build failed ($LASTEXITCODE)." }
    } finally {
        Pop-Location
    }

    $runner = Join-Path $repository_root 'out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe'
    if (-not (Test-Path -LiteralPath $runner -PathType Leaf)) {
        throw "BenchmarkTools host output was not found: $runner"
    }
    $runner
}
