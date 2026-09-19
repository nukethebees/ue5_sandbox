namespace CodeFormatTools;

public static class Program
{
    public static async Task<int> Main(string[] arguments)
    {
        var process_runner = new ProcessRunner();
        var application = new FormatApplication(
            new FormatFileSelector(new GitFileSelector(process_runner)),
            new ClangFormatter(process_runner),
            Console.Out,
            Console.Error);
        return await application.RunAsync(arguments, Environment.CurrentDirectory);
    }
}
