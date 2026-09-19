using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class GitOutputParserTests
{
    private const string ordinary_prefix = "1";
    private const string object_id_1 = "0123456789012345678901234567890123456789";
    private const string object_id_2 = "abcdefabcdefabcdefabcdefabcdefabcdefabcd";
    private const string object_id_3 = "1111111111111111111111111111111111111111";
    private const string modes_and_ids =
        $"N... 100644 100644 100644 {object_id_1} {object_id_2}";

    [TestMethod]
    public void Status_parser_reads_ordinary_staged_and_unstaged_changes()
    {
        var staged = ParseStatus($"{ordinary_prefix} M. {modes_and_ids} staged.txt\0");
        var unstaged = ParseStatus($"{ordinary_prefix} .M {modes_and_ids} unstaged.txt\0");
        var combined = ParseStatus(
            $"{ordinary_prefix} A. {modes_and_ids} added.txt\0" +
            $"{ordinary_prefix} .D {modes_and_ids} deleted.txt\0");

        Assert.IsTrue(staged.HasStagedChanges);
        Assert.IsFalse(staged.HasUnstagedChanges);
        Assert.IsFalse(unstaged.HasStagedChanges);
        Assert.IsTrue(unstaged.HasUnstagedChanges);
        Assert.IsTrue(combined.HasStagedChanges);
        Assert.IsTrue(combined.HasUnstagedChanges);
    }

    [TestMethod]
    public void Status_parser_reads_rename_or_copy_and_consumes_following_pathname()
    {
        var status = ParseStatus($"2 R. {modes_and_ids} R100 new name.txt\0old name.txt\0");

        Assert.IsTrue(status.HasStagedChanges);
        Assert.IsFalse(status.HasUnstagedChanges);
    }

    [TestMethod]
    public void Status_parser_reads_unmerged_and_untracked_records()
    {
        var status = ParseStatus(
            $"u UU N... 100644 100644 100644 100644 " +
            $"{object_id_1} {object_id_2} {object_id_3} conflicted.txt\0" +
            "? untracked.txt\0");

        Assert.IsTrue(status.HasConflicts);
        Assert.IsTrue(status.HasStagedChanges);
        Assert.IsTrue(status.HasUnstagedChanges);
        Assert.IsTrue(status.HasUntrackedFiles);
    }

    [TestMethod]
    public void Status_parser_accepts_empty_output_and_one_terminal_empty_entry()
    {
        var empty = GitOutputParsers.ParseStatus([]);
        var terminated = ParseStatus($"{ordinary_prefix} .. {modes_and_ids} unchanged.txt\0");

        Assert.IsTrue(empty.IsClean);
        Assert.IsTrue(terminated.IsClean);
    }

    [TestMethod]
    public void Status_parser_rejects_malformed_truncated_and_unknown_records()
    {
        Assert.ThrowsException<RepositoryException>(() => ParseStatus("1 M. truncated\0"));
        Assert.ThrowsException<RepositoryStateException>(() => ParseStatus(
            $"{ordinary_prefix} M. {modes_and_ids} unterminated.txt"));
        Assert.ThrowsException<RepositoryException>(() => ParseStatus("x unsupported\0"));
        Assert.ThrowsException<RepositoryStateException>(() => ParseStatus(
            $"{ordinary_prefix} M. {modes_and_ids} first.txt\0\0"));
        Assert.ThrowsException<RepositoryException>(() => ParseStatus(
            $"2 M. {modes_and_ids} R100 malformed-rename.txt\0old.txt\0"));
        Assert.ThrowsException<RepositoryStateException>(() =>
            GitOutputParsers.ParseStatus([0xff, 0x00]));
    }

    [TestMethod]
    public void Status_parser_rejects_truncated_rename_records()
    {
        Assert.ThrowsException<RepositoryException>(() => ParseStatus(
            $"2 R. {modes_and_ids} R100 new.txt\0"));
    }

    [TestMethod]
    public void Config_parser_reads_exact_nul_delimited_names()
    {
        var names = ParseConfig("core.filemode\0remote.name with spaces.url\0include.path\0");

        CollectionAssert.AreEqual(
            new[] { "core.filemode", "remote.name with spaces.url", "include.path" },
            names.ToArray());
    }

    [TestMethod]
    public void Config_parser_rejects_ambiguous_whitespace_empty_and_malformed_records()
    {
        Assert.ThrowsException<RepositoryStateException>(() => ParseConfig(" include.path\0"));
        Assert.ThrowsException<RepositoryStateException>(() => ParseConfig("include.path \0"));
        Assert.ThrowsException<RepositoryStateException>(() => ParseConfig("include.\npath\0"));
        Assert.ThrowsException<RepositoryStateException>(() => ParseConfig("core.filemode\0\0"));
        Assert.ThrowsException<RepositoryStateException>(() => ParseConfig("core.filemode"));
        Assert.ThrowsException<RepositoryStateException>(() => ParseConfig("missing-section-separator\0"));
        Assert.ThrowsException<RepositoryStateException>(() =>
            GitOutputParsers.ParseConfigNames([0xff, 0x00]));
    }

    [TestMethod]
    public void Config_parser_accepts_empty_output()
    {
        Assert.AreEqual(0, GitOutputParsers.ParseConfigNames([]).Count);
    }

    private static WorkingTreeStatus ParseStatus(string value)
    {
        return GitOutputParsers.ParseStatus(Encoding.UTF8.GetBytes(value));
    }

    private static IReadOnlyList<string> ParseConfig(string value)
    {
        return GitOutputParsers.ParseConfigNames(Encoding.UTF8.GetBytes(value));
    }
}
