using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class TrustStoreTests
{
    [TestMethod]
    public void LoadCanonical_rejects_repository_build_output_before_loading_manifest()
    {
        var repository_binary = typeof(Program).Assembly.Location;

        var exception = Assert.ThrowsException<PolicyConfigurationException>(
            () => TrustStore.LoadCanonical(repository_binary));

        StringAssert.Contains(exception.Message, "Repository build outputs are not trusted");
    }

    [TestMethod]
    public void FindRegistration_rejects_missing_and_duplicate_repository_identities()
    {
        var registration = new TrustedRepository
        {
            RepositoryId = "test/repository",
            CommonGitDirectory = Path.GetFullPath("repository.git"),
            OriginUrl = "https://example.invalid/repository.git",
            PolicyRef = "refs/heads/dev",
            PolicyPath = ".agent-git.json",
        };
        var trust = new TrustContext("install", "git", null, "Test", "test@example.com", "config", "hooks", [registration]);

        Assert.AreSame(registration, trust.FindRegistration(registration.CommonGitDirectory));
        Assert.ThrowsException<RepositoryException>(() => trust.FindRegistration(Path.GetFullPath("other.git")));

        var duplicate = trust with { Repositories = [registration, registration] };
        Assert.ThrowsException<PolicyConfigurationException>(() => duplicate.FindRegistration(registration.CommonGitDirectory));
    }
}
