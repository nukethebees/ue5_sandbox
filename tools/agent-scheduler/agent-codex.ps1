$ErrorActionPreference = 'Stop'
$scriptFile = Get-Item -LiteralPath $PSCommandPath
if ($scriptFile.LinkType -eq 'SymbolicLink') { $scriptFile = $scriptFile.ResolveLinkTarget($true) }
$scriptRoot = $scriptFile.DirectoryName
$bin = $scriptRoot
$codex = Join-Path $bin 'codex-scheduler.exe'
$defaultRules = Join-Path $scriptRoot 'scheduling.default.rules'
if (Test-Path -LiteralPath "$scriptRoot/Cargo.toml") {
    # The source launcher uses the local development build; installed files are siblings.
    $repo = (Resolve-Path "$scriptRoot/../..").Path
    $bin = Join-Path $repo '.local/scheduler-target/debug'
    $codex = Join-Path $bin 'codex.exe'
    $defaultRules = Join-Path $scriptRoot 'scheduling.rules'
}
if (-not (Test-Path -LiteralPath $codex)) { throw "Missing '$codex'. Build codex-scheduler locally or rerun the central installer." }
$hostExecutable = Join-Path $bin 'codex-code-mode-host.exe'
if (-not (Test-Path -LiteralPath $hostExecutable)) { throw "Missing '$hostExecutable'. Rebuild/reinstall codex-scheduler; code-mode tool calls require this companion executable." }
$rules = $env:AGENT_SCHEDULER_RULES
if (-not $rules) {
    $configRoot = $env:NTB_APPDATA_LOCAL
    if (-not $configRoot) { $configRoot = Join-Path $env:LOCALAPPDATA 'NukeTheBees' }
    $rules = Join-Path $configRoot 'config/agent-scheduler/scheduling.rules'
}
if (-not (Test-Path -LiteralPath $rules -PathType Leaf)) {
    throw "Missing scheduling rules: '$rules'. Create its parent directory and copy '$defaultRules' there, then review your exemptions. Alternatively set AGENT_SCHEDULER_RULES to your rules file. Installation never creates or overwrites your active rules."
}
$rules = (Resolve-Path -LiteralPath $rules).Path
$previousPath = $env:PATH
$previousRules = $env:AGENT_SCHEDULER_RULES
try {
    $env:PATH = "$bin;$previousPath"
    $env:AGENT_SCHEDULER_RULES = $rules
    & $codex -c 'windows.sandbox="unelevated"' -c features.shell_snapshot=false @args
    $result = $LASTEXITCODE
} finally {
    $env:PATH = $previousPath
    $env:AGENT_SCHEDULER_RULES = $previousRules
}
exit $result
