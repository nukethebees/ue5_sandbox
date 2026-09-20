namespace ArchitectureChecks;

internal sealed class ModuleMigrationException(string message, Exception? inner_exception = null) : Exception(message, inner_exception);
