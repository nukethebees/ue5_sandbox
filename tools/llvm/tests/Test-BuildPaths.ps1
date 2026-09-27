[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$FixtureRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$FixtureRoot = [System.IO.Path]::GetFullPath($FixtureRoot, (Get-Location).Path)
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

$discoveryRoot = Join-Path $FixtureRoot 'discovery'
$discoverySource = Join-Path $discoveryRoot 'llvm-project'
$discoveryInstall = Join-Path $discoveryRoot 'installed'
$discoveryBuild = Join-Path $discoveryRoot 'existing build'
$secondBuild = Join-Path $discoveryRoot 'second build'
$null = New-Item -ItemType Directory -Path $discoverySource, $discoveryInstall, $discoveryBuild -Force
$cacheText = "CMAKE_HOME_DIRECTORY:INTERNAL=$discoverySource/llvm`nCMAKE_INSTALL_PREFIX:PATH=$discoveryInstall"
Set-Content -LiteralPath (Join-Path $discoveryBuild 'CMakeCache.txt') -Value $cacheText
$previousRoot = $env:LLVM_ROOT
try {
    $env:LLVM_ROOT = $discoveryInstall
    try {
        . $buildScript 'Common7/Tools'
        throw 'Discovery probe did not stop at the revision check'
    } catch {
        if ($_.Exception.Message -ne 'LLVM path probe reached revision check') { throw }
    }
    if ($LLVMSource -ne $discoverySource -or $BuildDir -ne $discoveryBuild -or $LLVMRoot -ne $discoveryInstall) {
        throw 'Single-argument invocation did not select the existing installation/source/build.'
    }
    $null = New-Item -ItemType Directory -Path $secondBuild -Force
    Set-Content -LiteralPath (Join-Path $secondBuild 'CMakeCache.txt') -Value $cacheText
    try {
        . $buildScript 'Common7/Tools'
        throw 'Ambiguous build discovery should fail'
    } catch {
        if ($_.Exception.Message -notlike 'Multiple matching LLVM build directories;*') { throw }
    }
} finally {
    $env:LLVM_ROOT = $previousRoot
    Remove-Item -LiteralPath (Join-Path $secondBuild 'CMakeCache.txt') -ErrorAction SilentlyContinue
}
Write-Host 'LLVM defaults reuse a matching build and reject ambiguous matches.'

function git {
    if ($args -contains 'rev-parse') {
        $global:LASTEXITCODE = 0
        return 'e0b3e4c82911376fcb2dfcbc4a3dd3f4f5891aba'
    }
    throw 'LLVM environment probe reached patch check'
}

$commonTools = Join-Path $FixtureRoot 'VS & tools % literal/Common7/Tools'
$null = New-Item -ItemType Directory -Path $commonTools -Force
$setupScript = Join-Path $commonTools 'VsDevCmd.bat'
$environmentNames = @('VSCMD_ARG_HOST_ARCH', 'VSCMD_ARG_TGT_ARCH', 'VCToolsVersion',
    'WindowsSDKVersion', 'IOJ_LLVM_VSDEVCMD', 'IOJ_LLVM_SETUP_PROBE')
$previousEnvironment = @{}
foreach ($name in $environmentNames) {
    $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}
try {
    $env:IOJ_LLVM_VSDEVCMD = 'preserve this value'
    Set-Content -LiteralPath $setupScript -Value @'
@echo off
if not "%*"=="-no_logo -arch=x64 -host_arch=x64 -vcvars_ver=14.50.35717 -winsdk=10.0.22621.0" exit /b 11
set VSCMD_ARG_HOST_ARCH=x64
set VSCMD_ARG_TGT_ARCH=x64
set VCToolsVersion=14.50.35717\
set WindowsSDKVersion=10.0.22621.0\
set IOJ_LLVM_SETUP_PROBE=loaded=with equals
exit /b 0
'@
    try {
        . $buildScript $commonTools -LLVMSource $sourceDirectory -BuildDir $discoveryBuild -LLVMRoot $discoveryInstall
        throw 'Environment probe did not stop at the patch check'
    } catch {
        if ($_.Exception.Message -ne 'LLVM environment probe reached patch check') { throw }
    }
    if ($env:IOJ_LLVM_SETUP_PROBE -ne 'loaded=with equals' -or
        $env:IOJ_LLVM_VSDEVCMD -ne 'preserve this value') {
        throw 'Developer environment was not imported correctly.'
    }
    Set-Content -LiteralPath $setupScript -Value '@exit /b 23'
    try {
        . $buildScript $commonTools -LLVMSource $sourceDirectory -BuildDir $discoveryBuild -LLVMRoot $discoveryInstall
        throw 'Failed developer setup should stop the build'
    } catch {
        if ($_.Exception.Message -ne 'VsDevCmd.bat failed with exit code 23.') { throw }
    }
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $previousEnvironment[$name], 'Process')
    }
}
Write-Host 'Common7/Tools initializes the pinned environment; setup failures stop before patching/building.'
