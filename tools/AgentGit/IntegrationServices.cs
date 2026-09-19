using System.ComponentModel;
using System.Diagnostics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace AgentGit;

internal interface IIntegrationLeaseVerifier
{
    Task VerifyAsync(
        string resource,
        string worktree,
        CancellationToken cancellation_token);
}

internal sealed class JobserverIntegrationLeaseVerifier : IIntegrationLeaseVerifier
{
    public async Task VerifyAsync(
        string resource,
        string worktree,
        CancellationToken cancellation_token)
    {
        var lease_id = Environment.GetEnvironmentVariable("NUKETHEBEES_JOBSERVER_LEASE");
        if (string.IsNullOrWhiteSpace(lease_id))
        {
            throw new RepositoryStateException(
                "Integration requires a jobserver lease; use the integrate-feature repository command.");
        }

        var local_app_data = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var jobserver = Path.Combine(local_app_data, "NukeTheBees", "jobserver", "bin", "jobserver.exe");
        if (!File.Exists(jobserver))
        {
            throw new RepositoryStateException($"The canonical jobserver client was not found at '{jobserver}'.");
        }

        var start_info = new ProcessStartInfo
        {
            FileName = jobserver,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        start_info.ArgumentList.Add("status");
        start_info.ArgumentList.Add("--json");
        using var process = new Process { StartInfo = start_info };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or Win32Exception)
        {
            throw new RepositoryStateException("Unable to query the active jobserver lease.", exception);
        }

        var output_task = process.StandardOutput.ReadToEndAsync(cancellation_token);
        var error_task = process.StandardError.ReadToEndAsync(cancellation_token);
        await process.WaitForExitAsync(cancellation_token);
        var output = await output_task;
        var error = await error_task;
        if (process.ExitCode != 0)
        {
            throw new RepositoryStateException(
                $"Unable to query the active jobserver lease: {error.Trim()}");
        }

        using var document = JsonDocument.Parse(output);
        var jobs = document.RootElement.GetProperty("jobs");
        var job = jobs.EnumerateArray().SingleOrDefault(candidate =>
            candidate.TryGetProperty("id", out var id) && id.GetString() == lease_id);
        if (job.ValueKind == JsonValueKind.Undefined)
        {
            throw new RepositoryStateException(
                $"Jobserver lease '{lease_id}' is not active; integration will not mutate Git.");
        }

        var valid_metadata = job.GetProperty("state").GetString() == "RUNNING" &&
            job.GetProperty("kind").GetString() == "integration" &&
            PathsEqual(job.GetProperty("worktree").GetString() ?? string.Empty, worktree);
        var valid_claim = job.GetProperty("claims").EnumerateArray().Any(claim =>
            claim.GetProperty("name").GetString() == resource &&
            claim.GetProperty("mode").GetString() == "exclusive");
        if (!valid_metadata || !valid_claim)
        {
            throw new RepositoryStateException(
                $"Jobserver lease '{lease_id}' is not the exclusive '{resource}' integration reservation " +
                $"for worktree '{worktree}'.");
        }
    }

    private static bool PathsEqual(string left, string right)
    {
        if (string.IsNullOrWhiteSpace(left) || string.IsNullOrWhiteSpace(right))
        {
            return false;
        }

        return string.Equals(
            Path.GetFullPath(left).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            Path.GetFullPath(right).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }
}

internal interface IIntegrationReviewer
{
    Task<bool> ConfirmAsync(
        string patch_fingerprint,
        bool maintainer_override,
        string? override_reason,
        CancellationToken cancellation_token);
}

internal sealed class ConsoleIntegrationReviewer(TextReader input, TextWriter output) : IIntegrationReviewer
{
    public async Task<bool> ConfirmAsync(
        string patch_fingerprint,
        bool maintainer_override,
        string? override_reason,
        CancellationToken cancellation_token)
    {
        output.WriteLine();
        var verb = maintainer_override ? "override" : "reviewed";
        if (maintainer_override)
        {
            output.WriteLine("MAINTAINER OVERRIDE REQUESTED");
            output.WriteLine($"Reason: {override_reason}");
            output.WriteLine("The listed review and validation gates will be skipped and recorded.");
        }
        else
        {
            output.WriteLine("Review the final effective feature diff above for lost or accidental changes.");
        }
        output.WriteLine($"Type '{verb} {patch_fingerprint}' within 15 minutes to continue.");
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellation_token);
        timeout.CancelAfter(TimeSpan.FromMinutes(15));
        try
        {
            var response = await input.ReadLineAsync(timeout.Token);
            return string.Equals(response, $"{verb} {patch_fingerprint}", StringComparison.Ordinal);
        }
        catch (OperationCanceledException) when (!cancellation_token.IsCancellationRequested)
        {
            return false;
        }
    }
}

