using System.ComponentModel;
using System.Diagnostics;
using System.Text;

namespace BenchmarkTools;

internal sealed record ProcessRequest(
    string FileName,
    IReadOnlyList<string> Arguments,
    string WorkingDirectory,
    IReadOnlyDictionary<string, string?>? EnvironmentVariables = null,
    TimeSpan? Timeout = null,
    string? OutputLogPath = null);

internal sealed record ProcessResult(int ExitCode, string StandardOutput = "", string StandardError = "");

internal interface IProcessRunner
{
    Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token);
}

internal sealed class ProcessLaunchException(string message, Exception inner_exception) : Exception(message, inner_exception);

internal sealed class ProcessTimeoutException(string message) : Exception(message);

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
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
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

        using var output_log = request.OutputLogPath is null ? null : new StreamWriter(request.OutputLogPath, false, new UTF8Encoding(false)) { AutoFlush = true };
        var output_lock = new object();
        using var process = new Process { StartInfo = start_info };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or Win32Exception)
        {
            throw new ProcessLaunchException($"Unable to start '{request.FileName}'.", exception);
        }

        var timeout = request.Timeout;
        using var timeout_source = timeout is null
            ? null
            : CancellationTokenSource.CreateLinkedTokenSource(cancellation_token);
        if (timeout is not null)
        {
            timeout_source!.CancelAfter(timeout.Value);
        }
        var process_token = timeout_source?.Token ?? cancellation_token;
        var standard_output_task = CaptureAsync(process.StandardOutput, output_log, output_lock);
        var standard_error_task = CaptureAsync(process.StandardError, output_log, output_lock);

        try
        {
            await process.WaitForExitAsync(process_token);
            await Task.WhenAll(standard_output_task, standard_error_task);
        }
        catch (OperationCanceledException)
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
                await process.WaitForExitAsync(CancellationToken.None);
            }
            await Task.WhenAll(standard_output_task, standard_error_task);

            if (!cancellation_token.IsCancellationRequested && timeout is not null)
            {
                throw new ProcessTimeoutException(
                    $"'{request.FileName}' timed out after {timeout.Value.TotalSeconds:0} seconds.");
            }

            throw;
        }

        return new ProcessResult(process.ExitCode, await standard_output_task, await standard_error_task);
    }

    private static async Task<string> CaptureAsync(StreamReader reader, StreamWriter? log, object output_lock)
    {
        if (log is null) return await reader.ReadToEndAsync();
        var output = new StringBuilder();
        var buffer = new char[4096];
        int count;
        while ((count = await reader.ReadAsync(buffer)) != 0)
        {
            output.Append(buffer, 0, count);
            lock (output_lock)
            {
                log.Write(buffer, 0, count);
            }
        }
        return output.ToString();
    }
}
