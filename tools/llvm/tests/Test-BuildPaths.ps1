[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$FixtureRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$buildScript = Join-Path $PSScriptRoot '../build.ps1'
$shellDirectory = Join-Path $FixtureRoot 'shell location'
$processDirectory = Join-Path $FixtureRoot 'process location'
$sourceDirectory = Join-Path $shellDirectory 'source tree'
$null = New-Item -ItemType Directory -Path $sourceDirectory, $processDirectory -Force
$previousProcessDirectory = [Environment]::CurrentDirectory

# Stop the real build script at its first external command, before any build or patching.
function git {
    throw 'LLVM path probe reached revision check'
}

Push-Location -LiteralPath $shellDirectory
try {
    [Environment]::CurrentDirectory = $processDirectory
    foreach ($absolute in @($false, $true)) {
        $sourceArgument = 'source tree'
        $buildArgument = 'unused/../build tree'
        $rootArgument = 'unused/../install tree'
        if ($absolute) {
            $sourceArgument = $sourceDirectory
            $buildArgument = Join-Path $processDirectory $buildArgument
            $rootArgument = Join-Path $processDirectory $rootArgument
        }

        try {
            . $buildScript -LLVMSource $sourceArgument -BuildDir $buildArgument -LLVMRoot $rootArgument -Install
            throw 'Build script did not stop at the revision check'
        } catch {
            if ($_.Exception.Message -ne 'LLVM path probe reached revision check') {
                throw
            }
        }

        $expectedBase = if ($absolute) { $processDirectory } else { $shellDirectory }
        if ($LLVMSource -ne $sourceDirectory -or
            $BuildDir -ne (Join-Path $expectedBase 'build tree') -or
            $LLVMRoot -ne (Join-Path $expectedBase 'install tree')) {
            throw "Unexpected normalized paths: source=$LLVMSource build=$BuildDir install=$LLVMRoot"
        }
        if ((Test-Path -LiteralPath $BuildDir) -or (Test-Path -LiteralPath $LLVMRoot)) {
            throw 'The path probe must not create build or installation directories'
        }
    }
} finally {
    [Environment]::CurrentDirectory = $previousProcessDirectory
    Pop-Location
}
Write-Host 'LLVM build paths follow the PowerShell location; absolute paths are preserved.'
