namespace GamePackageTools;

public sealed class PackageVerificationException(string message, Exception? innerException = null)
    : Exception(message, innerException);
