[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$VSCommonTools,
    [string]$LLVMSource,
    [string]$BuildDir,
    [string]$LLVMRoot = $env:LLVM_ROOT,
    [ValidateRange(1, 256)]
    [int]$Jobs = 24,
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$revision = 'e0b3e4c82911376fcb2dfcbc4a3dd3f4f5891aba'
$invocationDirectory = (Get-Location -PSProvider FileSystem).ProviderPath
if ([string]::IsNullOrWhiteSpace($LLVMRoot)) {
    $installedClang = Get-Command clang-cl.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($installedClang) {
        $LLVMRoot = Split-Path (Split-Path $installedClang.Source -Parent) -Parent
    }
}
if (-not [string]::IsNullOrWhiteSpace($LLVMRoot)) {
    $LLVMRoot = [System.IO.Path]::GetFullPath($LLVMRoot, $invocationDirectory)
}
if ([string]::IsNullOrWhiteSpace($LLVMSource)) {
    if ([string]::IsNullOrWhiteSpace($LLVMRoot)) {
        throw 'Pass -LLVMSource and -BuildDir, or provide LLVM_ROOT/an installed clang-cl on PATH to discover them.'
    }
    $LLVMSource = Join-Path (Split-Path $LLVMRoot -Parent) 'llvm-project'
}
$LLVMSource = (Resolve-Path -LiteralPath ([System.IO.Path]::GetFullPath($LLVMSource, $invocationDirectory))).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $workspace = if ($LLVMRoot) { Split-Path $LLVMRoot -Parent } else { Split-Path $LLVMSource -Parent }
    $matchingBuilds = @(foreach ($directory in Get-ChildItem -LiteralPath $workspace -Directory) {
        $cache = Join-Path $directory.FullName 'CMakeCache.txt'
        if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) { continue }
        $cacheSource = $null
        $cacheInstall = $null
        foreach ($line in Get-Content -LiteralPath $cache) {
            if ($line -match '^CMAKE_HOME_DIRECTORY:[^=]+=(.+)$') { $cacheSource = $Matches[1] }
            if ($line -match '^CMAKE_INSTALL_PREFIX:[^=]+=(.+)$') { $cacheInstall = $Matches[1] }
        }
        if ($cacheSource -and $cacheInstall -and
            [System.IO.Path]::GetFullPath($cacheSource) -eq (Join-Path $LLVMSource 'llvm') -and
            (!$LLVMRoot -or [System.IO.Path]::GetFullPath($cacheInstall) -eq $LLVMRoot)) {
            $directory.FullName
        }
    })
    if ($matchingBuilds.Count -gt 1) {
        throw "Multiple matching LLVM build directories; select one with -BuildDir: $($matchingBuilds -join ', ')"
    }
    $BuildDir = if ($matchingBuilds.Count -eq 1) { $matchingBuilds[0] } else { Join-Path $workspace 'build' }
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir, $invocationDirectory)
if ($Install -and [string]::IsNullOrWhiteSpace($LLVMRoot)) {
    throw '-Install requires -LLVMRoot, LLVM_ROOT, or an installed clang-cl on PATH.'
}
Write-Host "LLVM source: $LLVMSource"
Write-Host "LLVM build: $BuildDir"
Write-Host "LLVM installation: $LLVMRoot"
$actualRevision = & git -C $LLVMSource rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualRevision -ne $revision) {
    throw "Expected LLVM revision $revision. Updating LLVM is an explicit infrastructure task."
}
if (-not [string]::IsNullOrWhiteSpace($VSCommonTools)) {
    $vsDevCmd = Join-Path ([System.IO.Path]::GetFullPath($VSCommonTools, $invocationDirectory)) 'VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $vsDevCmd -PathType Leaf)) {
        throw "VSCommonTools must contain VsDevCmd.bat: $VSCommonTools"
    }
    $previousSetup = $env:IOJ_LLVM_VSDEVCMD
    try {
        $env:IOJ_LLVM_VSDEVCMD = $vsDevCmd
        $developerEnvironment = & $env:ComSpec /d /v:off /s /c '""%IOJ_LLVM_VSDEVCMD%" -no_logo -arch=x64 -host_arch=x64 -vcvars_ver=14.50.35717 -winsdk=10.0.22621.0 >nul && set"'
        if ($LASTEXITCODE -ne 0) { throw "VsDevCmd.bat failed with exit code $LASTEXITCODE." }
        foreach ($line in $developerEnvironment) {
            if ($line -match '^([^=]+)=(.*)$') {
                [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
            }
        }
    } finally {
        $env:IOJ_LLVM_VSDEVCMD = $previousSetup
    }
}
if ($env:VSCMD_ARG_HOST_ARCH -ne 'x64' -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64' -or
    $env:VCToolsVersion -notmatch '^14\.50\.35717[\\/]?$' -or
    $env:WindowsSDKVersion -notmatch '^10\.0\.22621\.0[\\/]?$') {
    throw 'Pass -VSCommonTools pointing to Common7/Tools with MSVC 14.50.35717 and Windows SDK 10.0.22621.0 installed, or load that x64 developer environment first.'
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
    $configureArgs += "-DCMAKE_INSTALL_PREFIX=$LLVMRoot"
}
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'LLVM configure failed.' }

& cmake --build $BuildDir --target clang clang-format clang-tidy clang-scan-deps llvm-ar llvm-nm llvm-readobj runtimes --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'LLVM toolchain build failed.' }

$checks = & (Join-Path $BuildDir 'bin/clang-tidy.exe') '-checks=-*,ioj-*' -list-checks
if ($LASTEXITCODE -ne 0) { throw 'Cannot query the built clang-tidy checks.' }
foreach ($check in @('ioj-loop-condition-call', 'ioj-loop-view-construction',
        'ioj-loop-view-accessor-call', 'ioj-no-pair', 'ioj-no-tuple')) {
    if (($checks -join "`n") -notmatch "(?m)^\s+$check\s*$") {
        throw "The built clang-tidy did not register $check."
    }
}
$checks
$projectSource = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
foreach ($test in Get-ChildItem -LiteralPath (Join-Path $checkerSource 'tests') -Filter 'test_*.py') {
    & python $test.FullName --clang-tidy (Join-Path $BuildDir 'bin/clang-tidy.exe') --source-dir $projectSource
    if ($LASTEXITCODE -ne 0) { throw "IOJ clang-tidy test $($test.Name) failed; installation is blocked." }
}

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
