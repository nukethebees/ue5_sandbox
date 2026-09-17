using GitTools;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace GitTools.Tests;

[TestClass]
public sealed class WorktreePorcelainParserTests
{
    [TestMethod]
    public void Parse_returns_one_worktree_with_a_short_branch_name()
    {
        const string output = """
            worktree C:/src/sandbox
            HEAD 0123456789abcdef
            branch refs/heads/feature/worktrees

            """;

        var worktrees = WorktreePorcelainParser.Parse(output);

        Assert.AreEqual(1, worktrees.Count);
        Assert.AreEqual(new Worktree("sandbox", "C:/src/sandbox", "feature/worktrees"), worktrees[0]);
    }

    [TestMethod]
    public void Parse_returns_multiple_worktrees_and_ignores_standard_metadata()
    {
        const string output = """
            worktree C:/src/main
            HEAD 0123456789abcdef
            branch refs/heads/main
            locked maintenance

            worktree C:/src/topic
            HEAD fedcba9876543210
            branch refs/heads/topic
            prunable gitdir file points to non-existent location

            """;

        var worktrees = WorktreePorcelainParser.Parse(output);

        CollectionAssert.AreEqual(
            new[]
            {
                new Worktree("main", "C:/src/main", "main"),
                new Worktree("topic", "C:/src/topic", "topic"),
            },
            worktrees.ToArray());
    }

    [TestMethod]
    public void Parse_returns_null_branch_for_a_detached_worktree()
    {
        const string output = """
            worktree C:/src/detached
            HEAD 0123456789abcdef
            detached

            """;

        var worktrees = WorktreePorcelainParser.Parse(output);

        Assert.AreEqual(1, worktrees.Count);
        Assert.AreEqual(new Worktree("detached", "C:/src/detached", null), worktrees[0]);
    }

    [TestMethod]
    public void Parse_derives_names_and_completes_the_last_record_without_a_separator()
    {
        const string output = """

            worktree C:/src/worktrees/final-name/
            HEAD 0123456789abcdef
            branch refs/heads/final
            """;

        var worktrees = WorktreePorcelainParser.Parse(output);

        Assert.AreEqual(1, worktrees.Count);
        Assert.AreEqual(new Worktree("final-name", "C:/src/worktrees/final-name/", "final"), worktrees[0]);
    }

    [TestMethod]
    public void Parse_rejects_a_branch_before_a_worktree()
    {
        var exception = Assert.ThrowsException<FormatException>(() => WorktreePorcelainParser.Parse("branch refs/heads/main\n"));

        StringAssert.Contains(exception.Message, "before its worktree");
    }
}
