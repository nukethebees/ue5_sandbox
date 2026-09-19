using System.Diagnostics;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace CodeFormatTools.Tests;

internal sealed class TemporaryGitRepository : IDisposable
{
    public TemporaryGitRepository(string prefix = "CodeFormatTools")
    {
        Root = Directory.CreateTempSubdirectory($"{prefix}-").FullName;
        Directory.CreateDirectory(PathFor("Source"));
        RunGit("init", "-q");
        RunGit("config", "user.email", "format-test@example.com");
        RunGit("config", "user.name", "Code Format Test");
    }

    public string Root { get; }

    public string PathFor(string relative_path)
    {
        return Path.GetFullPath(Path.Combine(Root, relative_path));
    }

    public string WriteFile(string relative_path, string contents = "int main() {}\n")
    {
        var path = PathFor(relative_path);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, contents);
        return path;
    }

    public FormattingScope CreateScope()
    {
        return new FormattingScope(Root, ["Source"]);
    }

    public void CommitAll()
    {
        RunGit("add", "--all");
        RunGit("commit", "-qm", "initial");
    }

    public string RunGit(params string[] arguments)
    {
        var start_info = new ProcessStartInfo
        {
            FileName = "git",
            WorkingDirectory = Root,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        foreach (var argument in arguments)
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info) ?? throw new AssertFailedException("Unable to start Git.");
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new AssertFailedException($"Git {string.Join(' ', arguments)} failed: {error}");
        }

        return output;
    }

    public void Dispose()
    {
        foreach (var file_path in Directory.EnumerateFiles(Root, "*", SearchOption.AllDirectories))
        {
            File.SetAttributes(file_path, FileAttributes.Normal);
        }

        Directory.Delete(Root, recursive: true);
    }
}
