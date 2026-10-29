param(
    [Parameter(Mandatory)]
    [ValidateSet('Prepare', 'Restore', 'Clean')]
    [string] $Operation,
    [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$configuration = Join-Path $workspace 'Saved/UnrealBuildTool/BuildConfiguration.xml'

if ($Operation -eq 'Clean') {
    $roots = @($workspace)
    foreach ($plugin in Get-ChildItem -LiteralPath (Join-Path $workspace 'Plugins') -Directory) {
        $roots += $plugin.FullName
    }
    $targets = @()
    foreach ($root in $roots) {
        foreach ($relative in @('Intermediate/Build/Win64/x64/UnrealEditor/DebugGame', 'Intermediate/Build/Win64/UnrealEditor/Inc')) {
            $candidate = Join-Path $root $relative
            if (Test-Path -LiteralPath $candidate) {
                $resolved = (Resolve-Path -LiteralPath $candidate).Path
                if (-not $resolved.StartsWith($workspace + '\', [StringComparison]::OrdinalIgnoreCase)) {
                    throw "Clean target outside workspace: $resolved"
                }
                $targets += $resolved
            }
        }
    }
    foreach ($target in $targets) {
        Write-Output "Removing generated intermediates: $target"
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    exit 0
}

if (-not $OutputDirectory) {
    throw 'OutputDirectory is required for Prepare and Restore'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $OutputDirectory.StartsWith($workspace + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw "Profile output outside workspace: $OutputDirectory"
}
$backup = Join-Path $OutputDirectory 'BuildConfiguration.original.xml'
$absent = Join-Path $OutputDirectory 'BuildConfiguration.original.absent'
$started = Join-Path $OutputDirectory 'started.txt'

if ($Operation -eq 'Restore') {
    if (Test-Path -LiteralPath $backup) {
        [IO.File]::WriteAllBytes($configuration, [IO.File]::ReadAllBytes($backup))
    } elseif (Test-Path -LiteralPath $absent) {
        Remove-Item -LiteralPath $configuration -ErrorAction SilentlyContinue
    } else {
        exit 0
    }
    Write-Output 'Restored original project Unreal configuration.'
    $startTime = [DateTime]::Parse([IO.File]::ReadAllText($started), [Globalization.CultureInfo]::InvariantCulture).ToUniversalTime()
    $logRoot = Join-Path $env:LOCALAPPDATA 'UnrealBuildTool'
    foreach ($name in @('Log.txt', 'Log.json', 'Trace.uba')) {
        $source = Join-Path $logRoot $name
        if ((Test-Path -LiteralPath $source) -and (Get-Item -LiteralPath $source).LastWriteTimeUtc -ge $startTime) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $OutputDirectory $name)
        }
    }
    exit 0
}

New-Item -ItemType Directory -Path (Join-Path $OutputDirectory 'telemetry') -Force | Out-Null
[IO.File]::WriteAllText($started, [DateTime]::UtcNow.ToString('O'))
if (Test-Path -LiteralPath $configuration) {
    $bytes = [IO.File]::ReadAllBytes($configuration)
    [xml] $document = [IO.File]::ReadAllText($configuration)
    [IO.File]::WriteAllBytes($backup, $bytes)
} else {
    [xml] $document = '<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration" />'
    [IO.File]::WriteAllText($absent, '')
    New-Item -ItemType Directory -Path (Split-Path $configuration) -Force | Out-Null
}

$settings = @{
    UnrealBuildAccelerator = @{
        bDetailedTrace = 'true'
        bPrintSummary = 'true'
        ActionsOutputFile = (Join-Path $OutputDirectory 'actions.yaml')
    }
    ParallelExecutor = @{
        bShowCompilationTimes = 'true'
        bShowPerActionCompilationTimes = 'true'
        bLogActionCommandLines = 'true'
        bPrintActionTargetNames = 'true'
        bShowCPUUtilization = 'true'
    }
}
foreach ($section in $settings.Keys) {
    $node = $document.DocumentElement.SelectSingleNode("*[local-name()='$section']")
    if (-not $node) {
        $node = $document.CreateElement($section, $document.DocumentElement.NamespaceURI)
        [void] $document.DocumentElement.AppendChild($node)
    }
    foreach ($name in $settings[$section].Keys) {
        $setting = $node.SelectSingleNode("*[local-name()='$name']")
        if (-not $setting) {
            $setting = $document.CreateElement($name, $document.DocumentElement.NamespaceURI)
            [void] $node.AppendChild($setting)
        }
        $setting.InnerText = $settings[$section][$name]
    }
}
$document.Save((Join-Path $OutputDirectory 'BuildConfiguration.profile.xml'))
$document.Save($configuration)
Write-Output 'Enabled detailed UBA tracing in project-local configuration.'
