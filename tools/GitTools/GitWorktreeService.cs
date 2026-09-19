using System.Diagnostics;
using GitSupport;

namespace GitTools;

public sealed class GitWorktreeService
{
    public async Task<IReadOnlyList<Worktree>> GetWorktreesAsync(string root, CancellationToken cancellation_token = default)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(root);

        if (!Directory.Exists(root))
        {
            throw new DirectoryNotFoundException($"Git worktree root does not exist: '{root}'.");
        }

        var start_info = new ProcessStartInfo
        {
            FileName = "git",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        start_info.ArgumentList.Add("-C");
        start_info.ArgumentList.Add(root);
        start_info.ArgumentList.Add("worktree");
        start_info.ArgumentList.Add("list");
        start_info.ArgumentList.Add("--porcelain");
        start_info.ArgumentList.Add("-z");

        using var process = new Process { StartInfo = start_info };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            throw new InvalidOperationException("Unable to start Git. Ensure 'git' is installed and available on PATH.", exception);
        }

        var standard_output_task = process.StandardOutput.ReadToEndAsync(cancellation_token);
        var standard_error_task = process.StandardError.ReadToEndAsync(cancellation_token);

        await process.WaitForExitAsync(cancellation_token);
        var standard_output = await standard_output_task;
        var standard_error = await standard_error_task;

        if (process.ExitCode != 0)
        {
            var diagnostic = string.IsNullOrWhiteSpace(standard_error)
                ? "Git produced no diagnostic output."
                : standard_error.Trim();
            throw new InvalidOperationException($"Git worktree discovery failed with exit code {process.ExitCode}: {diagnostic}");
        }

        return WorktreePorcelainParser.ParseNullDelimited(standard_output);
    }
}
