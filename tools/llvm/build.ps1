[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$LLVMSource,
    [Parameter(Mandatory)]
    [string]$BuildDir,
    [string]$LLVMRoot = $env:LLVM_ROOT,
    [ValidateRange(1, 256)]
    [int]$Jobs = 24,
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$revision = 'e0b3e4c82911376fcb2dfcbc4a3dd3f4f5891aba'
$LLVMSource = (Resolve-Path -LiteralPath $LLVMSource).Path
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
if ($Install -and [string]::IsNullOrWhiteSpace($LLVMRoot)) {
    throw '-Install requires -LLVMRoot or the LLVM_ROOT environment variable.'
}
$actualRevision = & git -C $LLVMSource rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualRevision -ne $revision) {
    throw "Expected LLVM revision $revision. Updating LLVM is an explicit infrastructure task."
}
if ($env:VSCMD_ARG_HOST_ARCH -ne 'x64' -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64' -or
    $env:VCToolsVersion -notmatch '^14\.50\.35717[\\/]?$' -or
    $env:WindowsSDKVersion -notmatch '^10\.0\.22621\.0[\\/]?$') {
    throw 'Load VsDevCmd.bat -arch=x64 -host_arch=x64 -vcvars_ver=14.50.35717 -winsdk=10.0.22621.0 first.'
}
# Each patch is independently either pristine or fully applied.
$patches = @(
    '0001-add-ioj-clang-tidy-subdirectory.patch',
    '0002-fix-analyzer-control-dependency-lifetime.patch'
)
$applied = @()
$pending = @()
foreach ($name in $patches) {
    $patch = Join-Path $PSScriptRoot "patches/$name"
    & git -C $LLVMSource apply --reverse --check $patch 2>$null
    if ($LASTEXITCODE -eq 0) {
        $applied += $patch
        Write-Host "Already applied: $name"
        continue
    }
    & git -C $LLVMSource apply --check $patch
    if ($LASTEXITCODE -ne 0) { throw "Unexpected LLVM source state for $name." }
    $pending += $patch
}

# Compare against an index containing only HEAD and the already-applied owned
# patches. This also rejects unrelated edits within a patched file.
$previousIndex = $env:GIT_INDEX_FILE
$temporaryIndex = Join-Path ([System.IO.Path]::GetTempPath()) ([System.IO.Path]::GetRandomFileName())
try {
    $env:GIT_INDEX_FILE = $temporaryIndex
    & git -C $LLVMSource read-tree HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect LLVM source state.' }
    foreach ($patch in $applied) {
        & git -C $LLVMSource apply --cached $patch
        if ($LASTEXITCODE -ne 0) { throw "Cannot reconstruct expected source state for $patch." }
    }
    & git -C $LLVMSource diff --quiet
    if ($LASTEXITCODE -ne 0) { throw 'LLVM checkout has unexpected tracked edits. Use a clean pinned checkout.' }
    $untracked = & git -C $LLVMSource ls-files --others --exclude-standard
    if ($LASTEXITCODE -ne 0 -or $untracked) { throw "LLVM checkout has unexpected untracked files: $untracked" }
} finally {
    $env:GIT_INDEX_FILE = $previousIndex
    Remove-Item -LiteralPath $temporaryIndex -Force -ErrorAction SilentlyContinue
}
foreach ($patch in $pending) {
    & git -C $LLVMSource apply $patch
    if ($LASTEXITCODE -ne 0) { throw "Failed to apply $patch." }
}

