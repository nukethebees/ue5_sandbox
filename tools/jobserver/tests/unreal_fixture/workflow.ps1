param([string]$SourceRoot)
$script:dev_project_root = $PSScriptRoot
. "$SourceRoot/PowerShell/UnrealBuild.ps1"
function Get-JobserverPath { $env:FIXTURE_CLIENT }
Push-Location -LiteralPath $PSScriptRoot
try {
    Invoke-JobserverWorkflow -Name 'fixture workflow' -Preset fixture
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
