using System.ComponentModel;
using System.Diagnostics;

namespace UnrealBuildTools;

public sealed class BuildScriptRunner
{
    public void Run(BuildPaths paths, BuildRequest request, bool force_rebuild)
    {
        ArgumentNullException.ThrowIfNull(paths);
        ArgumentNullException.ThrowIfNull(request);

        var build_arguments = new List<string>
        {
            request.Target,
            request.Platform,
            request.Configuration,
            $"-Project={paths.ProjectPath}",
            "-WaitMutex",
        };
        if (force_rebuild)
        {
            build_arguments.Add("-Force");
        }

        var start_info = CreateStartInfo(paths.BuildScriptPath, build_arguments, request.NativeToolchain);
        using var process = new Process { StartInfo = start_info };
        try
        {
            process.Start();
        }
        catch (Exception exception) when (exception is InvalidOperationException or Win32Exception)
        {
            throw new BuildLaunchException($"Unable to start Unreal build script '{paths.BuildScriptPath}'.", exception);
        }

        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new BuildScriptFailedException(process.ExitCode);
        }
    }

    public static ProcessStartInfo CreateStartInfo(
        string build_script_path,
        IReadOnlyList<string> build_arguments,
        string native_toolchain)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(build_script_path);
        ArgumentNullException.ThrowIfNull(build_arguments);
        ArgumentException.ThrowIfNullOrWhiteSpace(native_toolchain);

        var extension = Path.GetExtension(build_script_path);
        var start_info = new ProcessStartInfo
        {
            UseShellExecute = false,
        };
        if (extension.Equals(".bat", StringComparison.OrdinalIgnoreCase) ||
            extension.Equals(".cmd", StringComparison.OrdinalIgnoreCase))
        {
            var command_processor = Environment.GetEnvironmentVariable("ComSpec");
            start_info.FileName = string.IsNullOrWhiteSpace(command_processor) ? "cmd.exe" : command_processor;
            start_info.ArgumentList.Add("/d");
            start_info.ArgumentList.Add("/c");
            start_info.ArgumentList.Add("call");
            start_info.ArgumentList.Add(build_script_path);
            foreach (var argument in build_arguments)
            {
                start_info.ArgumentList.Add(argument);
            }
        }
        else
        {
            start_info.FileName = build_script_path;
            foreach (var argument in build_arguments)
            {
                start_info.ArgumentList.Add(argument);
            }
        }

        start_info.Environment["SANDBOX_NATIVE_TOOLCHAIN"] = native_toolchain;
        return start_info;
    }

}
