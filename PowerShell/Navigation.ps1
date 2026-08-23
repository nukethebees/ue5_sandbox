$script:dev_project_root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

function Get-DevWorktree {
    $worktree_lines = & git -C $script:dev_project_root worktree list --porcelain

    if ($LASTEXITCODE -ne 0) {
        throw "Unable to discover Git worktrees from '$script:dev_project_root'."
    }

    $current_worktree = $null

    foreach ($line in $worktree_lines) {
        if ($line.StartsWith('worktree ')) {
            if ($null -ne $current_worktree) {
                [PSCustomObject]$current_worktree
            }

            $worktree_path = $line.Substring('worktree '.Length)
            $current_worktree = [ordered]@{
                Name = Split-Path -Leaf $worktree_path
                Path = $worktree_path
                Branch = $null
            }

            continue
        }

        if ($null -eq $current_worktree -or -not $line.StartsWith('branch ')) {
            continue
        }

        $branch = $line.Substring('branch '.Length)

        if ($branch.StartsWith('refs/heads/')) {
            $branch = $branch.Substring('refs/heads/'.Length)
        }

        $current_worktree.Branch = $branch
    }

    if ($null -ne $current_worktree) {
        [PSCustomObject]$current_worktree
    }
}

function Get-DevPlugin {
    $plugins_path = Join-Path $script:dev_project_root 'Plugins'

    Get-ChildItem -LiteralPath $plugins_path -Directory | ForEach-Object {
        [PSCustomObject]@{
            Name = $_.Name
            Path = $_.FullName
        }
    }
}

function dev-help {
    @'
Project development commands

Load these commands into the current PowerShell session by dot-sourcing this script:

    . .\dev.ps1

The leading dot matters: it keeps the project's functions available after the script finishes.

Commands:
  dev-help          Show this help text.
  croot             Change to this repository/worktree root.
  cwt <name>        Change to a Git worktree by directory name.
  cwb [branch]      Change to the worktree containing a branch, or list branches.
  cplugin <name>    Change to a project plugin directory.
  ctests            Change to Source\SandboxTests.
d=  reset-devs        Hard reset locally checkoutable devN branches to dev.

Unreal build commands:
  Loading dev.ps1 disables MSBuild node reuse for the current user and shell.
  enable-ubt-build-safety  Persist disabled MSBuild node reuse for the current user.
  get-ubt-build-state      List Sandbox CMake, UBT, and UE MSBuild processes.
  reset-ubt-build-state    Shut down orphan UE MSBuild workers when no build is active.
                           Pass -Force to stop matching active Sandbox build trees first.

Run .\dev.ps1 --help to view this help without loading commands into your session.
'@
}

function croot {
    Set-Location -LiteralPath $script:dev_project_root
}

function cwt {
    param(
        [Parameter(Mandatory)]
        [ArgumentCompleter({
                param($command_name, $parameter_name, $word_to_complete)

                Get-DevWorktree |
                    Where-Object { $_.Name -like "$word_to_complete*" } |
                    ForEach-Object {
                        [System.Management.Automation.CompletionResult]::new(
                            $_.Name,
                            $_.Name,
                            'ParameterValue',
                            $_.Path)
                    }
            })]
        [string]$name
    )

    $matching_worktrees = @(Get-DevWorktree | Where-Object { $_.Name -eq $name })

    if ($matching_worktrees.Count -eq 0) {
        throw "Unknown worktree '$name'. Available worktrees: $((Get-DevWorktree).Name -join ', ')."
    }

    if ($matching_worktrees.Count -gt 1) {
        throw "Worktree name '$name' is ambiguous: $($matching_worktrees.Path -join ', ')."
    }

    Set-Location -LiteralPath $matching_worktrees[0].Path
}

