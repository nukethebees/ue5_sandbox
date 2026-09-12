[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$settings_path
)

if (-not (Test-Path -LiteralPath $settings_path -PathType Leaf)) {
    return
}

$resolved_settings_path = (Resolve-Path -LiteralPath $settings_path).Path
$utf8_without_bom = [System.Text.UTF8Encoding]::new($false, $true)
$reader = [System.IO.StreamReader]::new($resolved_settings_path, $utf8_without_bom, $true)

try {
    $contents = $reader.ReadToEnd()
    $encoding = $reader.CurrentEncoding
} finally {
    $reader.Dispose()
}

$parts = [regex]::Split($contents, "(`r`n|`n|`r)")
$in_live_coding_section = $false
$changed = $false

for ($index = 0; $index -lt $parts.Count; $index += 2) {
    $line = $parts[$index]

    if ($line -match '^\s*\[([^\]]+)\]\s*$') {
        $in_live_coding_section = $Matches[1] -ieq '/Script/LiveCoding.LiveCodingSettings'
        continue
    }

    if (-not $in_live_coding_section) {
        continue
    }

    $updated_line = [regex]::Replace(
        $line,
        '^(\s*bEnabled\s*=\s*)[^;#\s]+(.*)$',
        '${1}False$2',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)

    if ($updated_line -cne $line) {
        $parts[$index] = $updated_line
        $changed = $true
    }
}

if ($changed) {
    [System.IO.File]::WriteAllText($resolved_settings_path, [string]::Concat($parts), $encoding)
    Write-Host "Disabled Live Coding in '$resolved_settings_path'."
}
