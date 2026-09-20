using System.Globalization;
using System.Text.Json;

namespace BenchmarkTools;

internal static class NativeSoaReserveMatrixBenchmarkCommand
{
    private static readonly int[] allowed_rows = [4096, 65536, 1048576];
    private static readonly int[] allowed_owners = [1, 2, 4, 8, 16, 32, 64, 128, 200, 256, 512];
    private static readonly string[] implementations = ["Vector", "Single", "RawMalloc", "RawRealloc"];
    private static readonly HashSet<string> value_arguments = ["--standard", "--mimalloc", "--output-dir", "--rows", "--owners", "--repetitions"];
    private static readonly HashSet<string> flag_arguments = ["--dry-run"];

    public static async Task<int> RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository_paths, IReadOnlyList<string> arguments, CancellationToken cancellation_token)
    {
        var parsed = CommandArguments.Parse(arguments, value_arguments, flag_arguments);
        var standard = Path.GetFullPath(parsed.Required("--standard"));
        var mimalloc = Path.GetFullPath(parsed.Required("--mimalloc"));
        var output = BenchmarkCommandSupport.ResolveOutputDirectory(repository_paths, parsed.Required("--output-dir"));
        var rows = parsed.PositiveInt32("--rows", 65536);
        if (!allowed_rows.Contains(rows))
        {
            throw new BenchmarkToolException("'--rows' must be 4096, 65536, or 1048576.");
        }
        var owners = ParseOwners(parsed.Value("--owners", string.Join(',', allowed_owners)));
        var repetitions = parsed.PositiveInt32("--repetitions", 10);
        var dry_run = parsed.HasFlag("--dry-run");
        if (!File.Exists(standard) || !File.Exists(mimalloc))
        {
            throw new BenchmarkToolException("Both native reserve-matrix executables must exist.");
        }

        var command = new List<string>
        {
            "native-soa-reserve-matrix", "--standard", standard, "--mimalloc", mimalloc, "--output-dir", output,
            "--rows", rows.ToString(CultureInfo.InvariantCulture), "--owners", string.Join(',', owners),
            "--repetitions", repetitions.ToString(CultureInfo.InvariantCulture),
        };
        if (dry_run)
        {
            command.Add("--dry-run");
        }
        return await BenchmarkCommandSupport.RunWithBenchmarkLeaseAsync(
            application, repository_paths, "native SoA reserve matrix", command,
            async () => await RunInsideLeaseAsync(application, repository_paths, standard, mimalloc, output, rows, owners, repetitions, dry_run, cancellation_token),
            skip_lease: dry_run,
            cancellation_token: cancellation_token);
    }

    private static async Task<int> RunInsideLeaseAsync(BenchmarkToolsApplication application, RepositoryPaths repository_paths, string standard, string mimalloc, string output, int rows, IReadOnlyList<int> owners, int repetitions, bool dry_run, CancellationToken cancellation_token)
    {
        Directory.CreateDirectory(output);
        var runs = new List<ReserveRun>();
        foreach (var owner_count in owners)
        {
            foreach (var (backend, executable) in new[] { ("Standard", standard), ("Mimalloc", mimalloc) })
            {
                foreach (var implementation in implementations)
                {
                    var name = $"{implementation}/reserve_batch/{rows}/{owner_count}/real_time";
                    var stem = $"{backend}-{implementation}-{rows}-{owner_count}";
                    var destination = Path.Combine(output, stem + ".json");
                    var command = new List<string>
                    {
                        $"--benchmark_filter=^{System.Text.RegularExpressions.Regex.Escape(name)}$",
                        $"--benchmark_repetitions={repetitions}", "--benchmark_min_time=0.1s",
                        $"--benchmark_out={destination}", "--benchmark_out_format=json",
                    };
                    if (dry_run)
                    {
                        command.Add("--benchmark_dry_run");
                    }
                    application.StandardOutput.WriteLine($"Running {backend} {name}");
                    var result = await application.ProcessRunner.RunAsync(new ProcessRequest(executable, command, repository_paths.Root), cancellation_token);
                    File.WriteAllText(Path.Combine(output, stem + ".log"), result.StandardOutput + result.StandardError);
                    if (result.ExitCode != 0)
                    {
                        application.WriteProcessOutput(result);
                        return result.ExitCode;
                    }
                    if (!File.Exists(destination))
                    {
                        throw new BenchmarkToolException($"Native reserve benchmark did not produce '{destination}'.");
                    }
                    using var document = JsonDocument.Parse(File.ReadAllText(destination));
                    ValidateResult(document.RootElement, name);
                    runs.Add(new ReserveRun(backend, implementation, owner_count, document.RootElement.Clone()));
                }
            }
        }

        var aggregate = new ReserveMatrixDocument(1, rows, owners, dry_run, runs);
        BenchmarkCommandSupport.WriteJson(Path.Combine(output, "native-reserve-matrix.json"), aggregate);
        BenchmarkCommandSupport.WriteCsv(Path.Combine(output, "native-reserve-matrix.csv"), CsvRows(runs, owners));
        return 0;
    }

    private static int[] ParseOwners(string value)
    {
        var owners = new List<int>();
        var seen = new HashSet<int>();
        foreach (var item in value.Split(',', StringSplitOptions.None))
        {
            if (!int.TryParse(item.Trim(), NumberStyles.None, CultureInfo.InvariantCulture, out var owner) || !allowed_owners.Contains(owner) || !seen.Add(owner))
            {
                throw new BenchmarkToolException($"'--owners' must contain unique values from {string.Join(", ", allowed_owners)}.");
            }
            owners.Add(owner);
        }
        if (owners.Count == 0)
        {
            throw new BenchmarkToolException("'--owners' must contain one or more values.");
        }
        return [.. owners];
    }

    private static void ValidateResult(JsonElement document, string name)
    {
        if (!document.TryGetProperty("benchmarks", out var benchmarks) || benchmarks.ValueKind != JsonValueKind.Array)
        {
            throw new BenchmarkToolException("Google Benchmark output did not contain a benchmarks array.");
        }
        var names = benchmarks.EnumerateArray().Select(record => record.TryGetProperty("run_name", out var run_name) ? run_name.GetString() : null).ToHashSet(StringComparer.Ordinal);
        if (names.Count != 1 || !names.Contains(name))
        {
            throw new BenchmarkToolException($"Expected exactly one benchmark: {name}.");
        }
        foreach (var record in benchmarks.EnumerateArray())
        {
            if (record.TryGetProperty("error_occurred", out var error) && error.GetBoolean())
            {
                throw new BenchmarkToolException(record.TryGetProperty("error_message", out var message) ? message.GetString() ?? "Google Benchmark failed." : "Google Benchmark failed.");
            }
        }
    }

    private static IEnumerable<string[]> CsvRows(IReadOnlyList<ReserveRun> runs, IReadOnlyList<int> owners)
    {
        var labels = runs.Select(run => $"{run.Backend} {run.Implementation}").Distinct(StringComparer.Ordinal).ToArray();
        yield return ["owners", .. labels];
        foreach (var owner_count in owners)
        {
            var row = new List<string> { owner_count.ToString(CultureInfo.InvariantCulture) };
            foreach (var label in labels)
            {
                var values = runs.Where(run => run.Owners == owner_count && $"{run.Backend} {run.Implementation}" == label)
                    .SelectMany(run => TimingValues(run.Result)).ToArray();
                row.Add(values.Average().ToString("F6", CultureInfo.InvariantCulture));
            }
            yield return [.. row];
        }
    }

    private static IEnumerable<double> TimingValues(JsonElement result)
    {
        foreach (var record in result.GetProperty("benchmarks").EnumerateArray())
        {
            if (record.TryGetProperty("run_type", out var type) && type.GetString() == "aggregate")
            {
                continue;
            }
            var multiplier = record.GetProperty("time_unit").GetString() switch { "ns" => 1e-6, "us" => 1e-3, "ms" => 1.0, "s" => 1e3, _ => throw new BenchmarkToolException("Invalid Google Benchmark time unit.") };
            var value = record.GetProperty("real_time").GetDouble() * multiplier;
            if (!double.IsFinite(value) || value <= 0)
            {
                throw new BenchmarkToolException("Invalid Google Benchmark timing.");
            }
            yield return value;
        }
    }

    private sealed record ReserveRun(string Backend, string Implementation, int Owners, JsonElement Result);
    private sealed record ReserveMatrixDocument(int SchemaVersion, int Rows, IReadOnlyList<int> Owners, bool DryRun, IReadOnlyList<ReserveRun> Runs);
}