internal interface IIntegrationValidator
{
    Task<IntegrationValidationResult> ValidateAsync(
        string worktree,
        string base_commit,
        PatchIdentity identity,
        IntegrationGatePlan plan,
        IntegrationStateStore state_store,
        TextWriter output,
        TextWriter error,
        CancellationToken cancellation_token);
}

internal sealed record IntegrationValidationResult(int ExitCode, string? FailedGate)
{
    public static IntegrationValidationResult Success { get; } = new(0, null);
}

internal interface IIntegrationCommandRunner
{
    Task<int> RunAsync(
        string executable,
        IReadOnlyList<string> arguments,
        string working_directory,
        TextWriter output,
        TextWriter error,
        CancellationToken cancellation_token);
}

internal sealed class IntegrationGateValidator : IIntegrationValidator
{
    private readonly IIntegrationCommandRunner command_runner;

    public IntegrationGateValidator()
        : this(new StreamingIntegrationCommandRunner())
    {
    }

    internal IntegrationGateValidator(IIntegrationCommandRunner command_runner)
    {
        this.command_runner = command_runner;
    }

    public async Task<IntegrationValidationResult> ValidateAsync(
        string worktree,
        string base_commit,
        PatchIdentity identity,
        IntegrationGatePlan plan,
        IntegrationStateStore state_store,
        TextWriter output,
        TextWriter error,
        CancellationToken cancellation_token)
    {
        foreach (var gate in plan.Gates)
        {
            var gate_name = IntegrationGatePlanner.Name(gate);
            var environment = EnvironmentIdentity(gate);
            var input = $"{gate_name}\n{base_commit}\n{identity.Tree}\n{environment}";
            var receipt_key = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(input)))
                .ToLowerInvariant();
            if (state_store.ReadValidation(receipt_key) is not null)
            {
                output.WriteLine($"Integration gate reused: {gate_name} ({receipt_key[..12]}).");
                continue;
            }

            output.WriteLine();
            output.WriteLine($"Integration gate: {gate_name}");
            output.WriteLine($"Reason: {plan.Reasons[gate]}");
            foreach (var command in Commands(gate, identity.ChangedPaths, worktree))
            {
                output.WriteLine($"Running: {command.Executable} {string.Join(' ', command.Arguments)}");
                var exit_code = await command_runner.RunAsync(
                    command.Executable,
                    command.Arguments,
                    worktree,
                    output,
                    error,
                    cancellation_token);
                if (exit_code != 0)
                {
                    error.WriteLine($"Integration blocked: {gate_name} failed with exit code {exit_code}.");
                    return new IntegrationValidationResult(exit_code, gate_name);
                }
            }

