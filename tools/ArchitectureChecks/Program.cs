namespace ArchitectureChecks;

public static class Program
{
    public static int Main(string[] arguments)
    {
        if (IsModuleMigrationHelp(arguments))
        {
            WriteModuleMigrationUsage(Console.Out);
            return 0;
        }

        if (TryParse(arguments, out var request))
        {
            return RunSpaceGameLayerCheck(request!);
        }

        if (TryParseModuleMigration(arguments, out var module_migration_request))
        {
            return RunModuleMigrationCheck(module_migration_request!);
        }

        Console.Error.WriteLine("Usage: ArchitectureChecks --root <path>");
        Console.Error.WriteLine("       ArchitectureChecks module-migration --help");
        return 2;
    }

    private static int RunSpaceGameLayerCheck(ArchitectureCheckRequest request)
    {
        try
        {
            var result = new SpaceGameLayerChecker().Check(request);
            if (result.IsValid)
            {
                Console.Out.WriteLine("SpaceGame module boundaries are valid.");
                return 0;
            }

            foreach (var diagnostic in result.Diagnostics)
            {
                Console.Out.WriteLine(diagnostic);
            }

            return 1;
        }
        catch (Exception exception) when (exception is ArgumentException or DirectoryNotFoundException or IOException or UnauthorizedAccessException)
        {
            Console.Error.WriteLine($"ArchitectureChecks: {exception.Message}");
            return 1;
        }
    }

    private static int RunModuleMigrationCheck(ModuleMigrationRequest request)
    {
        try
        {
            var result = new ModuleMigrationChecker().CheckAsync(request, CancellationToken.None).GetAwaiter().GetResult();
            ModuleMigrationRenderer.Render(result, Console.Out);
            return 0;
        }
        catch (Exception exception) when (exception is ArgumentException or DirectoryNotFoundException or IOException or UnauthorizedAccessException or ModuleMigrationException)
        {
            Console.Error.WriteLine($"ArchitectureChecks: {exception.Message}");
            return 1;
        }
    }

    internal static bool TryParse(IReadOnlyList<string> arguments, out ArchitectureCheckRequest? request)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        request = null;
        if (arguments.Count != 2 ||
            !string.Equals(arguments[0], "--root", StringComparison.Ordinal) ||
            string.IsNullOrWhiteSpace(arguments[1]))
        {
            return false;
        }

        request = new ArchitectureCheckRequest(arguments[1]);
        return true;
    }

    internal static bool TryParseModuleMigration(
        IReadOnlyList<string> arguments,
        out ModuleMigrationRequest? request)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        request = null;
        if (arguments.Count == 0 || !string.Equals(arguments[0], "module-migration", StringComparison.Ordinal))
        {
            return false;
        }

        string? root_path = null;
        var baseline = "HEAD";
        var old_module = "Sandbox";
        var plugin_modules = new List<string> { "ShooterGame", "SandboxGameShared" };
        var has_baseline = false;
        var has_old_module = false;

        for (var index = 1; index < arguments.Count; ++index)
        {
            var option = arguments[index];
            if (index + 1 >= arguments.Count)
            {
                return false;
            }

            var value = arguments[++index];
            if (string.IsNullOrWhiteSpace(value))
            {
                return false;
            }

            switch (option)
            {
                case "--root" when root_path is null:
                    root_path = value;
                    break;
                case "--baseline" when !has_baseline:
                    baseline = value;
                    has_baseline = true;
                    break;
                case "--old-module" when !has_old_module:
                    old_module = value;
                    has_old_module = true;
                    break;
                case "--plugin-module":
                    plugin_modules.Add(value);
                    break;
                default:
                    return false;
            }
        }

        if (string.IsNullOrWhiteSpace(root_path))
        {
            return false;
        }

        if (!IsModuleName(old_module) || plugin_modules.Count == 0 || plugin_modules.Any(module => !IsModuleName(module)))
        {
            return false;
        }

        plugin_modules = plugin_modules.Distinct(StringComparer.Ordinal).ToList();

        request = new ModuleMigrationRequest(root_path, baseline, old_module, plugin_modules);
        return true;
    }

    internal static bool IsModuleMigrationHelp(IReadOnlyList<string> arguments)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        return arguments.Count == 2 &&
               string.Equals(arguments[0], "module-migration", StringComparison.Ordinal) &&
               (string.Equals(arguments[1], "--help", StringComparison.Ordinal) ||
                string.Equals(arguments[1], "-h", StringComparison.Ordinal));
    }

    internal static void WriteModuleMigrationUsage(TextWriter output)
    {
        ArgumentNullException.ThrowIfNull(output);

        output.WriteLine("Usage: ArchitectureChecks module-migration --root <path> [--baseline <revision>] [--old-module <module>] [--plugin-module <module>]...");
        output.WriteLine("Defaults: old module Sandbox; plugin modules ShooterGame and SandboxGameShared.");
        output.WriteLine("Each --plugin-module argument adds to the default plugin modules; duplicates are ignored.");
        output.WriteLine("The audit is advisory: findings do not cause a failure exit code.");
    }

    private static bool IsModuleName(string value)
    {
        if (string.IsNullOrWhiteSpace(value) || !(char.IsAsciiLetter(value[0]) || value[0] == '_'))
        {
            return false;
        }

        return value.All(character => char.IsAsciiLetterOrDigit(character) || character == '_');
    }
}
