namespace AgentGitInstaller;

public static class Program
{
    public static async Task<int> Main(string[] arguments)
    {
        if (!CommandLine.TryParse(arguments, out var request, out var show_help, out var error))
        {
            Console.Error.WriteLine($"agent-git-installer: {error}");
            Console.Error.WriteLine(CommandLine.Usage);
            return ExitCodes.Usage;
        }

        if (show_help)
        {
            Console.WriteLine(CommandLine.Usage);
            return ExitCodes.Success;
        }

        var application = new AgentGitInstallerApplication(
            new ProcessRunner(),
            new ExecutableLocator(),
            new InstallerFileSystem(),
            new Sha256ArtifactHasher(),
            Console.Out);
        try
        {
            await application.InstallAsync(request!, CancellationToken.None);
            return ExitCodes.Success;
        }
        catch (Exception exception) when (exception is InstallerException or AgentGit.AgentGitException or
            IOException or UnauthorizedAccessException or System.Text.Json.JsonException)
        {
            Console.Error.WriteLine($"agent-git-installer: {exception.Message}");
            return ExitCodes.Failure;
        }
    }
}
