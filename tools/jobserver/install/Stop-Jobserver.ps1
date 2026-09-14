[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$ClientPath
)

if (-not (Test-Path -LiteralPath $ClientPath -PathType Leaf)) {
    return
}

& $ClientPath shutdown
if ($LASTEXITCODE -ne 0) {
    throw 'The running jobserver could not be drained for update.'
}
