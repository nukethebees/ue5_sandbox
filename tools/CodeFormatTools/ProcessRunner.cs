using System.ComponentModel;
using System.Diagnostics;

namespace CodeFormatTools;

internal sealed record ProcessRequest(
    string FileName,
    IReadOnlyList<string> Arguments,
    string WorkingDirectory,
    TimeSpan Timeout);

internal sealed record ProcessResult(int ExitCode, byte[] StandardOutput, string StandardError);

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

        using var process = new Process { StartInfo = start_info };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or Win32Exception)
        {
            throw new ProcessLaunchException($"Unable to start '{request.FileName}'.", exception);
        }

        using var timeout_source = CancellationTokenSource.CreateLinkedTokenSource(cancellation_token);
        timeout_source.CancelAfter(request.Timeout);
        var process_token = timeout_source.Token;
        var output_task = ReadAllBytesAsync(process.StandardOutput.BaseStream, process_token);
        var error_task = process.StandardError.ReadToEndAsync(process_token);

        try
        {
            await process.WaitForExitAsync(process_token);
            await Task.WhenAll(output_task, error_task);
        }
        catch (OperationCanceledException) when (!cancellation_token.IsCancellationRequested)
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
                await process.WaitForExitAsync(CancellationToken.None);
            }

            throw new ProcessTimeoutException($"'{request.FileName}' timed out after {request.Timeout.TotalSeconds:0} seconds.");
        }

        return new ProcessResult(process.ExitCode, await output_task, await error_task);
    }

    private static async Task<byte[]> ReadAllBytesAsync(Stream stream, CancellationToken cancellation_token)
    {
        await using var output = new MemoryStream();
        await stream.CopyToAsync(output, cancellation_token);
        return output.ToArray();
    }
}