$checkerSource = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'clang_tidy')).Path
$runtimeTools = @(
    "-DCMAKE_LINKER:FILEPATH=$(Join-Path $env:VCToolsInstallDir 'bin/Hostx64/x64/link.exe')",
    "-DCMAKE_MT:FILEPATH=$(Join-Path $env:WindowsSdkDir 'bin/10.0.22621.0/x64/mt.exe')"
) -join ';'
$configureArgs = @(
    '-S', (Join-Path $LLVMSource 'llvm'), '-B', $BuildDir, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl',
    '-DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra', '-DLLVM_TARGETS_TO_BUILD=X86',
    '-DLLVM_ENABLE_RUNTIMES=compiler-rt', '-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF',
    "-DBUILTINS_CMAKE_ARGS:STRING=$runtimeTools", "-DRUNTIMES_CMAKE_ARGS:STRING=$runtimeTools",
    '-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON', '-DCOMPILER_RT_SANITIZERS_TO_BUILD=asan',
    '-DCOMPILER_RT_BUILD_SANITIZERS=ON', '-DCOMPILER_RT_INCLUDE_TESTS=OFF',
    '-DCOMPILER_RT_BUILD_XRAY=OFF', '-DCOMPILER_RT_BUILD_LIBFUZZER=OFF',
    '-DCOMPILER_RT_BUILD_PROFILE=OFF', '-DCOMPILER_RT_BUILD_CTX_PROFILE=OFF',
    '-DCOMPILER_RT_BUILD_MEMPROF=OFF', '-DCOMPILER_RT_BUILD_COPYPROF=OFF',
    '-DCOMPILER_RT_BUILD_ORC=OFF', '-DCOMPILER_RT_BUILD_GWP_ASAN=OFF',
    '-DLLVM_ENABLE_ASSERTIONS=ON', '-DLLVM_ENABLE_PDB=ON',
    '-DLLVM_ENABLE_EH=OFF', '-DLLVM_ENABLE_RTTI=OFF',
    '-DLLVM_ENABLE_PLUGINS=OFF', '-DLLVM_EXPORT_SYMBOLS_FOR_PLUGINS=OFF',
    '-DCLANG_PLUGIN_SUPPORT=OFF', '-DCLANG_TOOLS_EXTRA_INCLUDE_TESTS=OFF',
    '-DLLVM_INCLUDE_EXAMPLES=OFF', '-DLLVM_INCLUDE_BENCHMARKS=OFF',
    "-DIOJ_TIDY_SOURCE_DIR:PATH=$checkerSource"
)
if (-not [string]::IsNullOrWhiteSpace($LLVMRoot)) {
    $LLVMRoot = [System.IO.Path]::GetFullPath($LLVMRoot)
    $configureArgs += "-DCMAKE_INSTALL_PREFIX=$LLVMRoot"
}
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'LLVM configure failed.' }

& cmake --build $BuildDir --target clang clang-format clang-tidy clang-scan-deps llvm-ar llvm-nm llvm-readobj runtimes --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'LLVM toolchain build failed.' }

$checks = & (Join-Path $BuildDir 'bin/clang-tidy.exe') '-checks=-*,ioj-loop-condition-call' -list-checks
if ($LASTEXITCODE -ne 0 -or ($checks -join "`n") -notmatch '(?m)^\s+ioj-loop-condition-call\s*$') {
    throw 'The built clang-tidy did not register ioj-loop-condition-call.'
}
$checks
$projectSource = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
& python (Join-Path $checkerSource 'tests/test_loop_condition_call.py') `
    --clang-tidy (Join-Path $BuildDir 'bin/clang-tidy.exe') --source-dir $projectSource
if ($LASTEXITCODE -ne 0) { throw 'IOJ clang-tidy semantic tests failed; installation is blocked.' }

if ($Install) {
    foreach ($component in @('clang', 'clang-resource-headers', 'clang-format', 'clang-tidy', 'clang-scan-deps',
            'llvm-ar', 'llvm-lib', 'llvm-nm', 'llvm-readobj', 'builtins', 'runtimes')) {
        & cmake --install $BuildDir --component $component
        if ($LASTEXITCODE -ne 0) { throw "LLVM component install failed: $component" }
    }
    foreach ($tool in @('clang.exe', 'clang-cl.exe', 'clang-format.exe', 'clang-tidy.exe', 'clang-scan-deps.exe',
            'run-clang-tidy', 'llvm-ar.exe', 'llvm-lib.exe', 'llvm-nm.exe', 'llvm-readobj.exe')) {
        if (-not (Test-Path -LiteralPath (Join-Path $LLVMRoot "bin/$tool") -PathType Leaf)) {
            throw "Installed toolchain is missing $tool."
        }
        if ($tool.EndsWith('.exe') -and
            (Get-FileHash -LiteralPath (Join-Path $LLVMRoot "bin/$tool")).Hash -ne
            (Get-FileHash -LiteralPath (Join-Path $BuildDir "bin/$tool")).Hash) {
            throw "Installed $tool does not match the newly built executable."
        }
    }
    $resourceDirectory = & (Join-Path $LLVMRoot 'bin/clang.exe') -print-resource-dir
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $resourceDirectory 'include/stddef.h'))) {
        throw 'Installed toolchain is missing Clang resource headers.'
    }
    foreach ($runtime in @('clang_rt.asan_dynamic-x86_64.dll', 'clang_rt.asan_dynamic-x86_64.lib',
            'clang_rt.asan_dynamic_runtime_thunk-x86_64.lib')) {
        if (-not (Test-Path -LiteralPath (Join-Path $resourceDirectory "lib/windows/$runtime") -PathType Leaf)) {
            throw "Installed toolchain is missing the Windows AddressSanitizer artifact $runtime."
        }
    }
    Write-Host "Verified LLVM toolchain installation: $LLVMRoot"
}
