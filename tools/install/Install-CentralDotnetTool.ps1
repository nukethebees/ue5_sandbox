[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('UnrealBuildTools', 'CodeFormatTools')]
    [string]$ToolName,
    [string]$InstallRoot,
    [string]$DotnetExecutable = 'dotnet'
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
if (-not $InstallRoot) {
    $InstallRoot = Join-Path $env:LOCALAPPDATA "NukeTheBees/$ToolName"
}
$root = [IO.Path]::GetFullPath($InstallRoot)
$tools = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$candidate = Join-Path $root ".candidate-$([Guid]::NewGuid().ToString('N'))"
$backup = Join-Path $root ".previous-$([Guid]::NewGuid().ToString('N'))"
$bin = Join-Path $root 'bin'
$published = Join-Path $candidate 'bin'
$artifacts = Join-Path $candidate 'build'
$userSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$mutex = [Threading.Mutex]::new($false, "Global\NukeTheBees.$ToolName.Install.$userSid")
$ownsMutex = $false

function Remove-PrivateOutput([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Private output escaped installation root: $resolved"
    }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}

try {
    try { $ownsMutex = $mutex.WaitOne([TimeSpan]::FromSeconds(30)) }
    catch [Threading.AbandonedMutexException] { $ownsMutex = $true }
    if (-not $ownsMutex) { throw "Another $ToolName installation is in progress." }

    & $DotnetExecutable publish "$tools/$ToolName/$ToolName.csproj" --configuration Release `
        --artifacts-path $artifacts --output $published --nologo -p:SandboxCMakeHostToolBuild=true
    if ($LASTEXITCODE -ne 0) { throw "$ToolName candidate publish failed ($LASTEXITCODE)." }

    & $DotnetExecutable test "$tools/$ToolName.Tests/$ToolName.Tests.csproj" --configuration Release `
        --artifacts-path $artifacts --nologo -p:SandboxCMakeHostToolBuild=true
    if ($LASTEXITCODE -ne 0) { throw "$ToolName candidate tests failed ($LASTEXITCODE)." }

    $executable = Join-Path $published "$ToolName.exe"
    & $executable --version
    if ($LASTEXITCODE -ne 0) { throw "$ToolName candidate version smoke test failed." }
    & $executable --invalid-installer-smoke-argument 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 2) { throw "$ToolName candidate argument smoke test failed." }

    if (Test-Path -LiteralPath $bin) { [IO.Directory]::Move($bin, $backup) }
    try {
        [IO.Directory]::Move($published, $bin)
    } catch {
        if (Test-Path -LiteralPath $backup) { [IO.Directory]::Move($backup, $bin) }
        throw
    }
    Remove-PrivateOutput $backup
    Write-Host "Installed $ToolName. The maintainer must ensure '$bin' is on PATH."
} finally {
    Remove-PrivateOutput $candidate
    if ($ownsMutex) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
