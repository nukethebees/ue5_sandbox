namespace AgentGitInstaller;

internal static class CommandLine
{
    public const string Usage = """
        Usage:
          agent-git-installer --source-root <path> --repository <path> --base-branch <branch>
            [--policy-path <repository-path>] [--install-root <path>]

        Test-only controls (temporary non-canonical installations only):
            [--test-validation-project <path>]
            [--test-skip-validation --test-artifact-root <path>]
            [--test-corrupt-validated-artifact] [--test-fail-post-install]
        """;

    public static bool TryParse(
        IReadOnlyList<string> arguments,
        out InstallerRequest? request,
        out bool show_help,
        out string? error)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        request = null;
        show_help = false;
        error = null;
        if (arguments is ["--help"] or ["-h"])
        {
            show_help = true;
            return true;
        }

        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        var switches = new HashSet<string>(StringComparer.Ordinal);
        for (var index = 0; index < arguments.Count; ++index)
        {
            var option = arguments[index];
            if (option is "--test-skip-validation" or "--test-corrupt-validated-artifact" or
                "--test-fail-post-install")
            {
                if (!switches.Add(option))
                {
                    error = $"Option '{option}' was specified more than once.";
                    return false;
                }

                continue;
            }

            if (option is not ("--source-root" or "--repository" or "--base-branch" or
                "--policy-path" or "--install-root" or "--test-validation-project" or
                "--test-artifact-root"))
            {
                error = $"Unknown option '{option}'.";
                return false;
            }

            if (index + 1 >= arguments.Count || string.IsNullOrWhiteSpace(arguments[index + 1]))
            {
                error = $"Option '{option}' requires a non-empty value.";
                return false;
            }

            if (!values.TryAdd(option, arguments[++index]))
            {
                error = $"Option '{option}' was specified more than once.";
                return false;
            }
        }

        if (!values.TryGetValue("--source-root", out var source_root) ||
            !values.TryGetValue("--repository", out var repository) ||
            !values.TryGetValue("--base-branch", out var base_branch))
        {
            error = "--source-root, --repository, and --base-branch are required.";
            return false;
        }

        request = new InstallerRequest(
            source_root,
            repository,
            base_branch,
            values.GetValueOrDefault("--policy-path") ?? ".agent-git.json",
            values.GetValueOrDefault("--install-root"),
            values.GetValueOrDefault("--test-validation-project"),
            switches.Contains("--test-skip-validation"),
            values.GetValueOrDefault("--test-artifact-root"),
            switches.Contains("--test-corrupt-validated-artifact"),
            switches.Contains("--test-fail-post-install"));
        return true;
    }
}
