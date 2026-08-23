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
