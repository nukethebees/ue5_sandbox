namespace UnrealBuildTools;

public sealed class ToolInputException : Exception
{
    public ToolInputException(string message, Exception? inner_exception = null)
        : base(message, inner_exception)
    {
    }
}

public sealed class BuildLaunchException : Exception
{
    public BuildLaunchException(string message, Exception inner_exception)
        : base(message, inner_exception)
    {
    }
}

public sealed class BuildScriptFailedException : Exception
{
    public BuildScriptFailedException(int exit_code)
        : base($"Unreal build script exited with code {exit_code}.")
    {
        ExitCode = exit_code;
    }

    public int ExitCode { get; }
}

public sealed class PostBuildCompatibilityException : Exception
{
    public PostBuildCompatibilityException(string message)
        : base(message)
    {
    }
}
