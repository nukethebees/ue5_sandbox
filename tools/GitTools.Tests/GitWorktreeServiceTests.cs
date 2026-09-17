using GitTools;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace GitTools.Tests;

[TestClass]
public sealed class GitWorktreeServiceTests
{
    [TestMethod]
    public async Task GetWorktreesAsync_discovers_the_current_repository_worktree()
    {
        var repository_root = FindRepositoryRoot();
        var service = new GitWorktreeService();

        var worktrees = await service.GetWorktreesAsync(repository_root);

        Assert.IsTrue(
            worktrees.Any(worktree => PathsEqual(worktree.Path, repository_root)),
            $"Git did not report the current worktree '{repository_root}'.");
    }

    private static string FindRepositoryRoot()
    {
        for (var directory = new DirectoryInfo(AppContext.BaseDirectory); directory is not null; directory = directory.Parent)
        {
            var navigation_script = Path.Combine(directory.FullName, "PowerShell", "Navigation.ps1");
            if (File.Exists(Path.Combine(directory.FullName, "dev.ps1")) && File.Exists(navigation_script))
            {
                return directory.FullName;
            }
        }

        throw new AssertFailedException("Unable to locate the repository root from the test output directory.");
    }

    private static bool PathsEqual(string left, string right)
    {
        return string.Equals(
            Path.GetFullPath(left).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            Path.GetFullPath(right).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            StringComparison.OrdinalIgnoreCase);
    }
}
