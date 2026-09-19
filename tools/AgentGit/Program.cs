namespace AgentGit;

public static class Program
{
    public static async Task<int> Main(string[] arguments)
    {
        if (!CommandLine.TryParse(arguments, out var request, out var error))
        {
            Console.Error.WriteLine($"agent-git: {error}");
            Console.Error.WriteLine(CommandLine.Usage);
            return ExitCodes.Usage;
        }

        if (request is HelpRequest or VersionRequest)
        {
            var placeholder = CreatePlaceholderApplication();
            return await placeholder.RunAsync(request, Environment.CurrentDirectory);
        }

        TrustContext trust;
        try
        {
            var process_path = Environment.ProcessPath
                ?? throw new PolicyConfigurationException("Unable to determine the agent-git executable path.");
            trust = TrustStore.LoadCanonical(process_path);
        }
        catch (AgentGitException exception)
        {
            Console.Error.WriteLine($"agent-git: {exception.Message}");
            return exception.ExitCode;
        }

        var process_runner = new ProcessRunner();
        var git = new GitClient(trust, process_runner);
        var discovery = new RepositoryDiscovery(git);
        var application = new AgentGitApplication(
            trust,
            discovery,
            new PolicyEvaluator(discovery),
            new OperationExecutor(git),
            Console.Out,
            Console.Error);
        return await application.RunAsync(request!, Environment.CurrentDirectory);
    }

    private static AgentGitApplication CreatePlaceholderApplication()
    {
        var root = Path.GetTempPath();
        var trust = new TrustContext(root, "git", null, "unused", "unused@example.invalid",
            Path.Combine(root, "unused"), Path.Combine(root, "unused-hooks"), []);
        var git = new GitClient(trust, new ProcessRunner());
        var discovery = new RepositoryDiscovery(git);
        return new AgentGitApplication(
            trust,
            discovery,
            new PolicyEvaluator(discovery),
            new OperationExecutor(git),
            Console.Out,
            Console.Error);
    }
}
