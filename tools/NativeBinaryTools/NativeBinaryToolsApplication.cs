namespace NativeBinaryTools;

internal sealed class NativeBinaryToolsApplication(
    MimallocSymbolTool mimallocSymbolTool,
    TextWriter standardError)
{
    public async Task<int> RunAsync(IReadOnlyList<string> arguments, CancellationToken cancellationToken = default)
    {
        if (!TryParse(arguments, out var request, out var usageError))
        {
            standardError.WriteLine(usageError);
            standardError.WriteLine("Usage: NativeBinaryTools mimalloc-symbols <generate|verify> --nm <path> --readobj <path> [--output <path>] <object>...");
            return 2;
        }

        try
        {
            if (request!.Command == MimallocSymbolCommand.Generate)
            {
                await mimallocSymbolTool.GenerateAsync(request, cancellationToken);
            }
            else
            {
                await mimallocSymbolTool.VerifyAsync(request, cancellationToken);
            }

            return 0;
        }
        catch (Exception exception) when (exception is MimallocSymbolToolException or IOException or UnauthorizedAccessException)
        {
            standardError.WriteLine($"NativeBinaryTools: {exception.Message}");
            return 1;
        }
    }

    internal static bool TryParse(
        IReadOnlyList<string> arguments,
        out MimallocSymbolRequest? request,
        out string usageError)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        request = null;
        usageError = "Invalid arguments.";
        if (arguments.Count < 2 || !string.Equals(arguments[0], "mimalloc-symbols", StringComparison.Ordinal))
        {
            return false;
        }

        var command = arguments[1] switch
        {
            "generate" => MimallocSymbolCommand.Generate,
            "verify" => MimallocSymbolCommand.Verify,
            _ => (MimallocSymbolCommand?)null,
        };
        if (command is null)
        {
            usageError = "Expected mimalloc-symbols command 'generate' or 'verify'.";
            return false;
        }

        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        var objects = new List<string>();
        for (var index = 2; index < arguments.Count; ++index)
        {
            var argument = arguments[index];
            if (argument is "--nm" or "--readobj" or "--output")
            {
                if (index + 1 >= arguments.Count || string.IsNullOrWhiteSpace(arguments[index + 1]) || !values.TryAdd(argument, arguments[++index]))
                {
                    usageError = $"Missing or duplicate value for '{argument}'.";
                    return false;
                }

                continue;
            }

            if (argument.StartsWith("-", StringComparison.Ordinal) || string.IsNullOrWhiteSpace(argument))
            {
                usageError = $"Unknown argument '{argument}'.";
                return false;
            }

            objects.Add(argument);
        }

        if (!values.TryGetValue("--nm", out var nmPath) || !values.TryGetValue("--readobj", out var readObjPath) || objects.Count == 0)
        {
            usageError = "Both --nm and --readobj, plus at least one object path, are required.";
            return false;
        }

        var outputPath = values.GetValueOrDefault("--output");
        if (command == MimallocSymbolCommand.Generate && string.IsNullOrWhiteSpace(outputPath))
        {
            usageError = "--output is required for generate.";
            return false;
        }

        if (command == MimallocSymbolCommand.Verify && outputPath is not null)
        {
            usageError = "--output is only valid for generate.";
            return false;
        }

        request = new MimallocSymbolRequest(command.Value, nmPath, readObjPath, outputPath, objects);
        return true;
    }
}
