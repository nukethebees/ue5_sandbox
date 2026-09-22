[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuiltToolPath,

    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'NukeTheBees')
)

$ErrorActionPreference = 'Stop'

$builtTool = (Resolve-Path -LiteralPath $BuiltToolPath).Path
$root = [IO.Path]::GetFullPath($InstallRoot)
$bin = Join-Path $root 'bin'
$installedTool = Join-Path $bin 'set-live-coding-disabled.exe'
$previous = Join-Path $root 'previous\set-live-coding-disabled.exe'
$staging = Join-Path $root ".set-live-coding-disabled.staging-$PID-$([Guid]::NewGuid().ToString('N'))"
$stagedTool = Join-Path $staging 'set-live-coding-disabled.exe'
$userSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$mutex = [Threading.Mutex]::new($false, "Global\NukeTheBees.SetLiveCodingDisabled.Install.$userSid")
$ownsMutex = $false

function Activate-StagedTool {
    param(
        [string]$StagedTool,
        [string]$InstalledTool,
        [string]$PreviousTool
    )

    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        try {
            Copy-Item -LiteralPath $InstalledTool -Destination $PreviousTool -Force
            [IO.File]::Move($StagedTool, $InstalledTool, $true)
            return
        } catch [UnauthorizedAccessException] {
            if ([DateTime]::UtcNow -ge $deadline) {
                throw
            }
            Start-Sleep -Milliseconds 100
        } catch [IO.IOException] {
            if ([DateTime]::UtcNow -ge $deadline) {
                throw
            }
            Start-Sleep -Milliseconds 100
        }
    } while ($true)
}

try {
    try {
        $ownsMutex = $mutex.WaitOne([TimeSpan]::FromSeconds(30))
    } catch [Threading.AbandonedMutexException] {
        $ownsMutex = $true
    }
    if (-not $ownsMutex) {
        throw 'Another set-live-coding-disabled installation is already in progress.'
    }

    New-Item -ItemType Directory -Path $staging -Force | Out-Null
    Copy-Item -LiteralPath $builtTool -Destination $stagedTool
    & $stagedTool --settings-path (Join-Path $staging 'missing-settings.ini')
    if ($LASTEXITCODE -ne 0) {
        throw 'The staged set-live-coding-disabled executable failed validation.'
    }

    New-Item -ItemType Directory -Path $bin -Force | Out-Null
    if (Test-Path -LiteralPath $installedTool -PathType Leaf) {
        $previousDirectory = Split-Path -Parent $previous
        New-Item -ItemType Directory -Path $previousDirectory -Force | Out-Null
        if (Test-Path -LiteralPath $previous) {
            Remove-Item -LiteralPath $previous -Force
        }

        Activate-StagedTool $stagedTool $installedTool $previous
    } else {
        Move-Item -LiteralPath $stagedTool -Destination $installedTool
    }

    Write-Host "Installed canonical set-live-coding-disabled at '$installedTool'."
} finally {
    if (Test-Path -LiteralPath $staging) {
        Remove-Item -LiteralPath $staging -Recurse -Force
    }
    if ($ownsMutex) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
