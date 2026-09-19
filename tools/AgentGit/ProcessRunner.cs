using System.ComponentModel;
using System.Diagnostics;
using System.Text;

namespace AgentGit;

internal sealed record ProcessRequest(
    string FileName,
    IReadOnlyList<string> Arguments,
    string WorkingDirectory,
    IReadOnlyDictionary<string, string> Environment,
    TimeSpan Timeout,
    int MaximumCapturedStreamBytes = 64 * 1024 * 1024);

internal sealed record ProcessResult(int ExitCode, byte[] StandardOutput, string StandardError);

internal interface IProcessRunner
{
    Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token);
}

internal sealed class ProcessRunner : IProcessRunner
{
    public async Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
    {
        ArgumentNullException.ThrowIfNull(request);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(request.MaximumCapturedStreamBytes);

        var start_info = CreateStartInfo(request);

        using var process = new Process { StartInfo = start_info };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or Win32Exception)
        {
            throw new RepositoryException($"Unable to start trusted executable '{request.FileName}'.", exception);
        }

        using var timeout_source = CancellationTokenSource.CreateLinkedTokenSource(cancellation_token);
        timeout_source.CancelAfter(request.Timeout);
        var token = timeout_source.Token;
        var output_task = CaptureStreamAsync(
            process.StandardOutput.BaseStream,
            request.MaximumCapturedStreamBytes,
            token);
        var error_task = CaptureStreamAsync(
            process.StandardError.BaseStream,
            request.MaximumCapturedStreamBytes,
            token);
        try
        {
            await process.WaitForExitAsync(token);
            await Task.WhenAll(output_task, error_task);
        }
        catch (OperationCanceledException)
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
                await process.WaitForExitAsync(CancellationToken.None);
            }

            if (cancellation_token.IsCancellationRequested)
            {
                throw;
            }

            throw new RepositoryStateException(
                $"Trusted process '{request.FileName}' timed out after {request.Timeout.TotalSeconds:0} seconds.");
        }

        var output = await output_task;
        var error = await error_task;
        if (output.ExceededLimit || error.ExceededLimit)
        {
            throw new RepositoryStateException(
                $"Trusted process '{request.FileName}' exceeded its " +
                $"{request.MaximumCapturedStreamBytes} byte diagnostic output limit.");
        }

        return new ProcessResult(process.ExitCode, output.Bytes, Encoding.UTF8.GetString(error.Bytes));
    }

    internal static ProcessStartInfo CreateStartInfo(ProcessRequest request)
    {
        var start_info = new ProcessStartInfo
        {
            FileName = request.FileName,
            WorkingDirectory = request.WorkingDirectory,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        start_info.Environment.Clear();
        foreach (var pair in request.Environment)
        {
            start_info.Environment.Add(pair.Key, pair.Value);
        }

        foreach (var argument in request.Arguments)
        {
            start_info.ArgumentList.Add(argument);
        }

        return start_info;
    }

    private static async Task<CapturedStream> CaptureStreamAsync(
        Stream stream,
        int maximum_captured_stream_bytes,
        CancellationToken cancellation_token)
    {
        await using var output = new MemoryStream();
        var buffer = new byte[81920];
        var exceeded_limit = false;
        while (true)
        {
            var count = await stream.ReadAsync(buffer, cancellation_token);
            if (count == 0)
            {
                break;
            }

            if (output.Length + count <= maximum_captured_stream_bytes)
            {
                await output.WriteAsync(buffer.AsMemory(0, count), cancellation_token);
            }
            else
            {
                exceeded_limit = true;
            }
        }

        return new CapturedStream(output.ToArray(), exceeded_limit);
    }

    private sealed record CapturedStream(byte[] Bytes, bool ExceededLimit);
}
