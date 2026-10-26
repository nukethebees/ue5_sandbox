param(
    [Parameter(Mandatory)][string]$EngineRoot,
    [ValidateRange(1, 100000)][int]$CapacityGB = 100
)

$ErrorActionPreference = 'Stop'

function Test-CacheListening {
    $client = [System.Net.Sockets.TcpClient]::new()
    try {
        $client.Connect('127.0.0.1', 1347)
        return $true
    }
    catch [System.Net.Sockets.SocketException] {
        return $false
    }
    finally {
        $client.Dispose()
    }
}

# Serialize startup across builds; the service persists between build invocations.
$mutex = [System.Threading.Mutex]::new($false, 'Local\IOJ-UbaCacheService-1347')
$locked = $false
try {
    try {
        $locked = $mutex.WaitOne(30000)
    }
    catch [System.Threading.AbandonedMutexException] {
        $locked = $true
    }
    if (-not $locked) {
        throw 'Timed out waiting for local UBA cache startup.'
    }
    if (Test-CacheListening) {
        return
    }

    $executable = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealBuildAccelerator/x64/UbaCacheService.exe'
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "UE installation is missing its cache service: $executable"
    }
    $service = Start-Process -FilePath $executable -ArgumentList @(
        '-port=127.0.0.1:1347', "-capacity=$CapacityGB"
    ) -WindowStyle Hidden -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (-not (Test-CacheListening)) {
        if ($service.HasExited) {
            throw "UBA cache service exited with code $($service.ExitCode)."
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            throw 'UBA cache service did not start listening within 15 seconds.'
        }
        Start-Sleep -Milliseconds 100
    }
    Write-Host "Started local UBA cache (PID $($service.Id), $CapacityGB GB, default storage directory)."
}
finally {
    if ($locked) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
