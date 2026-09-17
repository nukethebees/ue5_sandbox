using System.Text.Json;

namespace GitTools;

public static class Program
{
    private static readonly JsonSerializerOptions json_options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    };

    public static async Task<int> Main(string[] arguments)
    {
        if (arguments.Length != 4 ||
            !string.Equals(arguments[0], "worktree", StringComparison.Ordinal) ||
            !string.Equals(arguments[1], "list", StringComparison.Ordinal) ||
            !string.Equals(arguments[2], "--root", StringComparison.Ordinal) ||
            string.IsNullOrWhiteSpace(arguments[3]))
        {
            Console.Error.WriteLine("Usage: GitTools worktree list --root <path>");
            return 2;
        }

        try
        {
            var service = new GitWorktreeService();
            var worktrees = await service.GetWorktreesAsync(arguments[3]);
            Console.Out.WriteLine(JsonSerializer.Serialize(worktrees, json_options));
            return 0;
        }
        catch (Exception exception) when (exception is ArgumentException or DirectoryNotFoundException or FormatException or InvalidOperationException)
        {
            Console.Error.WriteLine($"GitTools: {exception.Message}");
            return 1;
        }
    }
}