            state_store.WriteValidation(receipt_key, new ValidationReceipt(
                1,
                gate_name,
                receipt_key,
                identity.Fingerprint,
                base_commit,
                identity.Tree,
                environment,
                DateTimeOffset.UtcNow));
            output.WriteLine($"Integration gate passed: {gate_name}.");
        }

        return IntegrationValidationResult.Success;
    }

    private static IReadOnlyList<IntegrationCommand> Commands(
        IntegrationGate gate,
        IReadOnlyList<string> changed_paths,
        string worktree) => gate switch
        {
            IntegrationGate.AgentGitTests =>
                [new("dotnet", ["test", "tools/AgentGit.Tests/AgentGit.Tests.csproj", "--nologo"])],
            IntegrationGate.JobserverTests =>
            [
                new("cmake", ["--preset", "native"]),
            new("cmake", ["--build", "--preset", "native", "--target", "jobserver-tests"]),
            new("ctest", ["--test-dir", "out/build/native", "-L", "^jobserver$", "--output-on-failure"]),
        ],
            IntegrationGate.CSharpToolsTests => CSharpCommands(changed_paths, worktree),
            IntegrationGate.ToolTests =>
                [new("cmake", ["--workflow", "--preset", "tool-tests"])],
            IntegrationGate.PowerShellChecks =>
                [new("pwsh", ["-NoProfile", "-File", "PowerShell/TestDeveloperScripts.ps1"])],
            IntegrationGate.PythonChecks =>
            [
                new("pyright", ["Scripts", "cmake/presets"]),
            new("ruff", ["check", "Scripts", "cmake/presets"]),
        ],
            IntegrationGate.CMakeChecks =>
            [
                new("python", ["cmake/presets/generate.py", "--check"]),
            new("cmake", ["--preset", "native"]),
        ],
            IntegrationGate.NativeTests =>
                [new("cmake", ["--workflow", "--preset", "native-tests"])],
            IntegrationGate.CodegenTests =>
                [new("cmake", ["--workflow", "--preset", "codegen"])],
            IntegrationGate.UnrealTests =>
                [new("cmake", ["--workflow", "--preset", "debug-game-tests"])],
            IntegrationGate.DevelopmentBuild =>
                [new("cmake", ["--workflow", "--preset", "development"])],
            IntegrationGate.BenchmarkBuild =>
            [
                new("cmake", ["--preset", "benchmark"]),
            new("cmake", ["--build", "--preset", "benchmark", "--target", "benchmarks"]),
        ],
            _ => throw new ArgumentOutOfRangeException(nameof(gate), gate, "Unknown integration gate."),
        };

    private static IReadOnlyList<IntegrationCommand> CSharpCommands(
        IReadOnlyList<string> changed_paths,
        string worktree)
    {
        var projects = new SortedSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var raw_path in changed_paths)
        {
            var parts = raw_path.Replace('\\', '/').Split('/');
            if (parts.Length < 3 || !string.Equals(parts[0], "tools", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var component = parts[1].EndsWith(".Tests", StringComparison.OrdinalIgnoreCase)
                ? parts[1][..^".Tests".Length]
                : parts[1];
            if (string.Equals(component, "GitSupport", StringComparison.OrdinalIgnoreCase))
            {
                projects.Add("tools/GitTools.Tests/GitTools.Tests.csproj");
                continue;
            }

            var test_project = $"tools/{component}.Tests/{component}.Tests.csproj";
            if (File.Exists(Path.Combine(worktree, test_project.Replace('/', Path.DirectorySeparatorChar))))
            {
                projects.Add(test_project);
            }
        }

        if (projects.Count == 0)
        {
            projects.Add("tools/Tools.slnx");
        }
        return projects
            .Select(project => new IntegrationCommand("dotnet", ["test", project, "--nologo"]))
            .ToArray();
    }

    private static string EnvironmentIdentity(IntegrationGate gate)
    {
        var unreal = gate is IntegrationGate.UnrealTests or IntegrationGate.DevelopmentBuild
            ? Environment.GetEnvironmentVariable("UE_ROOT") ?? "UE_ROOT-unset"
            : "no-unreal";
        var agent_git = Assembly.GetExecutingAssembly().GetName().Version?.ToString() ?? "unknown";
        var dotnet_root = Environment.GetEnvironmentVariable("DOTNET_ROOT") ?? "DOTNET_ROOT-unset";
        var compiler = Environment.GetEnvironmentVariable("VCToolsInstallDir") ?? "VCToolsInstallDir-unset";
        return $"{Environment.OSVersion}|{Environment.Version}|{agent_git}|{dotnet_root}|{compiler}|{unreal}";
    }

    private sealed record IntegrationCommand(string Executable, IReadOnlyList<string> Arguments);
}

internal sealed class StreamingIntegrationCommandRunner : IIntegrationCommandRunner
{
    public async Task<int> RunAsync(
        string executable,
        IReadOnlyList<string> arguments,
        string working_directory,
        TextWriter output,
        TextWriter error,
        CancellationToken cancellation_token)
    {
        var start_info = new ProcessStartInfo
        {
            FileName = executable,
            WorkingDirectory = working_directory,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        foreach (var argument in arguments)
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = new Process { StartInfo = start_info };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or Win32Exception)
        {
            error.WriteLine($"Unable to start '{executable}': {exception.Message}");
            return ExitCodes.RepositoryFailure;
        }

        var output_task = CopyAsync(process.StandardOutput, output, cancellation_token);
        var error_task = CopyAsync(process.StandardError, error, cancellation_token);
        try
        {
            await process.WaitForExitAsync(cancellation_token);
            await Task.WhenAll(output_task, error_task);
        }
        catch (OperationCanceledException)
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
                await process.WaitForExitAsync(CancellationToken.None);
            }
            throw;
        }

        return process.ExitCode;
    }

    private static async Task CopyAsync(
        StreamReader reader,
        TextWriter writer,
        CancellationToken cancellation_token)
    {
        while (await reader.ReadLineAsync(cancellation_token) is { } line)
        {
            await writer.WriteLineAsync(line);
            await writer.FlushAsync(cancellation_token);
        }
    }
}
