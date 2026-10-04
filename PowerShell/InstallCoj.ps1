[CmdletBinding()]
param(
    [string]$InstallRoot,
    [string]$LinkDirectory,
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
. "$PSScriptRoot/../tools/install/ToolLinks.ps1"
$iojRoot = Get-IojRoot
if (-not $InstallRoot) { $InstallRoot = Join-Path $iojRoot 'tools/coj' }
$install_root = [IO.Path]::GetFullPath($InstallRoot)
$installed_tool = Join-Path $install_root 'bin\coj.exe'
$links = Get-ToolLinkDirectory $install_root $LinkDirectory
Assert-ToolLinkSupport $links

$worktreeName = Split-Path -Leaf ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')))
$temporaryDirectory = Join-Path $iojRoot 'tmp' $worktreeName
New-Item -ItemType Directory -Path $temporaryDirectory -Force | Out-Null
$installationId = [Guid]::NewGuid().ToString('N')
$stagingRoot = Join-Path $temporaryDirectory "coj-install-$installationId"
$stagedTool = Join-Path $stagingRoot 'bin/coj.exe'
$installBin = Split-Path -Parent $installed_tool
$nextTool = Join-Path $installBin ".coj-next-$installationId.exe"
$retiredTool = $null
$restoredPrevious = $false
$previousTmp = $env:TMP
$previousTemp = $env:TEMP

# Run Cargo inside the workspace so rustup selects its pinned toolchain.
Push-Location -LiteralPath (Join-Path $PSScriptRoot '..\tools\rust')
try {
    $env:TMP = $temporaryDirectory
    $env:TEMP = $temporaryDirectory
    if (-not $SkipTests) {
        & cargo test --package coj --package jobserver-client --locked
        if ($LASTEXITCODE -ne 0) {
            throw "coj tests exited with code $LASTEXITCODE."
        }
    }

    & cargo install --path crates/coj --root $stagingRoot --locked --force
    if ($LASTEXITCODE -ne 0) {
        throw "coj installation exited with code $LASTEXITCODE."
    }

    & $stagedTool --version
    if ($LASTEXITCODE -ne 0) {
        throw "Staged coj smoke test exited with code $LASTEXITCODE."
    }

    # Rename the loaded image so existing Codex launchers can keep running it.
    New-Item -ItemType Directory -Path $installBin -Force | Out-Null
    Copy-Item -LiteralPath $stagedTool -Destination $nextTool
    if (Test-Path -LiteralPath $installed_tool) {
        $retiredPath = Join-Path $installBin ".coj-retired-$installationId.exe"
        Move-Item -LiteralPath $installed_tool -Destination $retiredPath
        $retiredTool = $retiredPath
    }
    try {
        Move-Item -LiteralPath $nextTool -Destination $installed_tool
        & $installed_tool --version
        if ($LASTEXITCODE -ne 0) {
            throw "Installed coj smoke test exited with code $LASTEXITCODE."
        }
    } catch {
        if ($retiredTool) {
            if (Test-Path -LiteralPath $installed_tool) {
                Remove-Item -LiteralPath $installed_tool -Force
            }
            Move-Item -LiteralPath $retiredTool -Destination $installed_tool
            $restoredPrevious = $true
        }
        throw
    }

    foreach ($retired in Get-ChildItem -LiteralPath $installBin -Filter '.coj-retired-*.exe' -File) {
        try {
            Remove-Item -LiteralPath $retired.FullName -Force
        } catch {
            Write-Host "Retained previous executable '$($retired.FullName)': $($_.Exception.Message)"
        }
    }

    Write-Host "`nInstalled coj:`n  $installed_tool"
    Publish-ToolLinks $installBin @('coj.exe') $links
    Write-Host "`nOn a fresh setup, install the shared project tools with:`n  coj install central-tools"
    Write-Host "`nPrepare tasks with 'coj prepare-worktree'; use 'coj git <args...>' for feature work."
    Write-Host "After validation and explicit user authorization, run 'coj integrate' for dev integration."
    Write-Host "`nRun 'coj --help' to see the available commands."
} finally {
    $env:TMP = $previousTmp
    $env:TEMP = $previousTemp
    Pop-Location

    try {
        if (Test-Path -LiteralPath $nextTool) { Remove-Item -LiteralPath $nextTool -Force }
        $stagingPath = [IO.Path]::GetFullPath($stagingRoot)
        $temporaryPrefix = [IO.Path]::GetFullPath($temporaryDirectory).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
        if (-not $stagingPath.StartsWith($temporaryPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Staging cleanup path is outside the worktree temporary directory: '$stagingPath'."
        }
        if (Test-Path -LiteralPath $stagingPath) { Remove-Item -LiteralPath $stagingPath -Recurse -Force }
    } catch {
        Write-Warning "Could not clean up installation staging files: $($_.Exception.Message)"
    }

    if ($retiredTool) {
        Write-Host "`nPrevious executable renamed:`n  $installed_tool`n  -> $retiredTool"
        if ($restoredPrevious) {
            Write-Host 'Installation failed; the previous executable was restored to its original path.'
        } elseif (Test-Path -LiteralPath $retiredTool) {
            Write-Host 'The renamed file is retained; existing sessions can continue using it. A later installation will retry cleanup.'
        } else {
            Write-Host 'The renamed file was no longer in use and has been removed.'
        }
    }
}
