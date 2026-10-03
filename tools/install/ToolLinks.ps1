function Get-IojRoot {
    if (-not $env:IOJ_ROOT -or -not [IO.Path]::IsPathFullyQualified($env:IOJ_ROOT)) {
        throw 'Set IOJ_ROOT to an absolute directory for shared tools and temporary files.'
    }
    return [IO.Path]::GetFullPath($env:IOJ_ROOT)
}

function Get-ToolLinkDirectory([string]$InstallRoot, [string]$Directory) {
    if ($Directory) { return [IO.Path]::GetFullPath($Directory) }

    $parent = Split-Path -Parent ([IO.Path]::GetFullPath($InstallRoot).TrimEnd('\', '/'))
    return Join-Path $parent 'bin'
}

function Assert-ToolLinkSupport([string]$Directory) {
    $probe = Join-Path $Directory ".symlink-check-$([Guid]::NewGuid().ToString('N'))"
    try {
        New-Item -ItemType Directory -Path $Directory -Force | Out-Null
        New-Item -ItemType SymbolicLink -Path $probe -Target $PSCommandPath | Out-Null
    } catch {
        throw "Cannot create tool symlinks in '$Directory'. Enable Windows Developer Mode, or grant your account the 'Create symbolic links' user right and sign out/in. Also ensure this directory is writable. No build or installation has started. Windows reported: $($_.Exception.Message)"
    } finally {
        if (Test-Path -LiteralPath $probe) { Remove-Item -LiteralPath $probe -Force }
    }
}

function Publish-ToolLinks([string]$Bin, [string[]]$Names, [string]$Directory) {
    foreach ($name in $Names) {
        $target = Join-Path ([IO.Path]::GetFullPath($Bin)) $name
        if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "Missing tool for symlink publication: '$target'." }
        $link = Join-Path $Directory $name
        $existing = Get-Item -LiteralPath $link -Force -ErrorAction SilentlyContinue
        if ($existing) {
            if ($existing.LinkType -eq 'SymbolicLink' -and $existing.LinkTarget -eq $target) { continue }
            throw "Cannot publish '$link': an existing file or link belongs to another target. Move it aside explicitly, then rerun the installer. Expected target: '$target'."
        }
        try {
            New-Item -ItemType SymbolicLink -Path $link -Target $target | Out-Null
        } catch {
            throw "Tool installed at '$target', but publishing symlink '$link' failed. Check Developer Mode and directory permissions, then rerun the installer. Windows reported: $($_.Exception.Message)"
        }
    }
    Write-Host "Tool links are in '$Directory'. Keep this one directory on PATH."
}

function Remove-RetiredTool([string]$Bin, [string]$Name, [string]$Directory) {
    $target = Join-Path ([IO.Path]::GetFullPath($Bin)) $Name
    $link = Join-Path $Directory $Name
    $existing = Get-Item -LiteralPath $link -Force -ErrorAction SilentlyContinue
    if ($existing -and ($existing.LinkType -ne 'SymbolicLink' -or $existing.LinkTarget -ne $target)) {
        throw "Cannot retire '$link': it belongs to another target. Remove it explicitly. Expected target: '$target'."
    }

    # Remove only the retired executable and its verified link, retaining unrelated tools.
    if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
    if ($existing) { Remove-Item -LiteralPath $link -Force }
}
