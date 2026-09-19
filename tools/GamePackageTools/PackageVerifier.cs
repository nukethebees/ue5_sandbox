using System.Text;

namespace GamePackageTools;

public sealed class PackageVerifier
{
    private static readonly string[] requiredMapPaths =
    [
        "/SpaceGame/Levels/MainMenu",
        "/SpaceGame/Levels/GameRuntime",
    ];

    private static readonly string[] requiredAssetDirectories =
    [
        "Plugins/SpaceGame/Content/UI",
        "Plugins/SpaceGame/Content/Input",
        "Plugins/SandboxShaders/Content/GpuStarfield",
        "Plugins/SandboxShaders/Content/CelestialBackdrop",
    ];

    private readonly IUnrealPakRunner unrealPakRunner;
    private readonly TextWriter standardOutput;

    internal PackageVerifier(IUnrealPakRunner unrealPakRunner, TextWriter standardOutput)
    {
        this.unrealPakRunner = unrealPakRunner;
        this.standardOutput = standardOutput;
    }

    public PackageVerifier(PackageVerificationRequest request, TextWriter standardOutput)
        : this(new UnrealPakRunner(new ProcessRunner(), request.UnrealPakPath), standardOutput)
    {
    }

    public async Task VerifyAsync(PackageVerificationRequest request, CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(request);

        var projectRoot = Path.GetFullPath(request.ProjectRoot);
        var packageRoot = Path.GetFullPath(request.PackageRoot);
        var verificationDirectory = Path.GetFullPath(request.VerificationDirectory);
        var paths = new PackagePathConverter(projectRoot);

        AssertFileExists(Path.Combine(packageRoot, "Sandbox.exe"));
        AssertFileExists(Path.Combine(packageRoot, "Sandbox", "Binaries", "Win64", GetGameBinaryName(request.Configuration)));
        AssertFileExists(Path.Combine(packageRoot, "Engine", "Extras", "Redist", "en-us", "vc_redist.x64.exe"));
        AssertFileExists(request.UnrealPakPath);

        var pakDirectory = Path.Combine(packageRoot, "Sandbox", "Content", "Paks");
        var pakFiles = Directory.EnumerateFiles(pakDirectory, "*.pak", SearchOption.TopDirectoryOnly).ToArray();
        var utocFiles = Directory.EnumerateFiles(pakDirectory, "*.utoc", SearchOption.TopDirectoryOnly).ToArray();
        if (pakFiles.Length == 0)
        {
            throw new PackageVerificationException($"No pak files were found under '{pakDirectory}'.");
        }
        if (utocFiles.Length == 0)
        {
            throw new PackageVerificationException($"No IoStore containers were found under '{pakDirectory}'.");
        }

        Directory.CreateDirectory(verificationDirectory);
        var pakInventoryPath = Path.Combine(verificationDirectory, "pak-files.txt");
        var ioStoreInventoryPath = Path.Combine(verificationDirectory, "iostore.csv");
        if (File.Exists(ioStoreInventoryPath))
        {
            File.Delete(ioStoreInventoryPath);
        }

        var pakListings = new List<string>(pakFiles.Length);
        foreach (var pakFile in pakFiles)
        {
            pakListings.Add(await unrealPakRunner.ListPakAsync(pakFile, cancellationToken));
        }
        await File.WriteAllLinesAsync(pakInventoryPath, pakListings, new UTF8Encoding(false), cancellationToken);

        await unrealPakRunner.ListIoStoreAsync(pakDirectory, ioStoreInventoryPath, cancellationToken);
        AssertFileExists(ioStoreInventoryPath);

        var inventory = new PackageInventory(
            string.Join(Environment.NewLine, pakListings),
            await File.ReadAllTextAsync(ioStoreInventoryPath, cancellationToken));

        var levelScripts = VerifyLevelScripts(projectRoot, inventory);
        foreach (var requiredMapPath in requiredMapPaths)
        {
            AssertPackagePresent(requiredMapPath, inventory);
        }
        AssertPackagePresent("/Game/UI/DA_ui_data", inventory);

        VerifyUnexpectedMaps(projectRoot, paths, inventory);
        VerifyRequiredAssetDirectories(projectRoot, paths, inventory);
        VerifyGeneratedAudio(projectRoot, paths, inventory);

        standardOutput.WriteLine($"Verified {levelScripts} level scripts, two maps, runtime-loaded assets, binaries, containers, and prerequisites.");
    }

