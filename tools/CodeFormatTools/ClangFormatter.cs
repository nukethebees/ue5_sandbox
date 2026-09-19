namespace CodeFormatTools;

internal sealed record FormatFileResult(bool Success, string? Error);

internal interface IFileFormatter
{
    Task EnsureAvailableAsync(CancellationToken cancellation_token);

    Task<FormatFileResult> FormatAsync(string file_path, CancellationToken cancellation_token);
}

internal sealed class ClangFormatter(IProcessRunner process_runner, string executable = "clang-format") : IFileFormatter
{
    private static readonly TimeSpan timeout = TimeSpan.FromSeconds(30);

    public async Task EnsureAvailableAsync(CancellationToken cancellation_token)
    {
        try
        {
            var result = await process_runner.RunAsync(
                new ProcessRequest(executable, ["--version"], Environment.CurrentDirectory, timeout),
                cancellation_token);
            if (result.ExitCode != 0)
            {
                var diagnostic = string.IsNullOrWhiteSpace(result.StandardError)
                    ? $"clang-format exited with code {result.ExitCode}."
                    : result.StandardError.Trim();
                throw new FormatToolException($"Unable to use clang-format: {diagnostic}");
            }
        }
        catch (ProcessLaunchException exception)
        {
            throw new FormatToolException(
                "Unable to start clang-format. Ensure it is installed and available on PATH.",
                exception);
        }
        catch (ProcessTimeoutException exception)
        {
            throw new FormatToolException($"clang-format availability check timed out: {exception.Message}", exception);
        }
    }

    public async Task<FormatFileResult> FormatAsync(string file_path, CancellationToken cancellation_token)
    {
        try
        {
            var result = await process_runner.RunAsync(
                new ProcessRequest(executable, ["-i", file_path], Path.GetDirectoryName(file_path)!, timeout),
                cancellation_token);
            if (result.ExitCode != 0)
            {
                return new FormatFileResult(
                    false,
                    string.IsNullOrWhiteSpace(result.StandardError)
                        ? $"clang-format exited with code {result.ExitCode}."
                        : result.StandardError.Trim());
            }

            NewlineNormalizer.Normalize(file_path);
            return new FormatFileResult(true, null);
        }
        catch (ProcessTimeoutException)
        {
            return new FormatFileResult(false, "Timeout - file too large or clang-format hung.");
        }
        catch (ProcessLaunchException exception)
        {
            return new FormatFileResult(false, exception.Message);
        }
        catch (IOException exception)
        {
            return new FormatFileResult(false, exception.Message);
        }
    }
}

internal static class NewlineNormalizer
{
    public static bool Normalize(string file_path)
    {
        var contents = File.ReadAllBytes(file_path);
        var normalized = new byte[contents.Length];
        var source_index = 0;
        var destination_index = 0;
        var changed = false;

        while (source_index < contents.Length)
        {
            var current = contents[source_index++];
            if (current == '\r')
            {
                normalized[destination_index++] = (byte)'\n';
                changed = true;
                if (source_index < contents.Length && contents[source_index] == '\n')
                {
                    source_index++;
                }

                continue;
            }

            normalized[destination_index++] = current;
        }

        if (!changed)
        {
            return false;
        }

        File.WriteAllBytes(file_path, normalized.AsSpan(0, destination_index).ToArray());
        return true;
    }
}
