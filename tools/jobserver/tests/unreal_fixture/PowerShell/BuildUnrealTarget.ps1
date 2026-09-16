param(
    [Parameter(Mandatory)]
    [string]$build_script,

    [Parameter(Mandatory)]
    [string]$target,

    [Parameter(Mandatory)]
    [string]$platform,

    [string]$configuration,
    [string]$project,
    [string]$native_toolchain
)

& $build_script $target $platform
exit $LASTEXITCODE
