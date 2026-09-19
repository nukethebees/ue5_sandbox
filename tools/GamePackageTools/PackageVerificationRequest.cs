namespace GamePackageTools;

public enum PackageConfiguration
{
    Development,
    Shipping,
}

public sealed record PackageVerificationRequest(
    string ProjectRoot,
    string PackageRoot,
    string UnrealPakPath,
    string VerificationDirectory,
    PackageConfiguration Configuration);
