namespace CodeFormatTools;

public static class Program
{
    public static async Task<int> Main(string[] arguments)
    {
        var process_runner = new ProcessRunner();
        var llvm_root = Environment.GetEnvironmentVariable("LLVM_ROOT");
        var clang_format = string.IsNullOrWhiteSpace(llvm_root)
            ? "clang-format"
            : Path.Combine(llvm_root, "bin", OperatingSystem.IsWindows() ? "clang-format.exe" : "clang-format");
        var application = new FormatApplication(
            new FormatFileSelector(new GitFileSelector(process_runner)),
            new ClangFormatter(process_runner, clang_format),
            Console.Out,
            Console.Error);
        return await application.RunAsync(arguments, Environment.CurrentDirectory);
    }
}
