using System.Diagnostics;
using System.Text;

namespace ArchitectureChecks.Tests;

internal sealed class ModuleMigrationFixture : IDisposable
{
    public ModuleMigrationFixture()
    {
        Root = Directory.CreateTempSubdirectory("Architecture migration ").FullName;
        RunGit("init");
        RunGit("config", "user.email", "architecture-checks@example.test");
        RunGit("config", "user.name", "Architecture Checks");
    }

    public string Root { get; }

    public void WriteFile(string relative_path, string contents)
    {
        var path = Path.Combine(Root, relative_path);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, contents, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
    }

    public void WriteBytes(string relative_path, byte[] contents)
    {
        var path = Path.Combine(Root, relative_path);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllBytes(path, contents);
    }

    public void Move(string source_relative_path, string destination_relative_path)
    {
        var source = Path.Combine(Root, source_relative_path);
        var destination = Path.Combine(Root, destination_relative_path);
        Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
        File.Move(source, destination);
    }

    public string Commit(string message)
    {
        RunGit("add", "--all");
        RunGit("commit", "--quiet", "-m", message);
        return RunGit("rev-parse", "HEAD").Trim();
    }

    public void Dispose()
    {
        foreach (var path in Directory.EnumerateFiles(Root, "*", SearchOption.AllDirectories))
        {
            File.SetAttributes(path, FileAttributes.Normal);
        }

        foreach (var path in Directory.EnumerateDirectories(Root, "*", SearchOption.AllDirectories)
            .OrderByDescending(path => path.Length))
        {
            File.SetAttributes(path, FileAttributes.Normal);
        }

        Directory.Delete(Root, recursive: true);
    }

    private string RunGit(params string[] arguments)
    {
        var start_info = new ProcessStartInfo
        {
            FileName = "git",
            WorkingDirectory = Root,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        foreach (var argument in arguments)
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info)!;
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new InvalidOperationException($"git {arguments[0]} failed: {error}");
        }

        return output;
    }
}
