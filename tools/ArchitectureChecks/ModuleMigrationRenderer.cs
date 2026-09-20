namespace ArchitectureChecks;

internal static class ModuleMigrationRenderer
{
    public static void Render(ModuleMigrationResult result, TextWriter output)
    {
        ArgumentNullException.ThrowIfNull(result);
        ArgumentNullException.ThrowIfNull(output);

        WriteSection(output, "Moved reflected types and expected redirects");
        if (result.RenamedFileCount == 0)
        {
            output.WriteLine($"No renamed files found relative to {result.Baseline}. For a committed migration, pass its parent revision with --baseline.");
        }
        else if (result.MovedTypes.Count == 0)
        {
            output.WriteLine("No reflected types moved in renamed headers.");
        }
        else
        {
            foreach (var finding in result.MovedTypes)
            {
                var status = finding.HasRedirect ? "OK     " : "MISSING";
                output.WriteLine($"{status} {finding.Type.Kind}: {finding.OldObjectPath} -> {finding.NewObjectPath}");
            }
        }

        WriteSection(output, "Old-module references in config and assets");
        if (result.OldModuleReferences.Count == 0)
        {
            output.WriteLine("No readable old-module script references found.");
        }
        else
        {
            foreach (var finding in result.OldModuleReferences)
            {
                output.WriteLine($"STALE   {finding.Path}");
            }
        }

        WriteSection(output, "Stale includes that have a plugin header counterpart");
        if (result.StaleIncludes.Count == 0)
        {
            output.WriteLine("No stale old-module includes with a plugin header counterpart found.");
        }
        else
        {
            foreach (var finding in result.StaleIncludes)
            {
                var counterparts = string.Join(", ", finding.CounterpartModules.Select(module => $"{module}/{finding.IncludePath}"));
                output.WriteLine($"STALE   {finding.Path}:{finding.Line} -> {counterparts}");
            }
        }

        WriteSection(output, "Quoted relative includes in plugin implementation files");
        if (result.RelativeIncludes.Count == 0)
        {
            output.WriteLine("No quoted relative includes found in plugin .cpp files.");
        }
        else
        {
            foreach (var finding in result.RelativeIncludes)
            {
                output.WriteLine($"RELATIVE {finding.Path}:{finding.Line}: {finding.IncludeText}");
            }
        }

        output.WriteLine();
        output.WriteLine("Audit complete. Review findings before changing redirects or assets.");
    }

    private static void WriteSection(TextWriter output, string title)
    {
        output.WriteLine();
        output.WriteLine($"=== {title} ===");
    }
}
