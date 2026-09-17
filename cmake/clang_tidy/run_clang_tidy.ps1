[CmdletBinding()]
param(
  [Parameter(Mandatory)]
  [string]$PythonExecutable,
  [Parameter(Mandatory)]
  [string]$RunClangTidyExecutable,
  [Parameter(Mandatory)]
  [string]$LogFile,
  [Parameter(Mandatory)]
  [string]$CompilationDatabase,
  [Parameter(Mandatory)]
  [string]$Checks,
  [Parameter(Mandatory)]
  [int]$Jobs,
  [Parameter(Mandatory)]
  [string]$SourceFilter
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$log_directory = Split-Path -Parent $LogFile
New-Item -ItemType Directory -Force -Path $log_directory | Out-Null

$clang_tidy_arguments = @(
  "-p",
  $CompilationDatabase,
  "-checks=-*,$Checks",
  "-extra-arg-before=/EHsc",
  "-j",
  $Jobs,
  $SourceFilter
)

& $PythonExecutable $RunClangTidyExecutable @clang_tidy_arguments *>&1 |
  Tee-Object -FilePath $LogFile
$exit_code = $LASTEXITCODE

if ($null -eq $exit_code) {
  $exit_code = 1
}
exit $exit_code
