using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace GamePackageTools.Tests;

[TestClass]
public sealed class ProgramTests
{
    [TestMethod]
    public void TryParse_accepts_all_required_long_options()
    {
        var parsed = Program.TryParse(
        [
            "--project-root", "project root",
            "--package-root", "package root",
            "--unreal-pak", "UnrealPak.exe",
            "--verification-directory", "verification",
            "--configuration", "shipping",
        ],
        out var request);

        Assert.IsTrue(parsed);
        Assert.IsNotNull(request);
        Assert.AreEqual(PackageConfiguration.Shipping, request.Configuration);
    }

    [TestMethod]
    public void Main_returns_usage_exit_code_for_invalid_arguments()
    {
        Assert.AreEqual(2, Program.Main([]).GetAwaiter().GetResult());
    }
}
