$ErrorActionPreference = 'Stop'

$normalizer_path = Join-Path $PSScriptRoot 'Set-LiveCodingDisabled.ps1'
$temp_root = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$test_directory = [System.IO.Path]::GetFullPath(
    (Join-Path $temp_root "SandboxLiveCoding-$([Guid]::NewGuid().ToString('N'))"))

if (-not $test_directory.StartsWith($temp_root, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to create tests outside the temporary directory: $test_directory"
}

function Assert-Equal {
    param(
        [Parameter(Mandatory)]
        [string]$expected,
        [Parameter(Mandatory)]
        [string]$actual,
        [Parameter(Mandatory)]
        [string]$message
    )

    if ($actual -cne $expected) {
        throw "$message`nExpected:`n$expected`nActual:`n$actual"
    }
}

New-Item -ItemType Directory -Path $test_directory | Out-Null

try {
    $utf8_without_bom = [System.Text.UTF8Encoding]::new($false, $true)
    $settings_path = Join-Path $test_directory 'EditorPerProjectUserSettings.ini'
    $input = "[Other]`r`nbEnabled=True`r`n`r`n[/Script/LiveCoding.LiveCodingSettings]`r`nbEnabled = True ; keep`r`nbPreloadProjectModules=True`r`n`r`n[After]`r`nValue=Keep`r`n"
    $expected = "[Other]`r`nbEnabled=True`r`n`r`n[/Script/LiveCoding.LiveCodingSettings]`r`nbEnabled = False ; keep`r`nbPreloadProjectModules=True`r`n`r`n[After]`r`nValue=Keep`r`n"
    [System.IO.File]::WriteAllText($settings_path, $input, $utf8_without_bom)

    & $normalizer_path -settings_path $settings_path

    Assert-Equal $expected ([System.IO.File]::ReadAllText($settings_path)) `
        'The normalizer changed content outside the Live Coding bEnabled value.'

    $already_disabled = "[/Script/LiveCoding.LiveCodingSettings]`nbEnabled=False`n[Other]`nbEnabled=True`n"
    [System.IO.File]::WriteAllText($settings_path, $already_disabled, $utf8_without_bom)
    $before_bytes = [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($settings_path))

    & $normalizer_path -settings_path $settings_path

    $after_bytes = [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($settings_path))
    Assert-Equal $before_bytes $after_bytes 'An already-disabled file was rewritten.'

    $utf16 = [System.Text.UnicodeEncoding]::new($false, $true, $true)
    $utf16_input = "[/Script/LiveCoding.LiveCodingSettings]`r`nbEnabled=True`r`nName=Ångström`r`n"
    $utf16_expected = "[/Script/LiveCoding.LiveCodingSettings]`r`nbEnabled=False`r`nName=Ångström`r`n"
    [System.IO.File]::WriteAllText($settings_path, $utf16_input, $utf16)

    & $normalizer_path -settings_path $settings_path

    $bytes = [System.IO.File]::ReadAllBytes($settings_path)
    if ($bytes.Length -lt 2 -or $bytes[0] -ne 0xff -or $bytes[1] -ne 0xfe) {
        throw 'The normalizer did not preserve the UTF-16 little-endian BOM.'
    }
    Assert-Equal $utf16_expected ([System.IO.File]::ReadAllText($settings_path, $utf16)) `
        'The normalizer did not preserve UTF-16 content.'

    $missing_path = Join-Path $test_directory 'Missing.ini'
    & $normalizer_path -settings_path $missing_path
    if (Test-Path -LiteralPath $missing_path) {
        throw 'The normalizer created a missing saved user-settings file.'
    }

    Write-Host 'Live Coding saved-config normalization tests passed.'
} finally {
    Remove-Item -LiteralPath $test_directory -Recurse -Force
}
