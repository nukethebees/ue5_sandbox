[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Repository,

    [Parameter(Mandatory)]
    [string]$BaseBranch,

    [string]$PolicyPath = '.agent-git.json',

    [string]$InstallRoot
)

$ErrorActionPreference = 'Stop'

function Invoke-GitText {
    param(
        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    $lines = @(& $script:git_path @Arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "Git command failed with exit code ${LASTEXITCODE}: git $($Arguments -join ' ')"
    }

    ($lines -join "`n").Trim()
}

$repository_root = (Resolve-Path -LiteralPath $Repository).Path
$source_root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$staged_bin = Join-Path $source_root 'tools\bin'
$agent_git = Join-Path $staged_bin 'agent-git.exe'
if (-not (Test-Path -LiteralPath $agent_git -PathType Leaf)) {
    throw "Staged agent-git was not found at '$agent_git'. Run ctools first."
}

$git_command = Get-Command git.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1
$script:git_path = $git_command.Source
$version_text = & $script:git_path --version
if ($LASTEXITCODE -ne 0 -or $version_text -notmatch '^git version (?<major>\d+)\.(?<minor>\d+)') {
    throw "Unable to determine the trusted Git version from '$script:git_path'."
}

$git_version = [version]::new([int]$Matches.major, [int]$Matches.minor)
if ($git_version -lt [version]::new(2, 38)) {
    throw "agent-git requires Git 2.38 or newer; found $version_text."
}

$common_git_directory = Invoke-GitText @('-C', $repository_root, 'rev-parse', '--path-format=absolute', '--git-common-dir')
$origin_url = Invoke-GitText @('-C', $repository_root, 'config', '--local', '--get', 'remote.origin.url')
$user_name = Invoke-GitText @('-C', $repository_root, 'config', '--get', 'user.name')
$user_email = Invoke-GitText @('-C', $repository_root, 'config', '--get', 'user.email')
$policy_ref = "refs/heads/$BaseBranch"
$null = Invoke-GitText @('-C', $repository_root, 'check-ref-format', $policy_ref)
if ([string]::IsNullOrWhiteSpace($PolicyPath) -or
    $PolicyPath.StartsWith('/') -or
    $PolicyPath.EndsWith('/') -or
    $PolicyPath.Contains('\') -or
    $PolicyPath.Contains(':') -or
    $PolicyPath -match '[\x00-\x1f\x7f]' -or
    @($PolicyPath.Split('/') | Where-Object { $_ -in @('', '.', '..') }).Count -ne 0) {
    throw "PolicyPath must be a safe repository-relative Git path; found '$PolicyPath'."
}
$policy_json = Invoke-GitText @('-C', $repository_root, 'cat-file', 'blob', "${policy_ref}:$PolicyPath")
$policy = $policy_json | ConvertFrom-Json
if ($policy.version -ne 1) {
    throw "Policy version '$($policy.version)' is not supported."
}
if ($policy.baseBranch -cne $BaseBranch) {
    throw "Policy baseBranch '$($policy.baseBranch)' does not match requested base '$BaseBranch'."
}
if ([string]::IsNullOrWhiteSpace($policy.repositoryId)) {
    throw 'Policy repositoryId is missing.'
}

$git_lfs_path = $null
if (@($policy.gitExtensions) -ccontains 'lfs') {
    $git_lfs_command = Get-Command git-lfs.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1
    $git_lfs_path = $git_lfs_command.Source
}

if ([string]::IsNullOrWhiteSpace($InstallRoot)) {
    $install_parent = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'NukeTheBees'
    $install_root = Join-Path $install_parent 'agent-git'
} else {
    $install_root = [System.IO.Path]::GetFullPath($InstallRoot)
    $install_parent = Split-Path -Parent $install_root
}
$install_name = Split-Path -Leaf $install_root
$staging_root = Join-Path $install_parent "$install_name.staging.$([guid]::NewGuid().ToString('N'))"
$previous_root = Join-Path $install_parent "$install_name.previous"
$staging_bin = Join-Path $staging_root 'bin'
$staging_config = Join-Path $staging_root 'config'
$null = New-Item -ItemType Directory -Path $staging_bin -Force
$null = New-Item -ItemType Directory -Path (Join-Path $staging_config 'empty-hooks') -Force
$null = New-Item -ItemType File -Path (Join-Path $staging_config 'empty.gitconfig') -Force
$null = New-Item -ItemType File -Path (Join-Path $staging_config 'empty.attributes') -Force

$runtime_files = @(
    'agent-git.exe',
    'agent-git.dll',
    'agent-git.deps.json',
    'agent-git.runtimeconfig.json',
    'GitSupport.dll'
)
foreach ($file in $runtime_files) {
    $source = Join-Path $staged_bin $file
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required staged runtime file is missing: '$source'."
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $staging_bin $file)
}

$repositories = @()
if (Test-Path -LiteralPath (Join-Path $install_root 'trust.json') -PathType Leaf) {
    $existing = Get-Content -LiteralPath (Join-Path $install_root 'trust.json') -Raw | ConvertFrom-Json
    if ($existing.version -eq 1) {
        $repositories = @($existing.repositories | Where-Object {
                [System.IO.Path]::GetFullPath($_.commonGitDirectory) -ine [System.IO.Path]::GetFullPath($common_git_directory)
            })
    }
}

$repositories += [ordered]@{
    repositoryId = $policy.repositoryId
    commonGitDirectory = [System.IO.Path]::GetFullPath($common_git_directory)
    originUrl = $origin_url
    policyRef = $policy_ref
    policyPath = $PolicyPath
}
$manifest = [ordered]@{
    version = 1
    gitExecutable = $script:git_path
    gitLfsExecutable = $git_lfs_path
    userName = $user_name
    userEmail = $user_email
    repositories = $repositories
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $staging_root 'trust.json') -Encoding utf8NoBOM

try {
    if (Test-Path -LiteralPath $previous_root) {
        Remove-Item -LiteralPath $previous_root -Recurse -Force
    }
    if (Test-Path -LiteralPath $install_root) {
        Move-Item -LiteralPath $install_root -Destination $previous_root
    }
    Move-Item -LiteralPath $staging_root -Destination $install_root
} catch {
    if (-not (Test-Path -LiteralPath $install_root) -and (Test-Path -LiteralPath $previous_root)) {
        Move-Item -LiteralPath $previous_root -Destination $install_root
    }
    throw
} finally {
    if (Test-Path -LiteralPath $staging_root) {
        Remove-Item -LiteralPath $staging_root -Recurse -Force
    }
}

& (Join-Path $install_root 'bin\agent-git.exe') --version
if ($LASTEXITCODE -ne 0) {
    throw 'The installed agent-git executable failed its version smoke test.'
}

Write-Host "Installed agent-git at '$(Join-Path $install_root 'bin\agent-git.exe')'."
Write-Host "Registered repository '$($policy.repositoryId)' with policy ref '$policy_ref'."
