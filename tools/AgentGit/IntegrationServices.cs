using System.ComponentModel;
using System.Diagnostics;
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
    Task<bool> ConfirmAsync(string rebased_tip, CancellationToken cancellation_token);
}

internal sealed class ConsoleIntegrationReviewer(TextReader input, TextWriter output) : IIntegrationReviewer
{
    public async Task<bool> ConfirmAsync(string rebased_tip, CancellationToken cancellation_token)
    {
        output.WriteLine();
        output.WriteLine("Review the rebased range-diff and feature diff above for lost or accidental changes.");
        output.WriteLine($"Type 'reviewed {rebased_tip}' within 15 minutes to continue.");
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellation_token);
        timeout.CancelAfter(TimeSpan.FromMinutes(15));
        try
        {
            var response = await input.ReadLineAsync(timeout.Token);
            return string.Equals(response, $"reviewed {rebased_tip}", StringComparison.Ordinal);
        }
        catch (OperationCanceledException) when (!cancellation_token.IsCancellationRequested)
        {
            return false;
        }
    }
}

internal interface IIntegrationValidator
{
    Task<int> ValidateAsync(
        string worktree,
        bool requires_benchmark_build,
        TextWriter output,
        TextWriter error,
        CancellationToken cancellation_token);
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

internal sealed class CMakeIntegrationValidator : IIntegrationValidator
{
    private readonly IIntegrationCommandRunner command_runner;

    public CMakeIntegrationValidator()
        : this(new StreamingIntegrationCommandRunner())
    {
    }

    internal CMakeIntegrationValidator(IIntegrationCommandRunner command_runner)
    {
        this.command_runner = command_runner;
    }

    public async Task<int> ValidateAsync(
        string worktree,
        bool requires_benchmark_build,
        TextWriter output,
        TextWriter error,
        CancellationToken cancellation_token)
    {
        var commands = new List<IReadOnlyList<string>>
        {
            new[] { "--workflow", "--preset", "debug-game-tests" },
        };
        if (requires_benchmark_build)
        {
            commands.Add(new[] { "--preset", "benchmark" });
            commands.Add(new[] { "--build", "--preset", "benchmark", "--target", "benchmarks" });
        }
        commands.Add(new[] { "--workflow", "--preset", "development" });

        foreach (var arguments in commands)
        {
            output.WriteLine();
            output.WriteLine($"Integration gate: cmake {string.Join(' ', arguments)}");
            var exit_code = await command_runner.RunAsync(
                "cmake",
                arguments,
                worktree,
                output,
                error,
                cancellation_token);
            if (exit_code != 0)
            {
                return exit_code;
            }
        }

        return 0;
    }
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
