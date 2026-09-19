using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;

namespace AgentGit;

internal sealed class PolicyDocument
{
    public int Version { get; init; }

    public required string RepositoryId { get; init; }

    public required string BaseBranch { get; init; }

    public required Dictionary<string, BranchMatcherDocument[]> BranchGroups { get; init; }

    public string[] GitExtensions { get; init; } = [];

    public required Dictionary<string, OperationPolicyDocument> Policies { get; init; }
}

internal sealed class BranchMatcherDocument
{
    public string? Exact { get; init; }

    public string? Regex { get; init; }
}

internal sealed class OperationPolicyDocument
{
    public string[] AllowedCurrentGroups { get; init; } = [];

    public string[] AllowedTargetGroups { get; init; } = [];

    public bool RequireClean { get; init; }

    public string? RequireMergedInto { get; init; }
}

internal sealed record BranchMatcher(string Description, Func<string, bool> Matches);

internal sealed record OperationPolicy(
    IReadOnlySet<BranchClassification> AllowedCurrentGroups,
    IReadOnlySet<BranchClassification> AllowedTargetGroups,
    bool RequireClean,
    bool RequireMergedIntoBase);

internal sealed class GitPolicy(
    string repository_id,
    string base_branch,
    IReadOnlyList<BranchMatcher> protected_matchers,
    IReadOnlyList<BranchMatcher> workspace_matchers,
    IReadOnlySet<string> git_extensions,
    IReadOnlyDictionary<AgentGitOperation, OperationPolicy> operations)
{
    public string RepositoryId { get; } = repository_id;

    public string BaseBranch { get; } = base_branch;

    public IReadOnlySet<string> GitExtensions { get; } = git_extensions;

    public IReadOnlyDictionary<AgentGitOperation, OperationPolicy> Operations { get; } = operations;

    public BranchClassification Classify(string branch)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(branch);

        var protected_match = protected_matchers.Any(matcher => matcher.Matches(branch));
        var workspace_match = workspace_matchers.Any(matcher => matcher.Matches(branch));
        if (protected_match && workspace_match)
        {
            throw new PolicyConfigurationException(
                $"Branch '{branch}' matches both protected and workspace groups; classification is ambiguous.");
        }

        if (protected_match)
        {
            return BranchClassification.Protected;
        }

        return workspace_match ? BranchClassification.Workspace : BranchClassification.Feature;
    }
}