function cwb {
    param(
        [ArgumentCompleter({
                param($command_name, $parameter_name, $word_to_complete)

                Get-DevWorktree |
                    Where-Object {
                        -not [string]::IsNullOrWhiteSpace($_.Branch) -and
                        $_.Branch -like "$word_to_complete*"
                    } |
                    ForEach-Object {
                        [System.Management.Automation.CompletionResult]::new(
                            $_.Branch,
                            $_.Branch,
                            'ParameterValue',
                            $_.Path)
                    }
            })]
        [string]$branch
    )

    $worktrees = @(Get-DevWorktree | Where-Object { -not [string]::IsNullOrWhiteSpace($_.Branch) })

    if ([string]::IsNullOrWhiteSpace($branch)) {
        $worktrees | Select-Object Branch, Path
        return
    }

    $matching_worktrees = @($worktrees | Where-Object { $_.Branch -eq $branch })

    if ($matching_worktrees.Count -eq 0) {
        throw "Branch '$branch' is not checked out in a worktree."
    }

    if ($matching_worktrees.Count -gt 1) {
        throw "Branch '$branch' is checked out in multiple worktrees: $($matching_worktrees.Path -join ', ')."
    }

    Set-Location -LiteralPath $matching_worktrees[0].Path
}

function cplugin {
    param(
        [Parameter(Mandatory)]
        [ArgumentCompleter({
                param($command_name, $parameter_name, $word_to_complete)

                Get-DevPlugin |
                    Where-Object { $_.Name -like "$word_to_complete*" } |
                    ForEach-Object {
                        [System.Management.Automation.CompletionResult]::new(
                            $_.Name,
                            $_.Name,
                            'ParameterValue',
                            $_.Path)
                    }
            })]
        [string]$name
    )

    $matching_plugins = @(Get-DevPlugin | Where-Object { $_.Name -eq $name })

    if ($matching_plugins.Count -eq 0) {
        throw "Unknown plugin '$name'. Available plugins: $((Get-DevPlugin).Name -join ', ')."
    }

    Set-Location -LiteralPath $matching_plugins[0].Path
}

function ctests {
    Set-Location -LiteralPath (Join-Path $script:dev_project_root 'Source\SandboxTests')
}

function reset-devs {
    $dev_commit_lines = @(& git -C $script:dev_project_root rev-parse --verify 'refs/heads/dev^{commit}')

    if ($LASTEXITCODE -ne 0 -or $dev_commit_lines.Count -ne 1) {
        throw "Unable to resolve local branch 'dev' to a commit."
    }

    $dev_commit = $dev_commit_lines[0].Trim()
    $branch_lines = @(& git -C $script:dev_project_root for-each-ref '--format=%(refname:short)' refs/heads/)

    if ($LASTEXITCODE -ne 0) {
        throw "Unable to discover local branches from '$script:dev_project_root'."
    }

    $dev_branches = @($branch_lines | Where-Object { $_ -match '^dev[0-9]+$' } | Sort-Object)

    if ($dev_branches.Count -eq 0) {
        Write-Host 'No local devN branches found.'
        return
    }

    $original_branch_lines = @(& git -C $script:dev_project_root branch --show-current)

    if ($LASTEXITCODE -ne 0) {
        throw "Unable to determine the current branch in '$script:dev_project_root'."
    }

    $original_branch = if ($original_branch_lines.Count -eq 1) {
        $original_branch_lines[0].Trim()
    } else {
        ''
    }

    $original_commit_lines = @(& git -C $script:dev_project_root rev-parse --verify HEAD)

    if ($LASTEXITCODE -ne 0 -or $original_commit_lines.Count -ne 1) {
        throw "Unable to resolve HEAD in '$script:dev_project_root'."
    }

    $original_commit = $original_commit_lines[0].Trim()

    try {
        foreach ($branch in $dev_branches) {
            & git -C $script:dev_project_root switch $branch

            if ($LASTEXITCODE -ne 0) {
                Write-Host "Skipping $branch because it could not be checked out in the current worktree."
                continue
            }

            Write-Host "Resetting $branch to dev ($dev_commit)."
            & git -C $script:dev_project_root reset --hard $dev_commit

            if ($LASTEXITCODE -ne 0) {
                throw "Unable to reset branch '$branch' to dev."
            }
        }
    } finally {
        if ([string]::IsNullOrWhiteSpace($original_branch)) {
            & git -C $script:dev_project_root switch --detach $original_commit
        } else {
            & git -C $script:dev_project_root switch $original_branch
        }

        if ($LASTEXITCODE -ne 0) {
            throw "Unable to restore the original Git state in '$script:dev_project_root'."
        }
    }
}
