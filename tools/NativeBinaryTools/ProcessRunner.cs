using System.ComponentModel;
using System.Diagnostics;

namespace NativeBinaryTools;

internal sealed record ProcessRequest(
    string FileName,
    IReadOnlyList<string> Arguments,
    string WorkingDirectory,
    TimeSpan Timeout);

internal sealed record ProcessResult(int ExitCode, string StandardOutput, string StandardError);

internal interface IProcessRunner
{
    Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellationToken);
}

internal sealed class ProcessLaunchException(string message, Exception innerException) : Exception(message, innerException);

internal sealed class ProcessTimeoutException(string message) : Exception(message);

internal sealed class ProcessRunner : IProcessRunner
{
    public async Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(request);

        var startInfo = new ProcessStartInfo
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
            startInfo.ArgumentList.Add(argument);
        }

        using var process = new Process { StartInfo = startInfo };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or Win32Exception)
        {
            throw new ProcessLaunchException($"Unable to start '{request.FileName}'.", exception);
        }

        using var timeoutSource = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeoutSource.CancelAfter(request.Timeout);
        var processToken = timeoutSource.Token;
        var standardOutputTask = process.StandardOutput.ReadToEndAsync(processToken);
        var standardErrorTask = process.StandardError.ReadToEndAsync(processToken);

        try
        {
            await process.WaitForExitAsync(processToken);
            await Task.WhenAll(standardOutputTask, standardErrorTask);
        }
        catch (OperationCanceledException)
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
                await process.WaitForExitAsync(CancellationToken.None);
            }

            if (!cancellationToken.IsCancellationRequested)
            {
                throw new ProcessTimeoutException($"'{request.FileName}' timed out after {request.Timeout.TotalSeconds:0} seconds.");
            }

            throw;
        }

        return new ProcessResult(process.ExitCode, await standardOutputTask, await standardErrorTask);
    }
}
