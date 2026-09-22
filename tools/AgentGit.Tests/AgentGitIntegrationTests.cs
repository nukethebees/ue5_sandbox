using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class AgentGitIntegrationTests
{
    [TestMethod]
    public async Task Discovery_works_from_subdirectory_and_reports_feature_policy()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/discovery");
        var subdirectory = Directory.CreateDirectory(Path.Combine(fixture.RepositoryRoot, "Source", "Nested")).FullName;

        var result = await fixture.RunAgentGitAsync(subdirectory, "policy", "commit");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        StringAssert.Contains(result.Output, "Current branch: feature/discovery");
        StringAssert.Contains(result.Output, "Classification: feature");
        StringAssert.Contains(result.Output, "Decision: ALLOW");
    }

    [TestMethod]
    public async Task Integration_info_reports_stable_candidate_metadata_as_json()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var worktree = fixture.CreateWorktree("dev1");
        fixture.RunGitAt(worktree, "switch", "-qc", "feature/info");
        fixture.WriteFile("tools/AgentGit/info.txt", "candidate\n", worktree);
        fixture.RunGitAt(worktree, "add", "tools/AgentGit/info.txt");
        fixture.RunGitAt(worktree, "commit", "-qm", "candidate");

        var result = await fixture.RunAgentGitAsync(worktree, "integration-info", "--json");
        using var json = System.Text.Json.JsonDocument.Parse(result.Output);

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        Assert.AreEqual("feature/info", json.RootElement.GetProperty("branch").GetString());
        Assert.AreEqual("dev", json.RootElement.GetProperty("baseBranch").GetString());
        Assert.AreEqual(64, json.RootElement.GetProperty("patchFingerprint").GetString()!.Length);
        Assert.AreEqual(40, json.RootElement.GetProperty("tree").GetString()!.Length);
    }

    [TestMethod]
    public async Task Discovery_allows_enabled_but_absent_worktree_configuration()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("config", "extensions.worktreeConfig", "true");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
    }

    [TestMethod]
    public async Task Config_name_parser_accepts_real_subsection_whitespace()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("config", "remote.name with spaces.url", "https://example.invalid/unused.git");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
    }

    [TestMethod]
    public async Task Add_and_commit_are_denied_on_protected_and_workspace_branches()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.WriteFile("protected.txt", "change\n");

        var protected_context = await fixture.DiscoverAsync(fixture.RepositoryRoot);
        var protected_add = await fixture.EvaluateAsync(protected_context, new AddAllRequest(false));
        var protected_commit = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "commit", "-m", "forbidden");

        Assert.IsFalse(protected_add.Decision.Allowed);
        Assert.AreEqual(ExitCodes.PolicyDenied, protected_commit.ExitCode);
        StringAssert.Contains(protected_commit.Output, "protected branch 'dev'");

        var workspace = fixture.CreateWorktree("dev1");
        fixture.WriteFile("workspace.txt", "change\n", workspace);
        var workspace_context = await fixture.DiscoverAsync(workspace);
        var workspace_add = await fixture.EvaluateAsync(workspace_context, new AddAllRequest(false));
        Assert.IsFalse(workspace_add.Decision.Allowed);
        StringAssert.Contains(workspace_add.Decision.Reason, "workspace branch 'dev1'");
    }

    [TestMethod]
    public async Task Switch_and_switch_create_cannot_move_a_protected_worktree()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("branch", "feature/existing", "dev");
        var original_head = fixture.RunGit("rev-parse", "HEAD").Trim();

        var switch_result = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "switch",
            "feature/existing");
        var context = await fixture.DiscoverAsync(fixture.RepositoryRoot);
        var create_result = await fixture.EvaluateAsync(
            context,
            new SwitchCreateRequest(false, "feature/new"));

        Assert.AreEqual(ExitCodes.PolicyDenied, switch_result.ExitCode, switch_result.Error);
        Assert.IsFalse(create_result.Decision.Allowed);
        StringAssert.Contains(switch_result.Output, "protected branch 'dev'");
        StringAssert.Contains(create_result.Decision.Reason, "protected branch 'dev'");
        Assert.AreEqual("dev\n", fixture.RunGit("branch", "--show-current"));
        Assert.AreEqual(original_head, fixture.RunGit("rev-parse", "HEAD").Trim());
        Assert.AreEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/new"));
    }

    [TestMethod]
    public async Task Permissive_switch_policy_cannot_move_a_protected_worktree()
    {
        foreach (var operation in new[] { "switch", "switchCreate" })
        {
            using var fixture = new TemporaryAgentGitRepository();
            fixture.RunGit("branch", "feature/existing", "dev");
            var policy = System.Text.Json.Nodes.JsonNode.Parse(PolicyLoaderTests.ValidPolicy())!.AsObject();
            policy["policies"]![operation]!["allowedCurrentGroups"] =
                new System.Text.Json.Nodes.JsonArray("protected", "workspace", "feature");
            fixture.WriteFile(".agent-git.json", policy.ToJsonString());
            fixture.RunGit("add", ".agent-git.json");
            fixture.RunGit("commit", "-qm", "attempt to broaden switch policy");
            var original_head = fixture.RunGit("rev-parse", "HEAD").Trim();

            var result = operation == "switch"
                ? await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "switch", "feature/existing")
                : await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "switch-create", "feature/new");

            Assert.AreEqual(ExitCodes.ConfigurationFailure, result.ExitCode, result.Error);
            StringAssert.Contains(result.Error, "protected current");
            Assert.AreEqual("dev\n", fixture.RunGit("branch", "--show-current"));
            Assert.AreEqual(original_head, fixture.RunGit("rev-parse", "HEAD").Trim());
            Assert.AreEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/new"));
        }
    }

    [TestMethod]
    public async Task Malformed_policy_cannot_move_a_protected_worktree()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("branch", "feature/existing", "dev");
        fixture.WriteFile(".agent-git.json", "{ malformed policy }");
        fixture.RunGit("add", ".agent-git.json");
        fixture.RunGit("commit", "-qm", "malformed policy");
        var original_head = fixture.RunGit("rev-parse", "HEAD").Trim();

        var switch_result = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "switch",
            "feature/existing");
        var create_result = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "switch-create",
            "feature/new");

        Assert.AreEqual(ExitCodes.ConfigurationFailure, switch_result.ExitCode, switch_result.Error);
        Assert.AreEqual(ExitCodes.ConfigurationFailure, create_result.ExitCode, create_result.Error);
        Assert.AreEqual("dev\n", fixture.RunGit("branch", "--show-current"));
        Assert.AreEqual(original_head, fixture.RunGit("rev-parse", "HEAD").Trim());
        Assert.AreEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/new"));
    }

    [TestMethod]
    public async Task Switch_create_carries_changes_then_add_and_commit_preserve_metacharacters_as_data()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var workspace = fixture.CreateWorktree("dev1");
        const string file_name = "odd & name.txt";
        const string message = "literal & whoami; $(touch escaped-marker)";
        fixture.WriteFile(file_name, "content\n", workspace);

        var create = await fixture.RunAgentGitAsync(workspace, "switch-create", "feature/odd-input");
        var add = await fixture.RunAgentGitAsync(workspace, "add", file_name);
        var commit = await fixture.RunAgentGitAsync(workspace, "commit", "-m", message);

        Assert.AreEqual(ExitCodes.Success, create.ExitCode, create.Error);
        Assert.AreEqual(ExitCodes.Success, add.ExitCode, add.Error);
        Assert.AreEqual(ExitCodes.Success, commit.ExitCode, commit.Error);
        Assert.AreEqual(message + "\n", fixture.RunGitAt(workspace, "log", "-1", "--format=%s"));
        Assert.IsFalse(File.Exists(Path.Combine(workspace, "escaped-marker")));
    }

    [TestMethod]
    public async Task Switch_requires_clean_tree_but_allows_return_to_own_home_branch()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var workspace = fixture.CreateWorktree("feature/switch", "dev1");
        fixture.RunGit("branch", "dev1", "dev");
        fixture.WriteFile("dirty.txt", "dirty\n", workspace);

        var dirty = await fixture.RunAgentGitAsync(workspace, "switch", "dev1");
        Assert.AreEqual(ExitCodes.PolicyDenied, dirty.ExitCode);
        StringAssert.Contains(dirty.Output, "requires a clean");

        File.Delete(Path.Combine(workspace, "dirty.txt"));
        var clean = await fixture.RunAgentGitAsync(workspace, "switch", "dev1");
        Assert.AreEqual(ExitCodes.Success, clean.ExitCode, clean.Error);
        Assert.AreEqual("dev1\n", fixture.RunGitAt(workspace, "branch", "--show-current"));
    }

    [TestMethod]
    public async Task Feature_worktree_policy_edit_does_not_change_protected_ref_policy()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/policy-tamper");
        var permissive = PolicyLoaderTests.ValidPolicy().Replace(
            "\"allowedCurrentGroups\": [\"feature\"]",
            "\"allowedCurrentGroups\": [\"feature\", \"protected\"]",
            StringComparison.Ordinal);
        fixture.WriteFile(".agent-git.json", permissive);
        fixture.RunGit("add", ".agent-git.json");
        fixture.RunGit("commit", "-qm", "tamper policy");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "policy", "commit");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        StringAssert.Contains(result.Output, "Decision: ALLOW");
        var branch_info = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "branch-info");
        StringAssert.Contains(branch_info.Output, "Policy commit:");
        Assert.AreNotEqual(
            fixture.RunGit("rev-parse", "HEAD").Trim(),
            fixture.RunGit("rev-parse", "dev").Trim());
    }

    [TestMethod]
    public async Task Dry_run_evaluates_without_mutating()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var workspace = fixture.CreateWorktree("dev1");

        var result = await fixture.RunAgentGitAsync(
            workspace,
            "--dry-run",
            "switch-create",
            "feature/dry-run");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        StringAssert.Contains(result.Output, "Dry run: no mutation executed");
        Assert.AreEqual("dev1\n", fixture.RunGitAt(workspace, "branch", "--show-current"));
        Assert.AreEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/dry-run"));
    }
}