internal static class PolicyLoader
{
    private static readonly JsonSerializerOptions json_options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = false,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
    };

    private static readonly IReadOnlyDictionary<string, AgentGitOperation> operation_names =
        new Dictionary<string, AgentGitOperation>(StringComparer.Ordinal)
        {
            ["add"] = AgentGitOperation.Add,
            ["addAll"] = AgentGitOperation.AddAll,
            ["commit"] = AgentGitOperation.Commit,
            ["switch"] = AgentGitOperation.Switch,
            ["switchCreate"] = AgentGitOperation.SwitchCreate,
            ["rebaseBase"] = AgentGitOperation.RebaseBase,
            ["branchDelete"] = AgentGitOperation.BranchDelete,
        };

    public static GitPolicy Parse(string json)
    {
        ArgumentNullException.ThrowIfNull(json);

        PolicyDocument document;
        try
        {
            StrictJson.RejectDuplicateProperties(json);
            document = JsonSerializer.Deserialize<PolicyDocument>(json, json_options)
                ?? throw new PolicyConfigurationException("Policy JSON contained null.");
        }
        catch (JsonException exception)
        {
            throw new PolicyConfigurationException($"Policy JSON is malformed: {exception.Message}", exception);
        }

        if (document.Version != 1)
        {
            throw new PolicyConfigurationException($"Unsupported policy version {document.Version}; expected version 1.");
        }

        ValidateBranchNameField(document.RepositoryId, "repositoryId");
        ValidateBranchNameField(document.BaseBranch, "baseBranch");
        if (document.BranchGroups is null || document.BranchGroups.Count != 2 ||
            !document.BranchGroups.ContainsKey("protected") ||
            !document.BranchGroups.ContainsKey("workspace"))
        {
            throw new PolicyConfigurationException("branchGroups must contain exactly 'protected' and 'workspace'.");
        }

        if (document.GitExtensions is null)
        {
            throw new PolicyConfigurationException("gitExtensions cannot be null.");
        }

        if (document.Policies is null)
        {
            throw new PolicyConfigurationException("policies cannot be null.");
        }

        var protected_matchers = CompileMatchers("protected", document.BranchGroups["protected"]);
        var workspace_matchers = CompileMatchers("workspace", document.BranchGroups["workspace"]);
        var extensions = new HashSet<string>(StringComparer.Ordinal);
        foreach (var extension in document.GitExtensions)
        {
            if (!string.Equals(extension, "lfs", StringComparison.Ordinal) || !extensions.Add(extension))
            {
                throw new PolicyConfigurationException($"Unknown or duplicate Git extension '{extension}'.");
            }
        }

        var operations = new Dictionary<AgentGitOperation, OperationPolicy>();
        foreach (var pair in document.Policies)
        {
            if (!operation_names.TryGetValue(pair.Key, out var operation))
            {
                throw new PolicyConfigurationException($"Unknown policy operation '{pair.Key}'.");
            }

            operations.Add(operation, CompileOperation(operation, pair.Value));
        }

        var policy = new GitPolicy(
            document.RepositoryId,
            document.BaseBranch,
            protected_matchers,
            workspace_matchers,
            extensions,
            operations);
        if (policy.Classify(document.BaseBranch) != BranchClassification.Protected)
        {
            throw new PolicyConfigurationException("baseBranch must match the protected branch group.");
        }

        return policy;
    }

    private static IReadOnlyList<BranchMatcher> CompileMatchers(
        string group,
        IReadOnlyList<BranchMatcherDocument>? documents)
    {
        if (documents is null || documents.Count == 0)
        {
            throw new PolicyConfigurationException($"Branch group '{group}' must contain at least one matcher.");
        }

        var result = new List<BranchMatcher>();
        foreach (var document in documents)
        {
            if (document is null)
            {
                throw new PolicyConfigurationException($"Branch group '{group}' cannot contain a null matcher.");
            }

            if ((document.Exact is null) == (document.Regex is null))
            {
                throw new PolicyConfigurationException(
                    $"Each matcher in branch group '{group}' must specify exactly one of exact or regex.");
            }

            if (document.Exact is { } exact)
            {
                ValidateBranchNameField(exact, $"branchGroups.{group}.exact");
                result.Add(new BranchMatcher($"exact:{exact}", value =>
                    string.Equals(value, exact, StringComparison.OrdinalIgnoreCase)));
                continue;
            }

            try
            {
                var regex = new Regex(
                    document.Regex!,
                    RegexOptions.CultureInvariant | RegexOptions.IgnoreCase | RegexOptions.NonBacktracking,
                    TimeSpan.FromMilliseconds(100));
                result.Add(new BranchMatcher($"regex:{document.Regex}", regex.IsMatch));
            }
            catch (ArgumentException exception)
            {
                throw new PolicyConfigurationException(
                    $"Invalid regex in branch group '{group}': {exception.Message}", exception);
            }
        }

        return result;
    }

    private static OperationPolicy CompileOperation(AgentGitOperation operation, OperationPolicyDocument? document)
    {
        if (document is null)
        {
            throw new PolicyConfigurationException(
                $"Policy '{CommandLine.OperationName(operation)}' cannot be null.");
        }

        var current = CompileGroups(document.AllowedCurrentGroups, operation, "allowedCurrentGroups");
        var target = CompileGroups(document.AllowedTargetGroups, operation, "allowedTargetGroups");
        var require_merged = document.RequireMergedInto is not null;
        if (require_merged && !string.Equals(document.RequireMergedInto, "base", StringComparison.Ordinal))
        {
            throw new PolicyConfigurationException(
                $"Policy '{CommandLine.OperationName(operation)}' may only require merging into 'base'.");
        }

        ValidateSafetyCeiling(operation, current, target, document.RequireClean, require_merged);
        return new OperationPolicy(current, target, document.RequireClean, require_merged);
    }

    private static IReadOnlySet<BranchClassification> CompileGroups(
        IEnumerable<string>? groups,
        AgentGitOperation operation,
        string field)
    {
        if (groups is null)
        {
            throw new PolicyConfigurationException(
                $"Policy '{CommandLine.OperationName(operation)}' field '{field}' cannot be null.");
        }

        var result = new HashSet<BranchClassification>();
        foreach (var group in groups)
        {
            var classification = group switch
            {
                "protected" => BranchClassification.Protected,
                "workspace" => BranchClassification.Workspace,
                "feature" => BranchClassification.Feature,
                _ => throw new PolicyConfigurationException(
                    $"Policy '{CommandLine.OperationName(operation)}' references unknown branch group '{group}'."),
            };
            if (!result.Add(classification))
            {
                throw new PolicyConfigurationException(
                    $"Policy '{CommandLine.OperationName(operation)}' contains duplicate {field} value '{group}'.");
            }
        }

        return result;
    }

    private static void ValidateSafetyCeiling(
        AgentGitOperation operation,
        IReadOnlySet<BranchClassification> current,
        IReadOnlySet<BranchClassification> target,
        bool require_clean,
        bool require_merged)
    {
        var feature_only_current = operation is AgentGitOperation.Add or AgentGitOperation.AddAll or
            AgentGitOperation.Commit or AgentGitOperation.RebaseBase;
        if (feature_only_current && current.Any(value => value != BranchClassification.Feature))
        {
            throw new PolicyConfigurationException(
                $"Policy '{CommandLine.OperationName(operation)}' cannot allow protected or workspace branches.");
        }

        if (operation == AgentGitOperation.Switch &&
            (current.Contains(BranchClassification.Protected) ||
             target.Contains(BranchClassification.Protected) ||
             !require_clean))
        {
            throw new PolicyConfigurationException(
                "Switch policy must require a clean tree and cannot allow protected current or target branches.");
        }

        if (operation == AgentGitOperation.SwitchCreate &&
            (current.Contains(BranchClassification.Protected) ||
             target.Any(value => value != BranchClassification.Feature)))
        {
            throw new PolicyConfigurationException(
                "Switch-create policy cannot allow protected current branches and may target only feature branches.");
        }

        if (operation == AgentGitOperation.RebaseBase && !require_clean)
        {
            throw new PolicyConfigurationException("Rebase-base policy must require a clean tree.");
        }

        if (operation == AgentGitOperation.BranchDelete &&
            (target.Any(value => value != BranchClassification.Feature) || !require_clean || !require_merged))
        {
            throw new PolicyConfigurationException(
                "Branch-delete policy must target only feature branches, require cleanliness, and require merging into base.");
        }
    }

    private static void ValidateBranchNameField(string value, string field)
    {
        if (string.IsNullOrWhiteSpace(value) || value.Any(char.IsControl))
        {
            throw new PolicyConfigurationException(
                $"Policy field '{field}' cannot be empty or contain control characters.");
        }
    }
}
