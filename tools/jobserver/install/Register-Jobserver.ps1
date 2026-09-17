[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$DaemonPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedDaemon = (Resolve-Path -LiteralPath $DaemonPath -ErrorAction Stop).Path
$taskName = 'NukeTheBeesJobserver'
$action = New-ScheduledTaskAction -Execute $resolvedDaemon
$trigger = New-ScheduledTaskTrigger -AtLogOn -User "$env:USERDOMAIN\$env:USERNAME"
$principal = New-ScheduledTaskPrincipal `
    -UserId "$env:USERDOMAIN\$env:USERNAME" `
    -LogonType Interactive `
    -RunLevel Limited
$settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -MultipleInstances IgnoreNew

Register-ScheduledTask `
    -TaskName $taskName `
    -Action $action `
    -Trigger $trigger `
    -Principal $principal `
    -Settings $settings `
    -Force | Out-Null

Start-ScheduledTask -TaskName $taskName
Write-Host "Registered and started per-user scheduled task '$taskName'."
