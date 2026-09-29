$ErrorActionPreference = 'Stop'
$scriptFile = Get-Item -LiteralPath $PSCommandPath
if ($scriptFile.LinkType -eq 'SymbolicLink') { $scriptFile = $scriptFile.ResolveLinkTarget($true) }
$scriptRoot = $scriptFile.DirectoryName
$bin = $scriptRoot
$codex = Join-Path $bin 'codex-scheduler.exe'
if (Test-Path -LiteralPath "$scriptRoot/Cargo.toml") {
    # The source launcher uses the local development build; installed files are siblings.
    $repo = (Resolve-Path "$scriptRoot/../..").Path
    $bin = Join-Path $repo '.local/scheduler-target/debug'
    $codex = Join-Path $bin 'codex.exe'
}
if (-not (Test-Path -LiteralPath $codex)) { throw "Missing '$codex'. Build codex-scheduler locally or rerun the central installer." }
$previousPath = $env:PATH
$previousRules = $env:AGENT_SCHEDULER_RULES
try {
    $env:PATH = "$bin;$previousPath"
    $env:AGENT_SCHEDULER_RULES = Join-Path $scriptRoot 'scheduling.rules'
    & $codex -c 'windows.sandbox="unelevated"' -c features.shell_snapshot=false @args
    $result = $LASTEXITCODE
} finally {
    $env:PATH = $previousPath
    $env:AGENT_SCHEDULER_RULES = $previousRules
}
exit $result
