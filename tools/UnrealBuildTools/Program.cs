namespace UnrealBuildTools;

public static class Program
{
    public static int Main(string[] arguments)
    {
        if (!TryParse(arguments, out var request))
        {
            Console.Error.WriteLine("Usage: UnrealBuildTools --build-script <path> --target <target> --platform <platform> --configuration <configuration> --project <path> --native-toolchain <name>");
            return 2;
        }

        try
        {
            new UnrealBuildOrchestrator().Build(request);

            return 0;
        }
        catch (ToolInputException exception)
        {
            return WriteFailure(3, exception);
        }
        catch (BuildLaunchException exception)
        {
            return WriteFailure(4, exception);
        }
        catch (BuildScriptFailedException exception)
        {
            return WriteFailure(exception.ExitCode, exception);
        }
    }

    private static bool TryParse(string[] arguments, out BuildRequest request)
    {
        request = null!;
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        for (var index = 0; index < arguments.Length; ++index)
        {
            var argument = arguments[index];
            if (argument is not ("--build-script" or "--target" or "--platform" or "--configuration" or "--project" or "--native-toolchain") ||
                index + 1 >= arguments.Length ||
                string.IsNullOrWhiteSpace(arguments[index + 1]) ||
                !values.TryAdd(argument, arguments[++index]))
            {
                return false;
            }
        }

        if (!values.TryGetValue("--build-script", out var build_script) ||
            !values.TryGetValue("--target", out var target) ||
            !values.TryGetValue("--platform", out var platform) ||
            !values.TryGetValue("--configuration", out var configuration) ||
            !values.TryGetValue("--project", out var project) ||
            !values.TryGetValue("--native-toolchain", out var native_toolchain))
        {
            return false;
        }

        request = new BuildRequest(
            build_script,
            target,
            platform,
            configuration,
            project,
            native_toolchain);
        return true;
    }

    private static int WriteFailure(int exit_code, Exception exception)
    {
        Console.Error.WriteLine($"UnrealBuildTools: {exception.Message}");
        return exit_code;
    }
}
