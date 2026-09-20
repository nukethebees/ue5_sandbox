namespace AgentGitInstaller;

internal sealed record InstallerRequest(
    string SourceRoot,
    string Repository,
    string BaseBranch,
    string PolicyPath,
    string? InstallRoot,
    string? TestValidationProject,
    bool TestSkipValidation,
    string? TestArtifactRoot,
    bool TestCorruptValidatedArtifact,
    bool TestFailPostInstall);

internal sealed class InstallerException(string message, Exception? inner_exception = null)
    : Exception(message, inner_exception);

internal static class ExitCodes
{
    public const int Success = 0;
    public const int Failure = 1;
    public const int Usage = 2;
}
