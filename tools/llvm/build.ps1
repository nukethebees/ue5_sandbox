[CmdletBinding()]
param(
    [string]$LLVMSource = 'C:/dev/llvm/llvm-project',
    [string]$BuildDir = 'C:/dev/llvm/build',
    [string]$LLVMRoot = 'C:/dev/llvm/install',
    [ValidateRange(1, 256)]
    [int]$Jobs = 24,
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
$revision = '688a1498b3ce9011ee58c214086e3cd408e86f5e'
$actualRevision = & git -C $LLVMSource rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualRevision -ne $revision) {
    throw "Expected LLVM revision $revision. Updating LLVM is an explicit infrastructure task."
}
if ($env:VSCMD_ARG_HOST_ARCH -ne 'x64' -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64' -or
    $env:VCToolsVersion -notmatch '^14\.50\.35717[\\/]?$' -or
    $env:WindowsSDKVersion -notmatch '^10\.0\.22621\.0[\\/]?$') {
    throw 'Load VsDevCmd.bat -arch=x64 -host_arch=x64 -vcvars_ver=14.50.35717 -winsdk=10.0.22621.0 first.'
}
$tidyCMake = Get-Content -LiteralPath (Join-Path $LLVMSource 'clang-tools-extra/clang-tidy/CMakeLists.txt') -Raw
if (-not $tidyCMake.Contains('if(IOJ_TIDY_SOURCE_DIR)')) {
    throw 'Apply patches/0001-add-ioj-clang-tidy-subdirectory.patch to the LLVM source first.'
}
$checkerSource = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../cmake/clang_tidy/static')).Path
$configureArgs = @(
    '-S', (Join-Path $LLVMSource 'llvm'), '-B', $BuildDir, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_INSTALL_PREFIX=$LLVMRoot",
    '-DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra', '-DLLVM_TARGETS_TO_BUILD=X86',
    '-DLLVM_ENABLE_ASSERTIONS=ON', '-DLLVM_ENABLE_PDB=ON',
    '-DLLVM_ENABLE_EH=OFF', '-DLLVM_ENABLE_RTTI=OFF',
    '-DLLVM_ENABLE_PLUGINS=OFF', '-DLLVM_EXPORT_SYMBOLS_FOR_PLUGINS=OFF',
    '-DCLANG_PLUGIN_SUPPORT=OFF', '-DCLANG_TOOLS_EXTRA_INCLUDE_TESTS=OFF',
    '-DLLVM_INCLUDE_EXAMPLES=OFF', '-DLLVM_INCLUDE_BENCHMARKS=OFF',
    "-DIOJ_TIDY_SOURCE_DIR:PATH=$checkerSource"
)
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'LLVM configure failed.' }

& cmake --build $BuildDir --target clang-tidy --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'clang-tidy build failed.' }

$checks = & (Join-Path $BuildDir 'bin/clang-tidy.exe') '-checks=-*,ioj-loop-condition-call' -list-checks
if ($LASTEXITCODE -ne 0 -or ($checks -join "`n") -notmatch '(?m)^\s+ioj-loop-condition-call\s*$') {
    throw 'The built clang-tidy did not register ioj-loop-condition-call.'
}
$checks
if ($Install) {
    & cmake --install $BuildDir --component clang-tidy
    if ($LASTEXITCODE -ne 0) { throw 'clang-tidy install failed.' }
}
