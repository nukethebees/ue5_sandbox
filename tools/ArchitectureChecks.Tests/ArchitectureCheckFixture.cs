namespace ArchitectureChecks.Tests;

internal sealed class ArchitectureCheckFixture : IDisposable
{
    public ArchitectureCheckFixture()
    {
        Root = Directory.CreateTempSubdirectory("Architecture Checks ").FullName;
    }

    public string Root { get; }

    public void WriteBuildCs(string module_name, string contents, string? parent = null, bool bom = false)
    {
        WriteFile(
            Path.Combine(parent ?? Path.Combine("Plugins", "Fixture", "Source", module_name), $"{module_name}.Build.cs"),
            contents,
            bom);
    }

    public void WriteSimulationSource(string relative_path, string contents)
    {
        WriteFile(Path.Combine("Plugins", "SpaceGame", "Source", "SpaceGameSimulation", relative_path), contents);
    }

    public void WritePresentationSource(string relative_path, string contents)
    {
        WriteFile(Path.Combine("Plugins", "SpaceGame", "Source", "SpaceGamePresentation", relative_path), contents);
    }

    public void WriteValidComposition()
    {
        WriteBuildCs("SpaceGameSimulation", "PublicDependencyModuleNames.Add(\"Core\");");
        WriteBuildCs("SpaceGamePresentation", "PrivateDependencyModuleNames.Add(\"SpaceGameSimulation\");");
        WriteBuildCs("SpaceGame", "PublicDependencyModuleNames.AddRange(new string[] { \"SpaceGameSimulation\", \"SpaceGamePresentation\" });");
    }

    public void Dispose()
    {
        Directory.Delete(Root, recursive: true);
    }

    private void WriteFile(string relative_path, string contents, bool bom = false)
    {
        var path = Path.Combine(Root, relative_path);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, contents, new System.Text.UTF8Encoding(bom));
    }
}
