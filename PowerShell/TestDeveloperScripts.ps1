[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$scripts = @(
    (Join-Path $root 'dev.ps1')
    (Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1' -File | Select-Object -ExpandProperty FullName)
)

$failed = $false
foreach ($script_path in $scripts) {
    $tokens = $null
    $errors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        $script_path,
        [ref]$tokens,
        [ref]$errors)
    foreach ($parse_error in $errors) {
        Write-Error "${script_path}:$($parse_error.Extent.StartLineNumber): $($parse_error.Message)"
        $failed = $true
    }
}

if ($failed) {
    exit 1
}

Write-Host "PowerShell syntax checks passed for $($scripts.Count) developer scripts."
