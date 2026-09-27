$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../Navigation.ps1"

function Assert-Equal($actual, $expected) {
    if ($actual -cne $expected) { throw "Expected '$expected', got '$actual'." }
}

$porcelain = "worktree C:/path with spaces/雪'work`nname`0HEAD abc`0branch refs/heads/topic/one`0locked maintenance`0`0" +
    "worktree C:/detached`0HEAD def`0detached`0`0worktree C:/bare`0bare`0`0"
$parsed = @(ConvertFrom-DevWorktreePorcelain $porcelain)
Assert-Equal $parsed.Count 3
Assert-Equal $parsed[0].Path "C:/path with spaces/雪'work`nname"
Assert-Equal $parsed[0].Name "雪'work`nname"
Assert-Equal $parsed[0].Branch 'topic/one'
Assert-Equal $parsed[1].Branch $null
Assert-Equal $parsed[2].Branch $null

# The current checkout may itself be a linked worktree.
$current = @(Get-DevWorktree | Where-Object { [IO.Path]::GetFullPath($_.Path) -eq $script:dev_project_root })
Assert-Equal $current.Count 1
Push-Location
try {
    cwt $current[0].Name
    Assert-Equal (Get-Location).Path $script:dev_project_root
    if ($current[0].Branch) {
        cwb $current[0].Branch
        Assert-Equal (Get-Location).Path $script:dev_project_root
        Assert-Equal (@(cwb | Where-Object Branch -eq $current[0].Branch).Count) 1
    }
} finally { Pop-Location }

function Get-DevWorktree { $parsed }
$completion = [Management.Automation.CommandCompletion]::CompleteInput('cwt ', 4, $null)
Assert-Equal $completion.CompletionMatches[0].CompletionText "'雪''work`nname'"
$completion = [Management.Automation.CommandCompletion]::CompleteInput('cwb topic', 9, $null)
Assert-Equal $completion.CompletionMatches[0].CompletionText "'topic/one'"
Write-Host 'Navigation tests passed.'
