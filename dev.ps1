[CmdletBinding()]
param(
    [switch]$Help,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArguments
)

if ([string]::IsNullOrWhiteSpace($env:UE_ROOT)) {
    Write-Warning 'UE_ROOT should be defined and point to the Unreal Engine installation root.'
} else {
    $unreal_binary_path = Join-Path $env:UE_ROOT 'Engine\Binaries\Win64'

    if ($env:PATH -split [System.IO.Path]::PathSeparator -notcontains $unreal_binary_path) {
        $env:PATH = $unreal_binary_path + [System.IO.Path]::PathSeparator + $env:PATH
    }
}

$navigation_path = Join-Path $PSScriptRoot 'PowerShell\Navigation.ps1'
$unreal_build_path = Join-Path $PSScriptRoot 'PowerShell\UnrealBuild.ps1'

if (-not (Test-Path -LiteralPath $navigation_path -PathType Leaf)) {
    throw "Development command module was not found: $navigation_path"
}

if (-not (Test-Path -LiteralPath $unreal_build_path -PathType Leaf)) {
    throw "Unreal build command module was not found: $unreal_build_path"
}

. $navigation_path
. $unreal_build_path

$show_help = $Help -or ($RemainingArguments.Count -eq 1 -and $RemainingArguments[0] -eq '--help')

if (-not $show_help -and $RemainingArguments.Count -gt 0) {
    throw "Unknown dev.ps1 argument: $($RemainingArguments -join ' '). Use '.\\dev.ps1 --help' for usage."
}

if ($show_help) {
    dev-help
} elseif ($MyInvocation.InvocationName -eq '.') {
    Write-Host "Loaded project PowerShell commands. Run 'dev-help' for available commands."
}
