$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$bin = Join-Path $repo '.local/scheduler-target/debug'
$codex = Join-Path $bin 'codex.exe'
if (-not (Test-Path -LiteralPath $codex)) { throw 'Build the codex-scheduler CMake target first.' }
$previousPath = $env:PATH
$previousRules = $env:AGENT_SCHEDULER_RULES
try {
    $env:PATH = "$bin;$previousPath"
    $env:AGENT_SCHEDULER_RULES = Join-Path $PSScriptRoot 'scheduling.rules'
    & $codex @args
    $result = $LASTEXITCODE
} finally {
    $env:PATH = $previousPath
    $env:AGENT_SCHEDULER_RULES = $previousRules
}
exit $result
