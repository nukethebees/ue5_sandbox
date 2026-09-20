using System.ComponentModel;
using System.Diagnostics;
using System.Text;

namespace AgentGitInstaller;

internal sealed record ProcessRequest(
    string FileName,
    IReadOnlyList<string> Arguments,
    string WorkingDirectory,
    IReadOnlyDictionary<string, string?>? Environment = null,
    TimeSpan? Timeout = null,
    int MaximumCapturedStreamBytes = 64 * 1024 * 1024);

internal sealed record ProcessResult(int ExitCode, string StandardOutput, string StandardError);

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

        if (request.Environment is not null)
        {
            foreach (var pair in request.Environment)
            {
                if (pair.Value is null)
                {
                    start_info.Environment.Remove(pair.Key);
                }
                else
                {
                    start_info.Environment[pair.Key] = pair.Value;
                }
            }
        }

        using var process = new Process { StartInfo = start_info };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or Win32Exception)
        {
            throw new InstallerException($"Unable to start executable '{request.FileName}'.", exception);
        }

        using var timeout_source = CancellationTokenSource.CreateLinkedTokenSource(cancellation_token);
        timeout_source.CancelAfter(request.Timeout ?? TimeSpan.FromMinutes(30));
        var output_task = CaptureStreamAsync(
            process.StandardOutput.BaseStream,
            request.MaximumCapturedStreamBytes,
            timeout_source.Token);
        var error_task = CaptureStreamAsync(
            process.StandardError.BaseStream,
            request.MaximumCapturedStreamBytes,
            timeout_source.Token);
        try
        {
            await process.WaitForExitAsync(timeout_source.Token);
            await Task.WhenAll(output_task, error_task);
        }
        catch (OperationCanceledException) when (!cancellation_token.IsCancellationRequested)
        {
            if (!process.HasExited)
            {
                process.Kill(entireProcessTree: true);
                await process.WaitForExitAsync(CancellationToken.None);
            }

            throw new InstallerException(
                $"Executable '{request.FileName}' timed out after " +
                $"{(request.Timeout ?? TimeSpan.FromMinutes(30)).TotalSeconds:0} seconds.");
        }

        var output = await output_task;
        var error = await error_task;
        if (output.ExceededLimit || error.ExceededLimit)
        {
            throw new InstallerException(
                $"Executable '{request.FileName}' exceeded its " +
                $"{request.MaximumCapturedStreamBytes} byte diagnostic output limit.");
        }

        return new ProcessResult(
            process.ExitCode,
            Encoding.UTF8.GetString(output.Bytes),
            Encoding.UTF8.GetString(error.Bytes));
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
