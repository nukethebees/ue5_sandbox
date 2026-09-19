namespace GamePackageTools.Tests;

internal sealed class PackageToolFixture : IDisposable
{
    private readonly string root;

    public PackageToolFixture()
    {
        root = Directory.CreateTempSubdirectory("Sandbox GamePackageTools ").FullName;
        ProjectRoot = Path.Combine(root, "project root");
        PackageRoot = Path.Combine(root, "package root");
        UnrealPakPath = Path.Combine(root, "tools with spaces", "UnrealPak.exe");
        VerificationDirectory = Path.Combine(root, "verification artifacts");

        WriteFile(UnrealPakPath);
        WriteFile(Path.Combine(PackageRoot, "Sandbox.exe"));
        WriteFile(Path.Combine(PackageRoot, "Sandbox", "Binaries", "Win64", "Sandbox.exe"));
        WriteFile(Path.Combine(PackageRoot, "Sandbox", "Binaries", "Win64", "Sandbox-Win64-Shipping.exe"));
        WriteFile(Path.Combine(PackageRoot, "Engine", "Extras", "Redist", "en-us", "vc_redist.x64.exe"));
        WriteFile(Path.Combine(PakDirectory, "Sandbox-Windows.pak"));
        WriteFile(Path.Combine(PakDirectory, "Sandbox-Windows.utoc"));

        WriteProjectFile("LevelScripts/Nested/Scenario.scm");
        WriteProjectFile("Content/UI/DA_ui_data.uasset");
        WriteProjectFile("Plugins/SpaceGame/SpaceGame.uplugin", "{}");
        WriteProjectFile("Plugins/SpaceGame/Content/Levels/MainMenu.umap");
        WriteProjectFile("Plugins/SpaceGame/Content/Levels/GameRuntime.umap");
        WriteProjectFile("Plugins/SpaceGame/Content/UI/RequiredUi.uasset");
        WriteProjectFile("Plugins/SpaceGame/Content/Input/RequiredInput.uasset");
        WriteProjectFile("Plugins/SandboxShaders/SandboxShaders.uplugin", "{}");
        WriteProjectFile("Plugins/SandboxShaders/Content/GpuStarfield/RequiredStarfield.uasset");
        WriteProjectFile("Plugins/SandboxShaders/Content/CelestialBackdrop/RequiredBackdrop.uasset");
    }

    public string ProjectRoot { get; }

    public string PackageRoot { get; }

    public string UnrealPakPath { get; }

    public string VerificationDirectory { get; }

    public string PakDirectory => Path.Combine(PackageRoot, "Sandbox", "Content", "Paks");

    public PackageVerificationRequest CreateRequest(PackageConfiguration configuration = PackageConfiguration.Development)
    {
        return new PackageVerificationRequest(ProjectRoot, PackageRoot, UnrealPakPath, VerificationDirectory, configuration);
    }

    public string WriteProjectFile(string relativePath, string contents = "")
    {
        var path = Path.Combine(ProjectRoot, relativePath);
        WriteFile(path, contents);
        return path;
    }

    public FakeUnrealPakRunner CreateRunner()
    {
        return new FakeUnrealPakRunner { IoStoreContents = BuildInventory() };
    }

    public string BuildInventory()
    {
        return string.Join('\n',
        [
            "LevelScripts/Nested/Scenario.scm",
            "/SpaceGame/Levels/MainMenu",
            "/SpaceGame/Levels/GameRuntime",
            "/Game/UI/DA_ui_data",
            "/SpaceGame/UI/RequiredUi",
            "/SpaceGame/Input/RequiredInput",
            "/SandboxShaders/GpuStarfield/RequiredStarfield",
            "/SandboxShaders/CelestialBackdrop/RequiredBackdrop",
        ]);
    }

    public void Dispose()
    {
        Directory.Delete(root, recursive: true);
    }

    private static void WriteFile(string path, string contents = "")
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, contents);
    }
}

internal sealed class FakeUnrealPakRunner : IUnrealPakRunner
{
    public List<string> PakFiles { get; } = [];

    public string PakListing { get; set; } = "pak inventory";

    public string IoStoreContents { get; set; } = string.Empty;

    public bool WriteIoStoreCsv { get; set; } = true;

    public Exception? ListPakException { get; set; }

    public Task<string> ListPakAsync(string pakPath, CancellationToken cancellationToken)
    {
        PakFiles.Add(pakPath);
        if (ListPakException is not null)
        {
            throw ListPakException;
        }

        return Task.FromResult(PakListing);
    }

    public Task ListIoStoreAsync(string pakDirectory, string csvPath, CancellationToken cancellationToken)
    {
        if (WriteIoStoreCsv)
        {
            File.WriteAllText(csvPath, IoStoreContents);
        }

        return Task.CompletedTask;
    }
}
