namespace BenchmarkTools;

public static class Program
{
    public static async Task<int> Main(string[] arguments)
    {
        var application = new BenchmarkToolsApplication(
            new ProcessRunner(),
            new JobserverLocator(),
            new ProcessEnvironment(),
            Console.Out,
            Console.Error,
            Environment.ProcessPath ?? throw new InvalidOperationException("Could not determine the BenchmarkTools executable path."));
        return await application.RunAsync(arguments, Environment.CurrentDirectory);
    }
}
