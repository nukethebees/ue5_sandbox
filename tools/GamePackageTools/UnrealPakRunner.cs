namespace GamePackageTools;

internal interface IUnrealPakRunner
{
    Task<string> ListPakAsync(string pakPath, CancellationToken cancellationToken);

    Task ListIoStoreAsync(string pakDirectory, string csvPath, CancellationToken cancellationToken);
}

internal sealed class UnrealPakRunner(IProcessRunner processRunner, string unrealPakPath) : IUnrealPakRunner
{
    private static readonly TimeSpan timeout = TimeSpan.FromMinutes(5);

    public async Task<string> ListPakAsync(string pakPath, CancellationToken cancellationToken)
    {
        var result = await RunAsync([pakPath, "-List"], cancellationToken);
        if (result.ExitCode != 0)
        {
            throw new PackageVerificationException(
                $"UnrealPak failed to list '{pakPath}' with code {result.ExitCode}.{FormatDiagnostic(result.StandardError)}");
        }

        return CombineOutput(result);
    }

    public async Task ListIoStoreAsync(string pakDirectory, string csvPath, CancellationToken cancellationToken)
    {
        var utocPattern = Path.Combine(pakDirectory, "*.utoc");
        var result = await RunAsync([$"-ListContainer={utocPattern}", $"-Csv={csvPath}"], cancellationToken);
        if (result.ExitCode != 0)
        {
            throw new PackageVerificationException(
                $"UnrealPak failed to list the IoStore containers with code {result.ExitCode}.{FormatDiagnostic(result.StandardError)}");
        }
    }

    private async Task<ProcessResult> RunAsync(IReadOnlyList<string> arguments, CancellationToken cancellationToken)
    {
        try
        {
            return await processRunner.RunAsync(
                new ProcessRequest(unrealPakPath, arguments, Environment.CurrentDirectory, timeout),
                cancellationToken);
        }
        catch (ProcessLaunchException exception)
        {
            throw new PackageVerificationException($"Unable to start UnrealPak '{unrealPakPath}'.", exception);
        }
        catch (ProcessTimeoutException exception)
        {
            throw new PackageVerificationException($"UnrealPak timed out: {exception.Message}", exception);
        }
    }

    private static string CombineOutput(ProcessResult result)
    {
        return string.IsNullOrWhiteSpace(result.StandardError)
            ? result.StandardOutput
            : result.StandardOutput + Environment.NewLine + result.StandardError;
    }

    private static string FormatDiagnostic(string standardError)
    {
        return string.IsNullOrWhiteSpace(standardError) ? string.Empty : $" {standardError.Trim()}";
    }
}
