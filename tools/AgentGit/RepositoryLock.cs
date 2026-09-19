namespace AgentGit;

internal sealed class RepositoryLock : IAsyncDisposable
{
    private readonly FileStream stream;

    private RepositoryLock(FileStream stream)
    {
        this.stream = stream;
    }

    public static async Task<RepositoryLock> AcquireAsync(
        string common_git_directory,
        CancellationToken cancellation_token = default)
    {
        var lock_path = Path.Combine(common_git_directory, "agent-git.lock");
        try
        {
            if ((File.GetAttributes(lock_path) & FileAttributes.ReparsePoint) != 0)
            {
                throw new RepositoryStateException(
                    $"Repository lock path cannot be a symbolic link or reparse point: '{lock_path}'.");
            }
        }
        catch (Exception exception) when (exception is FileNotFoundException or DirectoryNotFoundException)
        {
            // The lock file is created below while its registered common Git directory is held fixed.
        }

        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(30);
        IOException? last_error = null;
        while (DateTime.UtcNow < deadline)
        {
            cancellation_token.ThrowIfCancellationRequested();
            try
            {
                var stream = new FileStream(
                    lock_path,
                    FileMode.OpenOrCreate,
                    FileAccess.ReadWrite,
                    FileShare.None,
                    bufferSize: 1,
                    FileOptions.Asynchronous);
                return new RepositoryLock(stream);
            }
            catch (IOException exception)
            {
                last_error = exception;
                await Task.Delay(TimeSpan.FromMilliseconds(100), cancellation_token);
            }
        }

        throw new RepositoryStateException(
            $"Timed out waiting for another agent-git mutation in '{common_git_directory}'.", last_error);
    }

    public async ValueTask DisposeAsync()
    {
        await stream.DisposeAsync();
    }
}
