param(
    [Parameter(ValueFromRemainingArguments)]
    [string[]]$arguments
)

$ErrorActionPreference = 'Stop'

for ($index = 0; $index -lt $arguments.Count; ++$index) {
    switch ($arguments[$index]) {
        '--build-script' {
            $build_script = $arguments[++$index]
        }
        '--target' {
            $target = $arguments[++$index]
        }
        '--platform' {
            $platform = $arguments[++$index]
        }
    }
}

& $build_script $target $platform
exit $LASTEXITCODE
