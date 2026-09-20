using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class CommandArgumentsTests
{
    private static readonly HashSet<string> values = ["--foo", "--bar"];
    private static readonly HashSet<string> flags = ["--verbose"];

    [TestMethod]
    public void Parse_accepts_space_separated_values()
    {
        var parsed = CommandArguments.Parse(["--foo", "value"], values, flags);

        Assert.AreEqual("value", parsed.Required("--foo"));
    }

    [TestMethod]
    public void Parse_accepts_equals_separated_values_and_mixed_syntax()
    {
        var parsed = CommandArguments.Parse(["--foo=value", "--bar", "another=value"], values, flags);

        Assert.AreEqual("value", parsed.Required("--foo"));
        Assert.AreEqual("another=value", parsed.Required("--bar"));
    }

    [TestMethod]
    public void Parse_rejects_duplicate_values_across_syntaxes()
    {
        Assert.ThrowsException<BenchmarkToolException>(() => CommandArguments.Parse(["--foo", "one", "--foo=two"], values, flags));
    }

    [TestMethod]
    public void Parse_rejects_empty_equals_value_and_unknown_equals_option()
    {
        Assert.ThrowsException<BenchmarkToolException>(() => CommandArguments.Parse(["--foo="], values, flags));
        Assert.ThrowsException<BenchmarkToolException>(() => CommandArguments.Parse(["--unknown=value"], values, flags));
    }

    [TestMethod]
    public void Parse_preserves_exact_flag_behavior()
    {
        var parsed = CommandArguments.Parse(["--verbose", "--foo=value"], values, flags);

        Assert.IsTrue(parsed.HasFlag("--verbose"));
        Assert.ThrowsException<BenchmarkToolException>(() => CommandArguments.Parse(["--verbose=value"], values, flags));
    }
}
