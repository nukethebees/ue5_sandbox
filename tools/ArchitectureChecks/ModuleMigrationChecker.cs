using System.Text;
using System.Text.RegularExpressions;

namespace ArchitectureChecks;

internal sealed class ModuleMigrationChecker
{
    private static readonly HashSet<string> transient_directories = new(StringComparer.OrdinalIgnoreCase)
    {
        "Binaries",
        "DerivedDataCache",
        "Intermediate",
    };

    private static readonly HashSet<string> header_extensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".h",
        ".hpp",
    };

    private static readonly HashSet<string> source_extensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".h",
        ".hpp",
        ".cpp",
    };

    private static readonly HashSet<string> implementation_extensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".cpp",
    };

    private static readonly Regex relative_include = new(
        "^\\s*#\\s*include\\s+\"(?<path>[^/\"]+)\"",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private readonly ModuleMigrationGit git;
    private readonly CoreRedirectParser redirect_parser;

    public ModuleMigrationChecker(IProcessRunner? process_runner = null, CoreRedirectParser? redirect_parser = null)
    {
        git = new ModuleMigrationGit(process_runner ?? new ProcessRunner());
        this.redirect_parser = redirect_parser ?? new CoreRedirectParser();
    }

    public async Task<ModuleMigrationResult> CheckAsync(
        ModuleMigrationRequest request,
        CancellationToken cancellation_token)
    {
        ArgumentNullException.ThrowIfNull(request);
        ArgumentException.ThrowIfNullOrWhiteSpace(request.RootPath);
        ArgumentException.ThrowIfNullOrWhiteSpace(request.Baseline);
        ArgumentException.ThrowIfNullOrWhiteSpace(request.OldModule);
        ArgumentNullException.ThrowIfNull(request.PluginModules);
        if (request.PluginModules.Count == 0)
        {
            throw new ArgumentException("At least one plugin module is required.", nameof(request));
        }

        var repository_root = await git.GetRepositoryRootAsync(request.RootPath, cancellation_token);
        var redirects = ReadRedirects(repository_root);
        var renamed_files = await git.GetRenamesAsync(repository_root, request, cancellation_token);
        var moved_types = await FindMovedTypesAsync(repository_root, request, redirects, renamed_files, cancellation_token);
        var old_module_references = FindOldModuleReferences(repository_root, request.OldModule);
        var stale_includes = FindStaleIncludes(repository_root, request.OldModule, request.PluginModules);
        var relative_includes = FindRelativeIncludes(repository_root, request.PluginModules);

        return new ModuleMigrationResult(
            repository_root,
            request.Baseline,
            renamed_files.Count,
            moved_types,
            old_module_references,
            stale_includes,
            relative_includes);
    }

    private IReadOnlySet<CoreRedirect> ReadRedirects(string repository_root)
    {
        var path = Path.Combine(repository_root, "Config", "DefaultEngine.ini");
        if (!File.Exists(path))
        {
            return new HashSet<CoreRedirect>();
        }

        return redirect_parser.Parse(File.ReadAllText(path, Encoding.UTF8));
    }

    private async Task<IReadOnlyList<MovedReflectedTypeFinding>> FindMovedTypesAsync(
        string repository_root,
        ModuleMigrationRequest request,
        IReadOnlySet<CoreRedirect> redirects,
        IReadOnlyList<RenamedFile> renamed_files,
        CancellationToken cancellation_token)
    {
        var findings = new List<MovedReflectedTypeFinding>();
        foreach (var renamed_file in renamed_files.Where(file => IsHeader(file.SourcePath)))
        {
            var source_text = await git.ReadBaselineFileAsync(
                repository_root,
                request.Baseline,
                renamed_file.SourcePath,
                cancellation_token);
            var destination_path = ToFullPath(repository_root, renamed_file.DestinationPath);
            if (!File.Exists(destination_path))
            {
                continue;
            }

            var destination_types = ReflectedTypeParser.Parse(File.ReadAllText(destination_path, Encoding.UTF8)).ToHashSet();
            foreach (var type in ReflectedTypeParser.Parse(source_text))
            {
                if (!destination_types.Contains(type))
                {
                    continue;
                }

                var old_object_path = $"/Script/{request.OldModule}.{type.ReflectionName}";
                var new_object_path = $"/Script/{renamed_file.DestinationModule}.{type.ReflectionName}";
                findings.Add(
                    new MovedReflectedTypeFinding(
                        type,
                        renamed_file.SourcePath,
                        renamed_file.DestinationPath,
                        renamed_file.DestinationModule,
                        old_object_path,
                        new_object_path,
                        redirects.Contains(new CoreRedirect(type.Kind, old_object_path, new_object_path))));
            }
        }

        return findings
            .OrderBy(finding => finding.Type.Kind)
            .ThenBy(finding => finding.OldObjectPath, StringComparer.Ordinal)
            .ToArray();
    }

    private static IReadOnlyList<OldModuleReferenceFinding> FindOldModuleReferences(string repository_root, string old_module)
    {
        var needle = Encoding.ASCII.GetBytes($"/Script/{old_module}.");
        var findings = new List<OldModuleReferenceFinding>();
        foreach (var directory in new[] { "Config", "Content", "Plugins" })
        {
            foreach (var path in EnumerateFiles(Path.Combine(repository_root, directory)))
            {
                if (ContainsBytes(path, needle))
                {
                    findings.Add(new OldModuleReferenceFinding(ToRepositoryPath(repository_root, path)));
                }
            }
        }

        return findings.OrderBy(finding => finding.Path, StringComparer.Ordinal).ToArray();
    }

    private static IReadOnlyList<StaleIncludeFinding> FindStaleIncludes(
        string repository_root,
        string old_module,
        IReadOnlyList<string> plugin_modules)
    {
        var headers = FindPluginHeaders(repository_root, plugin_modules);
        if (headers.Count == 0)
        {
            return [];
        }

        var old_include = new Regex(
            $"^\\s*#\\s*include\\s*[<\"]{Regex.Escape(old_module)}/(?<path>[^>\"]+)[>\"]",
            RegexOptions.Compiled | RegexOptions.CultureInvariant);
        var findings = new List<StaleIncludeFinding>();
        foreach (var directory in new[] { "Source", "Plugins" })
        {
            foreach (var path in EnumerateFiles(Path.Combine(repository_root, directory), source_extensions))
            {
                AddStaleIncludes(repository_root, path, old_include, headers, findings);
            }
        }

        return findings
            .OrderBy(finding => finding.Path, StringComparer.Ordinal)
            .ThenBy(finding => finding.Line)
            .ToArray();
    }

    private static IReadOnlyList<RelativeIncludeFinding> FindRelativeIncludes(
        string repository_root,
        IReadOnlyList<string> plugin_modules)
    {
        var findings = new List<RelativeIncludeFinding>();
        foreach (var module in plugin_modules)
        {
            var private_root = Path.Combine(repository_root, "Plugins", module, "Source", module, "Private");
            foreach (var path in EnumerateFiles(private_root, implementation_extensions))
            {
                var lines = File.ReadAllLines(path, Encoding.UTF8);
                for (var index = 0; index < lines.Length; ++index)
                {
                    var match = relative_include.Match(lines[index]);
                    if (match.Success)
                    {
                        findings.Add(new RelativeIncludeFinding(
                            ToRepositoryPath(repository_root, path),
                            index + 1,
                            match.Groups["path"].Value));
                    }
                }
            }
        }

        return findings
            .OrderBy(finding => finding.Path, StringComparer.Ordinal)
            .ThenBy(finding => finding.Line)
            .ToArray();
    }

    private static Dictionary<string, IReadOnlyList<string>> FindPluginHeaders(
        string repository_root,
        IReadOnlyList<string> plugin_modules)
    {
        var headers = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        foreach (var module in plugin_modules)
        {
            var public_root = Path.Combine(repository_root, "Plugins", module, "Source", module, "Public", module);
            foreach (var path in EnumerateFiles(public_root, header_extensions))
            {
                var include_path = ToRepositoryPath(public_root, path);
                if (!headers.TryGetValue(include_path, out var modules))
                {
                    modules = [];
                    headers.Add(include_path, modules);
                }

                modules.Add(module);
            }
        }

        return headers.ToDictionary(
            pair => pair.Key,
            pair => (IReadOnlyList<string>)pair.Value.OrderBy(module => module, StringComparer.Ordinal).ToArray(),
            StringComparer.Ordinal);
    }

    private static void AddStaleIncludes(
        string repository_root,
        string path,
        Regex old_include,
        IReadOnlyDictionary<string, IReadOnlyList<string>> headers,
        ICollection<StaleIncludeFinding> findings)
    {
        var lines = File.ReadAllLines(path, Encoding.UTF8);
        for (var index = 0; index < lines.Length; ++index)
        {
            var match = old_include.Match(lines[index]);
            if (match.Success && headers.TryGetValue(match.Groups["path"].Value, out var modules))
            {
                findings.Add(new StaleIncludeFinding(
                    ToRepositoryPath(repository_root, path),
                    index + 1,
                    match.Groups["path"].Value,
                    modules));
            }
        }
    }

    private static IEnumerable<string> EnumerateFiles(string directory, IReadOnlySet<string>? extensions = null)
    {
        if (!Directory.Exists(directory))
        {
            return [];
        }

        var files = new List<string>();
        var pending = new Stack<string>();
        pending.Push(directory);
        while (pending.TryPop(out var current))
        {
            try
            {
                foreach (var child_directory in Directory.EnumerateDirectories(current))
                {
                    var attributes = File.GetAttributes(child_directory);
                    if (!transient_directories.Contains(Path.GetFileName(child_directory)) && !attributes.HasFlag(FileAttributes.ReparsePoint))
                    {
                        pending.Push(child_directory);
                    }
                }

                foreach (var path in Directory.EnumerateFiles(current))
                {
                    if (extensions is null || extensions.Contains(Path.GetExtension(path)))
                    {
                        files.Add(path);
                    }
                }
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                continue;
            }
        }

        return files.OrderBy(path => path, StringComparer.Ordinal).ToArray();
    }

    private static bool ContainsBytes(string path, ReadOnlySpan<byte> needle)
    {
        try
        {
            using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            var overlap = Math.Max(needle.Length - 1, 0);
            var buffer = new byte[8192 + overlap];
            var prefix_length = 0;
            while (true)
            {
                var count = stream.Read(buffer, prefix_length, buffer.Length - prefix_length);
                if (count == 0)
                {
                    return false;
                }

                var contents = buffer.AsSpan(0, prefix_length + count);
                if (contents.IndexOf(needle) >= 0)
                {
                    return true;
                }

                prefix_length = Math.Min(overlap, contents.Length);
                contents[^prefix_length..].CopyTo(buffer);
            }
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            return false;
        }
    }

    private static bool IsHeader(string path)
    {
        return header_extensions.Contains(Path.GetExtension(path));
    }

    internal static string ToFullPath(string repository_root, string repository_path)
    {
        if (Path.IsPathRooted(repository_path))
        {
            throw new ModuleMigrationException($"Git reported a path outside the repository: '{repository_path}'.");
        }

        var path = Path.GetFullPath(Path.Combine(repository_root, repository_path.Replace('/', Path.DirectorySeparatorChar)));
        var relative = Path.GetRelativePath(repository_root, path);
        if (IsParentEscape(relative))
        {
            throw new ModuleMigrationException($"Git reported a path outside the repository: '{repository_path}'.");
        }

        return path;
    }

    private static bool IsParentEscape(string path)
    {
        return path == ".." ||
               path.StartsWith($"..{Path.DirectorySeparatorChar}", StringComparison.Ordinal) ||
               Path.AltDirectorySeparatorChar != Path.DirectorySeparatorChar &&
               path.StartsWith($"..{Path.AltDirectorySeparatorChar}", StringComparison.Ordinal);
    }

    private static string ToRepositoryPath(string root, string path)
    {
        return Path.GetRelativePath(root, path).Replace(Path.DirectorySeparatorChar, '/');
    }
}
