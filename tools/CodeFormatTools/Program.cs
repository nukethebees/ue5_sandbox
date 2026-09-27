namespace CodeFormatTools;

public static class Program
{
    public static async Task<int> Main(string[] arguments)
    {
        if (arguments is ["--version"])
        {
            Console.WriteLine($"CodeFormatTools {typeof(Program).Assembly.GetName().Version!.ToString(3)}");
            return 0;
        }

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
