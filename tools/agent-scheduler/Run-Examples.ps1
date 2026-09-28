param([ValidateSet('all', 'lifecycle', 'leases')][string]$Only = 'all')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$bin = Join-Path $repo '.local/scheduler-target/debug'
$daemon = (Get-Command jobserverd -ErrorAction Stop).Source
$runId = [Guid]::NewGuid().ToString('N')
$pipe = "\\.\pipe\NukeTheBees.SchedulerExample.$runId"
$data = Join-Path $repo ".local/scheduler-examples/$runId"
$previousPipe = $env:NUKETHEBEES_JOBSERVER_TEST_PIPE
$previousData = $env:NUKETHEBEES_JOBSERVER_TEST_DATA
$previousPid = $env:SCHEDULER_EXAMPLE_DAEMON_PID
$process = $null
try {
    $env:NUKETHEBEES_JOBSERVER_TEST_PIPE = $pipe
    $env:NUKETHEBEES_JOBSERVER_TEST_DATA = $data
    $start = [System.Diagnostics.ProcessStartInfo]::new($daemon)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $process = [System.Diagnostics.Process]::Start($start)
    # Only daemon readiness is retried. Scheduling waits use pipe notifications.
    $ready = $false
    for ($attempt = 0; $attempt -lt 50; ++$attempt) {
        $probe = [System.IO.Pipes.NamedPipeClientStream]::new('.', "NukeTheBees.SchedulerExample.$runId")
        try { $probe.Connect(100); $ready = $true; break } catch { Start-Sleep -Milliseconds 50 } finally { $probe.Dispose() }
    }
    if (-not $ready) { throw 'Example daemon did not start.' }
    if ($Only -eq 'all') {
        & "$bin/examples/codex_flow.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Codex integration demonstration failed.' }
    }
    if ($Only -ne 'leases') {
        & "$bin/examples/codex_lifecycle.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Codex lifecycle demonstration failed.' }
    }
    if ($Only -ne 'lifecycle') {
        $env:SCHEDULER_EXAMPLE_DAEMON_PID = "$($process.Id)"
        & "$bin/examples/leases.exe" "$PSScriptRoot/scheduling.rules"
        if ($LASTEXITCODE -ne 0) { throw 'Lease demonstration failed.' }
    }
} finally {
    if ($null -ne $process) {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
    $env:NUKETHEBEES_JOBSERVER_TEST_PIPE = $previousPipe
    $env:NUKETHEBEES_JOBSERVER_TEST_DATA = $previousData
    $env:SCHEDULER_EXAMPLE_DAEMON_PID = $previousPid
}
