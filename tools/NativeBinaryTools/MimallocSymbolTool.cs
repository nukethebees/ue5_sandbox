using System.Text;

namespace NativeBinaryTools;

internal sealed class MimallocSymbolTool(IProcessRunner processRunner, string workingDirectory)
{
    private static readonly TimeSpan toolTimeout = TimeSpan.FromSeconds(30);
    private static readonly IReadOnlySet<string> forbiddenOverrideSymbols = new HashSet<string>(StringComparer.Ordinal)
    {
        "_aligned_free",
        "_aligned_malloc",
        "_aligned_realloc",
        "calloc",
        "free",
        "malloc",
        "realloc",
    };

    public async Task GenerateAsync(MimallocSymbolRequest request, CancellationToken cancellationToken)
    {
        var symbols = await NmOutputAsync(request, ["--defined-only", "--extern-only", "--demangle"], cancellationToken);
        var identifiers = MimallocSymbolParser.AllocatorIdentifiers(symbols);
        if (identifiers.Count == 0)
        {
            throw new MimallocSymbolToolException("The mimalloc probe object exposed no allocator identifiers.");
        }

        var directives = await ReadObjDirectivesAsync(request, cancellationToken);
        var header = MimallocSymbolHeaderRenderer.Render(identifiers, MimallocSymbolParser.LinkerIncludes(directives));
        WriteHeader(request.OutputPath!, header);
    }

    public async Task VerifyAsync(MimallocSymbolRequest request, CancellationToken cancellationToken)
    {
        var externalSymbols = MimallocSymbolParser.WithoutMsvcStringLiterals(
            await NmOutputAsync(request, ["--defined-only", "--extern-only"], cancellationToken));
        ThrowIfUnprefixed(
            MimallocSymbolParser.AllocatorIdentifiers(externalSymbols),
            "Unprefixed externally visible mimalloc identifiers remain");

        var undefinedSymbols = MimallocSymbolParser.WithoutMsvcStringLiterals(
            await NmOutputAsync(request, ["--undefined-only", "--extern-only"], cancellationToken));
        ThrowIfUnprefixed(
            MimallocSymbolParser.AllocatorIdentifiers(undefinedSymbols),
            "Unprefixed mimalloc references remain");

        var rawDefinitions = SplitLines(
            await NmOutputAsync(request, ["-j", "--defined-only", "--extern-only"], cancellationToken)).ToHashSet(StringComparer.Ordinal);
        var forbidden = rawDefinitions
            .Where(symbol => forbiddenOverrideSymbols.Contains(symbol) || MimallocSymbolParser.IsMsvcOperatorNewDelete(symbol))
            .Order(StringComparer.Ordinal)
            .ToArray();
        if (forbidden.Length > 0)
        {
            throw new MimallocSymbolToolException($"Allocator override symbols are defined: {string.Join(", ", forbidden)}");
        }

        var includes = MimallocSymbolParser.LinkerIncludes(await ReadObjDirectivesAsync(request, cancellationToken));
        ThrowIfUnprefixed(
            includes.Where(MimallocSymbolParser.IsMimallocLinkerInclude),
            "Unprefixed mimalloc linker directives remain");
    }

    private async Task<string> NmOutputAsync(
        MimallocSymbolRequest request,
        IReadOnlyList<string> arguments,
        CancellationToken cancellationToken)
    {
        return await RunForObjectsAsync(request.NmPath, arguments, request.ObjectPaths, cancellationToken);
    }

    private async Task<string> ReadObjDirectivesAsync(MimallocSymbolRequest request, CancellationToken cancellationToken)
    {
        return await RunForObjectsAsync(request.ReadObjPath, ["--coff-directives"], request.ObjectPaths, cancellationToken);
    }

    private async Task<string> RunForObjectsAsync(
        string executable,
        IReadOnlyList<string> arguments,
        IReadOnlyList<string> objectPaths,
        CancellationToken cancellationToken)
    {
        var output = new List<string>();
        foreach (var objectPath in objectPaths)
        {
            var toolArguments = arguments.Append(objectPath).ToArray();
            ProcessResult result;
            try
            {
                result = await processRunner.RunAsync(
                    new ProcessRequest(executable, toolArguments, workingDirectory, toolTimeout),
                    cancellationToken);
            }
            catch (ProcessLaunchException exception)
            {
                throw new MimallocSymbolToolException($"Unable to run '{executable}' for '{objectPath}': {exception.Message}", exception);
            }
            catch (ProcessTimeoutException exception)
            {
                throw new MimallocSymbolToolException($"'{executable}' timed out while inspecting '{objectPath}': {exception.Message}", exception);
            }

            if (result.ExitCode != 0)
            {
                var error = string.IsNullOrWhiteSpace(result.StandardError)
                    ? string.Empty
                    : $"\n{result.StandardError}";
                throw new MimallocSymbolToolException(
                    $"'{executable}' failed for '{objectPath}' with exit code {result.ExitCode}.{error}");
            }

            output.Add(result.StandardOutput);
        }

        return string.Join("\n", output);
    }

    private static void ThrowIfUnprefixed(IEnumerable<string> symbols, string description)
    {
        var remaining = symbols.Order(StringComparer.Ordinal).ToArray();
        if (remaining.Length > 0)
        {
            throw new MimallocSymbolToolException($"{description}: {string.Join(", ", remaining)}");
        }
    }

    private static IEnumerable<string> SplitLines(string output)
    {
        return output.Split(["\r\n", "\n", "\r"], StringSplitOptions.RemoveEmptyEntries);
    }

    private static void WriteHeader(string outputPath, string header)
    {
        var fullPath = Path.GetFullPath(outputPath);
        Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
        File.WriteAllText(fullPath, header, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
    }
}

internal sealed class MimallocSymbolToolException(string message, Exception? innerException = null) : Exception(message, innerException);
