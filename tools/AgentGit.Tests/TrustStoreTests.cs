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

    [TestMethod]
    public void ValidateManifest_rejects_unsafe_policy_locations_and_duplicate_registrations()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var registration = fixture.Trust.Repositories.Single();
        var manifest = CreateManifest(fixture, registration);
        TrustStore.ValidateManifest(manifest);

        var unsafe_ref = CopyRegistration(registration, policy_ref: "refs/heads/dev:attacker");
        Assert.ThrowsException<PolicyConfigurationException>(
            () => TrustStore.ValidateManifest(CreateManifest(fixture, unsafe_ref)));

        var unsafe_path = CopyRegistration(registration, policy_path: "../.agent-git.json");
        Assert.ThrowsException<PolicyConfigurationException>(
            () => TrustStore.ValidateManifest(CreateManifest(fixture, unsafe_path)));

        Assert.ThrowsException<PolicyConfigurationException>(
            () => TrustStore.ValidateManifest(CreateManifest(fixture, registration, registration)));
    }

    private static TrustManifest CreateManifest(
        TemporaryAgentGitRepository fixture,
        params TrustedRepository[] repositories)
    {
        return new TrustManifest
        {
            Version = 1,
            GitExecutable = fixture.GitExecutable,
            UserName = "Agent Git Test",
            UserEmail = "agent-git@example.com",
            Repositories = repositories,
        };
    }

    private static TrustedRepository CopyRegistration(
        TrustedRepository source,
        string? policy_ref = null,
        string? policy_path = null)
    {
        return new TrustedRepository
        {
            RepositoryId = source.RepositoryId,
            CommonGitDirectory = source.CommonGitDirectory,
            OriginUrl = source.OriginUrl,
            PolicyRef = policy_ref ?? source.PolicyRef,
            PolicyPath = policy_path ?? source.PolicyPath,
        };
    }
}
