$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$checkout = Join-Path $repo '.local/codex-upstream'
$revision = (Get-Content -LiteralPath "$PSScriptRoot/upstream-revision.txt" -Raw).Trim()
$patch = Join-Path $PSScriptRoot 'codex.patch'
if (-not (Test-Path -LiteralPath $checkout)) {
    git clone --no-checkout --config core.longpaths=true --filter=blob:none https://github.com/openai/codex.git $checkout
    if ($LASTEXITCODE -ne 0) { throw 'Codex clone failed.' }
    git -C $checkout checkout --detach $revision
    if ($LASTEXITCODE -ne 0) { throw 'Pinned Codex checkout failed.' }
}
$actual = git -C $checkout rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual.Trim() -ne $revision) { throw 'Codex checkout does not match upstream-revision.txt; preserve it and prepare the pinned revision separately.' }
git -C $checkout -c core.longpaths=true apply --reverse --check $patch 2>$null
if ($LASTEXITCODE -eq 0) { Write-Output 'Pinned scheduler patch is already applied.'; exit 0 }
git -C $checkout -c core.longpaths=true apply --check $patch
if ($LASTEXITCODE -ne 0) { throw 'Scheduler patch does not apply cleanly.' }
git -C $checkout -c core.longpaths=true apply $patch
if ($LASTEXITCODE -ne 0) { throw 'Scheduler patch failed.' }
Write-Output 'Pinned scheduler-enabled Codex prepared.'
