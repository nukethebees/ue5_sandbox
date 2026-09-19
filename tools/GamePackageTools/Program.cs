namespace GamePackageTools;

public static class Program
{
    public static async Task<int> Main(string[] arguments)
    {
        if (!TryParse(arguments, out var request))
        {
            Console.Error.WriteLine("Usage: GamePackageTools --project-root <path> --package-root <path> --unreal-pak <path> --verification-directory <path> --configuration <Development|Shipping>");
            return 2;
        }

        try
        {
            var verifier = new PackageVerifier(request!, Console.Out);
            await verifier.VerifyAsync(request!);
            return 0;
        }
        catch (Exception exception) when (exception is PackageVerificationException or IOException or UnauthorizedAccessException)
        {
            Console.Error.WriteLine($"GamePackageTools: {exception.Message}");
            return 1;
        }
    }

    internal static bool TryParse(IReadOnlyList<string> arguments, out PackageVerificationRequest? request)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        request = null;
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        for (var index = 0; index < arguments.Count; ++index)
        {
            var option = arguments[index];
            if (option is not ("--project-root" or "--package-root" or "--unreal-pak" or "--verification-directory" or "--configuration") ||
                index + 1 >= arguments.Count ||
                string.IsNullOrWhiteSpace(arguments[index + 1]) ||
                !values.TryAdd(option, arguments[++index]))
            {
                return false;
            }
        }

        if (!values.TryGetValue("--project-root", out var projectRoot) ||
            !values.TryGetValue("--package-root", out var packageRoot) ||
            !values.TryGetValue("--unreal-pak", out var unrealPakPath) ||
            !values.TryGetValue("--verification-directory", out var verificationDirectory) ||
            !values.TryGetValue("--configuration", out var configuration) ||
            !TryParseConfiguration(configuration, out var parsedConfiguration))
        {
            return false;
        }

        request = new PackageVerificationRequest(projectRoot, packageRoot, unrealPakPath, verificationDirectory, parsedConfiguration);
        return true;
    }

    private static bool TryParseConfiguration(string value, out PackageConfiguration configuration)
    {
        if (string.Equals(value, "Development", StringComparison.OrdinalIgnoreCase))
        {
            configuration = PackageConfiguration.Development;
            return true;
        }
        if (string.Equals(value, "Shipping", StringComparison.OrdinalIgnoreCase))
        {
            configuration = PackageConfiguration.Shipping;
            return true;
        }

        configuration = default;
        return false;
    }
}
