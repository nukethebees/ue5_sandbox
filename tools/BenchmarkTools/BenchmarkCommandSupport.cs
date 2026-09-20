using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace BenchmarkTools;

internal sealed class CommandArguments
{
    private readonly Dictionary<string, string> values_ = new(StringComparer.Ordinal);
    private readonly HashSet<string> flags_ = new(StringComparer.Ordinal);

    public static CommandArguments Parse(IReadOnlyList<string> arguments, IReadOnlySet<string> values, IReadOnlySet<string> flags)
    {
        var result = new CommandArguments();
        for (var index = 0; index < arguments.Count; ++index)
        {
            var argument = arguments[index];
            if (flags.Contains(argument))
            {
                if (!result.flags_.Add(argument))
                {
                    throw new BenchmarkToolException($"Duplicate argument '{argument}'.");
                }
                continue;
            }
            var equals_index = argument.IndexOf('=');
            var name = equals_index < 0 ? argument : argument[..equals_index];
            var value = equals_index < 0
                ? index + 1 < arguments.Count ? arguments[++index] : null
                : argument[(equals_index + 1)..];
            if (!values.Contains(name) || string.IsNullOrWhiteSpace(value) || !result.values_.TryAdd(name, value))
            {
                throw new BenchmarkToolException($"Unknown, missing, or duplicate argument '{argument}'.");
            }
        }
        return result;
    }

    public bool HasFlag(string name) => flags_.Contains(name);

    public string Required(string name)
    {
        return values_.GetValueOrDefault(name) ?? throw new BenchmarkToolException($"'{name}' is required.");
    }

    public string Value(string name, string default_value) => values_.GetValueOrDefault(name, default_value);

    public int PositiveInt32(string name, int default_value, int maximum = int.MaxValue)
    {
        var value = Value(name, default_value.ToString(CultureInfo.InvariantCulture));
        if (!int.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out var number) || number <= 0 || number > maximum)
        {
            throw new BenchmarkToolException($"'{name}' must be a positive integer no greater than {maximum}.");
        }
        return number;
    }

    public double FiniteDouble(string name, double default_value, double minimum, double maximum)
    {
        var value = Value(name, default_value.ToString("R", CultureInfo.InvariantCulture));
        if (!double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out var number) || !double.IsFinite(number) || number < minimum || number > maximum)
        {
            throw new BenchmarkToolException($"'{name}' must be finite and in the range {minimum.ToString(CultureInfo.InvariantCulture)} to {maximum.ToString(CultureInfo.InvariantCulture)}.");
        }
        return number;
    }
}

internal static class BenchmarkCommandSupport
{
    public static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    };

    public static async Task<ProcessResult> RunSelfAsync(
        BenchmarkToolsApplication application,
        RepositoryPaths repository_paths,
        IReadOnlyList<string> arguments,
        CancellationToken cancellation_token)
    {
        return await application.ProcessRunner.RunAsync(
            new ProcessRequest(application.ExecutablePath, arguments, repository_paths.Root), cancellation_token);
    }

    public static async Task<int> RunWithBenchmarkLeaseAsync(
        BenchmarkToolsApplication application,
        RepositoryPaths repository_paths,
        string name,
        IReadOnlyList<string> command_arguments,
        Func<Task<int>> action,
        IReadOnlyList<string>? shared_resources = null,
        bool skip_lease = false,
        CancellationToken cancellation_token = default)
    {
        if (skip_lease || !string.IsNullOrWhiteSpace(application.Environment.GetEnvironmentVariable("NUKETHEBEES_JOBSERVER_JOB")))
        {
            return await action();
        }

        var request = JobserverExecution.CreateRequest(
            application.JobserverLocator.Locate(),
            application.ExecutablePath,
            repository_paths,
            name,
            command_arguments,
            shared_resources);
        var result = await application.ProcessRunner.RunAsync(request, cancellation_token);
        application.WriteProcessOutput(result);
        return result.ExitCode;
    }

    public static string ResolveOutputDirectory(RepositoryPaths repository_paths, string output_directory)
    {
        var path = Path.IsPathFullyQualified(output_directory)
            ? output_directory
            : Path.Combine(repository_paths.Root, output_directory);
        return Path.GetFullPath(path);
    }

    public static void WriteJson(string path, object value)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var temporary_path = path + ".tmp";
        File.WriteAllText(temporary_path, JsonSerializer.Serialize(value, JsonOptions) + Environment.NewLine, new UTF8Encoding(false));
        File.Move(temporary_path, path, overwrite: true);
    }

    public static IEnumerable<JsonElement> JsonLines(string output)
    {
        var results = new List<JsonElement>();
        foreach (var line in output.Split(["\r\n", "\n", "\r"], StringSplitOptions.RemoveEmptyEntries))
        {
            var trimmed = line.TrimStart();
            if (!trimmed.StartsWith('{'))
            {
                continue;
            }
            try
            {
                using var document = JsonDocument.Parse(trimmed);
                results.Add(document.RootElement.Clone());
            }
            catch (JsonException exception)
            {
                throw new BenchmarkToolException($"Benchmark output contained malformed JSON: {exception.Message}");
            }
        }
        return results;
    }

    public static string EngineResource(string editor_path)
    {
        var editor = new FileInfo(Path.GetFullPath(editor_path));
        var binaries = editor.Directory?.Parent;
        var engine = binaries?.Parent;
        var root = engine?.Parent;
        if (root is null || !string.Equals(engine?.Name, "Engine", StringComparison.OrdinalIgnoreCase))
        {
            throw new BenchmarkToolException($"Could not derive an Unreal Engine root from '{editor_path}'.");
        }
        var resolved_root = root.Exists ? root.ResolveLinkTarget(returnFinalTarget: true)?.FullName ?? root.FullName : root.FullName;
        var canonical = resolved_root.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar).ToLowerInvariant();
        var hash = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(canonical))).ToLowerInvariant();
        return $"unreal-build/{hash}";
    }

    public static void WriteCsv(string path, IEnumerable<string[]> rows)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using var writer = new StreamWriter(path, false, new UTF8Encoding(false));
        foreach (var row in rows)
        {
            writer.WriteLine(string.Join(',', row.Select(EscapeCsv)));
        }
    }

    private static string EscapeCsv(string value)
    {
        return value.IndexOfAny([',', '"', '\r', '\n']) < 0 ? value : $"\"{value.Replace("\"", "\"\"")}\"";
    }
}
