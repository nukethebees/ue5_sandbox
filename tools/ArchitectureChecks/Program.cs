namespace ArchitectureChecks;

public static class Program
{
    public static int Main(string[] arguments)
    {
        if (!TryParse(arguments, out var request))
        {
            Console.Error.WriteLine("Usage: ArchitectureChecks --root <path>");
            return 2;
        }

        try
        {
            var result = new SpaceGameLayerChecker().Check(request!);
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
}