    public static string GetGameBinaryName(PackageConfiguration configuration)
    {
        return configuration == PackageConfiguration.Development
            ? "Sandbox.exe"
            : $"Sandbox-Win64-{configuration}.exe";
    }

    private static void AssertFileExists(string path)
    {
        if (!File.Exists(path))
        {
            throw new PackageVerificationException($"Required package file is missing: {path}");
        }
    }

    private static int VerifyLevelScripts(string projectRoot, PackageInventory inventory)
    {
        var levelScriptsDirectory = Path.Combine(projectRoot, "LevelScripts");
        var levelScripts = Directory.EnumerateFiles(levelScriptsDirectory, "*.scm", SearchOption.AllDirectories).ToArray();
        foreach (var levelScript in levelScripts)
        {
            var relativePath = Path.GetRelativePath(levelScriptsDirectory, levelScript).Replace('\\', '/');
            if (!inventory.Contains($"LevelScripts/{relativePath}"))
            {
                throw new PackageVerificationException($"Required level script is missing from the pak: {relativePath}");
            }
        }

        return levelScripts.Length;
    }

    private static void VerifyUnexpectedMaps(string projectRoot, PackagePathConverter paths, PackageInventory inventory)
    {
        var mapFiles = Directory.EnumerateFiles(Path.Combine(projectRoot, "Content"), "*.umap", SearchOption.AllDirectories)
            .Concat(Directory.EnumerateFiles(Path.Combine(projectRoot, "Plugins"), "*.umap", SearchOption.AllDirectories));
        foreach (var mapFile in mapFiles)
        {
            var packagePath = paths.ToUnrealPackagePath(mapFile);
            if (requiredMapPaths.Contains(packagePath, StringComparer.Ordinal))
            {
                continue;
            }

            if (inventory.Contains(packagePath) || inventory.Contains(PackagePathConverter.ToStagedAssetPath(packagePath)))
            {
                throw new PackageVerificationException($"Unexpected project map was packaged: {packagePath}");
            }
        }
    }

    private static void VerifyRequiredAssetDirectories(string projectRoot, PackagePathConverter paths, PackageInventory inventory)
    {
        foreach (var relativeDirectory in requiredAssetDirectories)
        {
            VerifyAssetsInDirectory(Path.Combine(projectRoot, relativeDirectory), paths, inventory);
        }
    }

    private static void VerifyGeneratedAudio(string projectRoot, PackagePathConverter paths, PackageInventory inventory)
    {
        var audioDirectory = Path.Combine(projectRoot, "Plugins", "SpaceGame", "Content", "Audio", "Generated");
        if (Directory.Exists(audioDirectory))
        {
            VerifyAssetsInDirectory(audioDirectory, paths, inventory);
        }
    }

    private static void VerifyAssetsInDirectory(string directory, PackagePathConverter paths, PackageInventory inventory)
    {
        foreach (var asset in Directory.EnumerateFiles(directory, "*.uasset", SearchOption.AllDirectories))
        {
            AssertPackagePresent(paths.ToUnrealPackagePath(asset), inventory);
        }
    }

    private static void AssertPackagePresent(string packagePath, PackageInventory inventory)
    {
        var stagedPath = PackagePathConverter.ToStagedAssetPath(packagePath);
        if (!inventory.Contains(packagePath) && !inventory.Contains(stagedPath))
        {
            throw new PackageVerificationException($"Required Unreal package is missing from the containers: {packagePath}");
        }
    }
}
