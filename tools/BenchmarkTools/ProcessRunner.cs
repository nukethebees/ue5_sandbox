using System.ComponentModel;
using System.Diagnostics;

namespace BenchmarkTools;

internal sealed record ProcessRequest(
    string FileName,
    IReadOnlyList<string> Arguments,
    string WorkingDirectory,
    IReadOnlyDictionary<string, string?>? EnvironmentVariables = null);

internal sealed record ProcessResult(int ExitCode);

internal interface IProcessRunner
{
    Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token);
}

internal sealed class ProcessLaunchException(string message, Exception inner_exception) : Exception(message, inner_exception);

internal sealed class ProcessRunner : IProcessRunner
{
    public async Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
    {
        ArgumentNullException.ThrowIfNull(request);

        var start_info = new ProcessStartInfo
        {
            FileName = request.FileName,
            WorkingDirectory = request.WorkingDirectory,
            UseShellExecute = false,
        };
        foreach (var argument in request.Arguments)
        {
            start_info.ArgumentList.Add(argument);
        }
        if (request.EnvironmentVariables is not null)
        {
            foreach (var (name, value) in request.EnvironmentVariables)
            {
                start_info.Environment[name] = value;
            }
        }

        using var process = new Process { StartInfo = start_info };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or Win32Exception)
        {
            throw new ProcessLaunchException($"Unable to start '{request.FileName}'.", exception);
        }

        try
        {
            await process.WaitForExitAsync(cancellation_token);
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

        return new ProcessResult(process.ExitCode);
    }
}
