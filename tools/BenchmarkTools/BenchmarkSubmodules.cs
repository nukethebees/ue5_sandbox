using System.Text.Json;

namespace BenchmarkTools;

internal static class BenchmarkSubmodules
{
    public static async Task InitializeAsync(BenchmarkToolsApplication application, string target, string? source,
        IReadOnlyList<string>? paths, CancellationToken token)
    {
        var init = new List<string> { "submodule", "init" };
        if (paths is not null) { init.Add("--"); init.AddRange(paths); }
        await BenchmarkGit.TextAsync(application, target, init, token);
        if (!File.Exists(Path.Combine(target, ".gitmodules"))) return;

        var modules = await application.ProcessRunner.RunAsync(BenchmarkGit.Request(target,
            ["config", "--null", "--file", ".gitmodules", "--get-regexp", @"^submodule\..*\.path$"]), token);
        if (modules.ExitCode == 1) return; // A historical .gitmodules may contain no remaining modules.
        if (modules.ExitCode != 0) throw new BenchmarkToolException($"Could not read submodule paths: {modules.StandardError.Trim()}");
        foreach (var entry in modules.StandardOutput.Split('\0', StringSplitOptions.RemoveEmptyEntries))
        {
            var separator = entry.IndexOf('\n');
            if (separator < 0) throw new BenchmarkToolException("Malformed submodule path configuration.");
            var key = entry[..separator];
            var relative = entry[(separator + 1)..];
            if (paths is not null && !paths.Contains(relative)) continue;
            var destination = Path.GetFullPath(relative, target);
            RevisionComparisonSession.ValidateOwnedPath(destination, target);
            var local = source is null ? null : Path.GetFullPath(relative, source);
            if (local is not null)
            {
                RevisionComparisonSession.ValidateOwnedPath(local, source!);
                if (!File.Exists(Path.Combine(local, ".git")) && !Directory.Exists(Path.Combine(local, ".git"))) local = null;
            }

            if (local is not null)
            {
                var top = await BenchmarkGit.TextAsync(application, local, ["rev-parse", "--show-toplevel"], token);
                if (!string.Equals(Path.GetFullPath(top), local, StringComparison.OrdinalIgnoreCase))
                    throw new BenchmarkToolException($"Local submodule is not a repository root: '{local}'.");
                var url = await BenchmarkGit.TextAsync(application, target, ["config", "--get", key[..^4] + "url"], token);
                var tree = await BenchmarkGit.OutputAsync(application, target, ["ls-tree", "-z", "HEAD", "--", relative], token);
                var fields = tree.Split([' ', '\t'], 4);
                if (fields.Length != 4 || fields[0] != "160000" || fields[1] != "commit")
                    throw new BenchmarkToolException($"Missing baseline gitlink for '{relative}'.");
                var commit = fields[2];
                application.StandardOutput.WriteLine($"Seeding submodule {relative} from {local}");
                // Independent objects and checkout: candidate edits, refs, and later GC cannot affect this input.
                await BenchmarkGit.TextAsync(application, target, ["clone", "--no-checkout", "--no-hardlinks", "--", local, destination], token);
                await BenchmarkGit.TextAsync(application, destination, ["remote", "set-url", "origin", url], token);
                await BenchmarkGit.TextAsync(application, destination, ["config", "--local", "lfs.storage", Path.Combine(destination, ".git", "lfs")], token);
                var available = await application.ProcessRunner.RunAsync(BenchmarkGit.Request(destination, ["cat-file", "-e", commit + "^{commit}"]), token);
                if (available.ExitCode != 0)
                    await BenchmarkGit.TextAsync(application, destination, ["fetch", "--no-tags", "origin", commit], token);
                await CopyLfsObjectsAsync(application, local, destination, commit, token);
                await BenchmarkGit.TextAsync(application, destination, ["checkout", "--detach", commit], token);
            }
            else
            {
                await BenchmarkGit.TextAsync(application, target, ["submodule", "update", "--init", "--checkout", "--", relative], token);
            }
            await InitializeAsync(application, destination, local, null, token);
        }
    }

    private static async Task CopyLfsObjectsAsync(BenchmarkToolsApplication application, string source, string target, string commit, CancellationToken token)
    {
        var listing = await BenchmarkGit.TextAsync(application, target, ["lfs", "ls-files", "--json", commit], token);
        using var document = JsonDocument.Parse(listing);
        var files = document.RootElement.GetProperty("files");
        if (files.GetArrayLength() == 0) return;
        var source_cache = await MediaDirectory(source);
        var target_cache = await MediaDirectory(target);
        foreach (var file in files.EnumerateArray())
        {
            token.ThrowIfCancellationRequested();
            var oid = file.GetProperty("oid").GetString() ?? string.Empty;
            if (oid.Length != 64 || !oid.All(char.IsAsciiHexDigit)) throw new BenchmarkToolException("Invalid LFS object identity.");
            var relative = Path.Combine(oid[..2], oid[2..4], oid);
            var cached = Path.Combine(source_cache, relative);
            if (!File.Exists(cached)) continue; // Checkout fetches only objects absent from the seeded cache.
            var destination = Path.Combine(target_cache, relative);
            if (File.Exists(destination)) continue;
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            File.Copy(cached, destination);
        }

        async Task<string> MediaDirectory(string root)
        {
            var environment = await BenchmarkGit.TextAsync(application, root, ["lfs", "env"], token);
            var media = environment.Split('\n').SingleOrDefault(line => line.StartsWith("LocalMediaDir=", StringComparison.Ordinal));
            if (media is null) throw new BenchmarkToolException($"Could not locate LFS cache for '{root}'.");
            return Path.GetFullPath(media["LocalMediaDir=".Length..].TrimEnd('\r'), root);
        }
    }
}
