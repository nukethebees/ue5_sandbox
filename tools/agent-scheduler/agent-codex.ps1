$ErrorActionPreference = 'Stop'
$bin = Join-Path $env:LOCALAPPDATA 'NukeTheBees/agent-codex/bin'
$codex = Join-Path $bin 'codex-scheduler.exe'
$hostExecutable = Join-Path $bin 'codex-code-mode-host.exe'
$rules = Join-Path $env:LOCALAPPDATA 'NukeTheBees/config/agent-scheduler/scheduling.rules'
foreach ($component in @($codex, $hostExecutable, $rules)) {
    if (-not (Test-Path -LiteralPath $component -PathType Leaf)) {
        throw "Missing required component '$component'. Ask the maintainer to install/configure modified Codex."
    }
}
& $codex @args
exit $LASTEXITCODE
