using System.Text.Json.Nodes;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class PolicyLoaderTests
{
    [TestMethod]
    public void Parse_classifies_protected_workspace_and_feature_branches()
    {
        var policy = PolicyLoader.Parse(ValidPolicy());

        Assert.AreEqual(BranchClassification.Protected, policy.Classify("DEV"));
        Assert.AreEqual(BranchClassification.Workspace, policy.Classify("Dev12"));
        Assert.AreEqual(BranchClassification.Feature, policy.Classify("fighter-ai"));
    }

    [TestMethod]
    public void Parse_rejects_unknown_fields()
    {
        var json = ValidPolicy().Replace("\"version\": 1,", "\"version\": 1, \"escapeHatch\": true,", StringComparison.Ordinal);

        var exception = Assert.ThrowsException<PolicyConfigurationException>(() => PolicyLoader.Parse(json));

        StringAssert.Contains(exception.Message, "malformed");
    }

    [TestMethod]
    public void Parse_rejects_unknown_versions_and_operations()
    {
        var version = ValidPolicy().Replace("\"version\": 1", "\"version\": 2", StringComparison.Ordinal);
        Assert.ThrowsException<PolicyConfigurationException>(() => PolicyLoader.Parse(version));

        var operation = ValidPolicy().Replace("\"commit\":", "\"push\":", StringComparison.Ordinal);
        var exception = Assert.ThrowsException<PolicyConfigurationException>(() => PolicyLoader.Parse(operation));
        StringAssert.Contains(exception.Message, "Unknown policy operation");
    }

    [TestMethod]
    public void Parse_rejects_invalid_and_ambiguous_matchers()
    {
        var invalid = ValidPolicy().Replace("\"^dev[0-9]+$\"", "\"[\"", StringComparison.Ordinal);
        Assert.ThrowsException<PolicyConfigurationException>(() => PolicyLoader.Parse(invalid));

        var overlap = ValidPolicy().Replace(
            "{ \"regex\": \"^dev[0-9]+$\" }",
            "{ \"regex\": \"^dev.*$\" }",
            StringComparison.Ordinal);
        Assert.ThrowsException<PolicyConfigurationException>(() => PolicyLoader.Parse(overlap));
    }

    [TestMethod]
    public void Parse_rejects_policy_that_broadens_compiled_safety_ceiling()
    {
        var json = ValidPolicy().Replace(
            "\"allowedCurrentGroups\": [\"feature\"]",
            "\"allowedCurrentGroups\": [\"feature\", \"protected\"]",
            StringComparison.Ordinal);

        var exception = Assert.ThrowsException<PolicyConfigurationException>(() => PolicyLoader.Parse(json));

        StringAssert.Contains(exception.Message, "cannot allow protected");
    }

    [TestMethod]
    public void Parse_rejects_null_structures_and_dirty_rebase_policy()
    {
        var null_groups = JsonNode.Parse(ValidPolicy())!.AsObject();
        null_groups["branchGroups"] = null;
        Assert.ThrowsException<PolicyConfigurationException>(() => PolicyLoader.Parse(null_groups.ToJsonString()));

        var null_operation = JsonNode.Parse(ValidPolicy())!.AsObject();
        null_operation["policies"]!["commit"] = null;
        Assert.ThrowsException<PolicyConfigurationException>(() => PolicyLoader.Parse(null_operation.ToJsonString()));

        var dirty_rebase = JsonNode.Parse(ValidPolicy())!.AsObject();
        dirty_rebase["policies"]!["rebaseBase"]!["requireClean"] = false;
        var exception = Assert.ThrowsException<PolicyConfigurationException>(
            () => PolicyLoader.Parse(dirty_rebase.ToJsonString()));
        StringAssert.Contains(exception.Message, "must require a clean tree");
    }

    internal static string ValidPolicy(bool include_lfs = false)
    {
        return $$"""
            {
              "version": 1,
              "repositoryId": "test/repository",
              "baseBranch": "dev",
              "branchGroups": {
                "protected": [{ "exact": "dev" }, { "exact": "main" }],
                "workspace": [{ "regex": "^dev[0-9]+$" }]
              },
              "gitExtensions": {{(include_lfs ? "[\"lfs\"]" : "[]")}},
              "policies": {
                "add": { "allowedCurrentGroups": ["feature"] },
                "addAll": { "allowedCurrentGroups": ["feature"] },
                "commit": { "allowedCurrentGroups": ["feature"] },
                "switch": {
                  "allowedCurrentGroups": ["protected", "workspace", "feature"],
                  "allowedTargetGroups": ["workspace", "feature"],
                  "requireClean": true
                },
                "switchCreate": {
                  "allowedCurrentGroups": ["protected", "workspace", "feature"],
                  "allowedTargetGroups": ["feature"]
                },
                "rebaseBase": {
                  "allowedCurrentGroups": ["feature"],
                  "requireClean": true
                },
                "branchDelete": {
                  "allowedCurrentGroups": ["protected", "workspace", "feature"],
                  "allowedTargetGroups": ["feature"],
                  "requireClean": true,
                  "requireMergedInto": "base"
                }
              }
            }
            """;
    }
}
