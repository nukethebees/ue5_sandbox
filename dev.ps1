[CmdletBinding()]
param(
    [switch]$Help,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArguments
)

$navigation_path = Join-Path $PSScriptRoot 'PowerShell\Navigation.ps1'

if (-not (Test-Path -LiteralPath $navigation_path -PathType Leaf)) {
    throw "Development command module was not found: $navigation_path"
}

. $navigation_path

$show_help = $Help -or ($RemainingArguments.Count -eq 1 -and $RemainingArguments[0] -eq '--help')

if (-not $show_help -and $RemainingArguments.Count -gt 0) {
    throw "Unknown dev.ps1 argument: $($RemainingArguments -join ' '). Use '.\\dev.ps1 --help' for usage."
}

if ($show_help) {
    dev-help
} elseif ($MyInvocation.InvocationName -eq '.') {
    Write-Host "Loaded project PowerShell commands. Run 'dev-help' for available commands."
}
