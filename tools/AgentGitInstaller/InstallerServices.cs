using System.Security.Cryptography;

namespace AgentGitInstaller;

internal interface IExecutableLocator
{
    string Find(string executable_name);
}

internal sealed class ExecutableLocator : IExecutableLocator
{
    public string Find(string executable_name)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(executable_name);

        var path = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        foreach (var directory in path.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            var candidate = Path.Combine(directory.Trim('"'), executable_name);
            if (File.Exists(candidate))
            {
                var full_path = Path.GetFullPath(candidate);
                if ((File.GetAttributes(full_path) & FileAttributes.ReparsePoint) != 0)
                {
                    throw new InstallerException(
                        $"Trusted executable cannot be a symbolic link or reparse point: '{full_path}'.");
                }

                return full_path;
            }
        }

        throw new InstallerException($"Unable to locate required executable '{executable_name}' on PATH.");
    }
}

internal interface IArtifactHasher
{
    string Hash(string path);
}

internal sealed class Sha256ArtifactHasher : IArtifactHasher
{
    public string Hash(string path)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        return Convert.ToHexString(SHA256.HashData(stream));
    }
}

internal interface IInstallerFileSystem
{
    bool FileExists(string path);
    bool DirectoryExists(string path);
    FileAttributes GetAttributes(string path);
    long FileLength(string path);
    string ReadAllText(string path);
    void WriteAllText(string path, string contents);
    void AppendAllText(string path, string contents);
    void CreateDirectory(string path);
    void CopyFile(string source, string destination);
    void MoveDirectory(string source, string destination);
    void DeleteDirectory(string path);
}

internal sealed class InstallerFileSystem : IInstallerFileSystem
{
    public bool FileExists(string path) => File.Exists(path);

    public bool DirectoryExists(string path) => Directory.Exists(path);

    public FileAttributes GetAttributes(string path) => File.GetAttributes(path);

    public long FileLength(string path) => new FileInfo(path).Length;

    public string ReadAllText(string path) => File.ReadAllText(path);

    public void WriteAllText(string path, string contents) => File.WriteAllText(path, contents);

    public void AppendAllText(string path, string contents) => File.AppendAllText(path, contents);

    public void CreateDirectory(string path) => Directory.CreateDirectory(path);

    public void CopyFile(string source, string destination) => File.Copy(source, destination, overwrite: false);

    public void MoveDirectory(string source, string destination) => Directory.Move(source, destination);

    public void DeleteDirectory(string path) => Directory.Delete(path, recursive: true);
}
