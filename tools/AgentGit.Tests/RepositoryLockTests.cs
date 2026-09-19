using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class RepositoryLockTests
{
    [TestMethod]
    public async Task AcquireAsync_serializes_mutations_using_common_git_directory()
    {
        var directory = Directory.CreateTempSubdirectory("AgentGitLock-").FullName;
        try
        {
            await using (var first = await RepositoryLock.AcquireAsync(directory))
            {
                using var cancellation = new CancellationTokenSource(TimeSpan.FromMilliseconds(250));
                await Assert.ThrowsExceptionAsync<TaskCanceledException>(
                    async () => await RepositoryLock.AcquireAsync(directory, cancellation.Token));
            }

            await using var second = await RepositoryLock.AcquireAsync(directory);
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }
}
