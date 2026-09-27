namespace BenchmarkTools;

public static class Program
{
    public static async Task<int> Main(string[] arguments)
    {
        using var cancellation = new CancellationTokenSource();
        ConsoleCancelEventHandler cancel = (_, args) =>
        {
            args.Cancel = true;
            cancellation.Cancel();
        };
        Console.CancelKeyPress += cancel;
        var application = new BenchmarkToolsApplication(
            new ProcessRunner(),
            new JobserverLocator(),
            new ProcessEnvironment(),
            Console.Out,
            Console.Error,
            Environment.ProcessPath ?? throw new InvalidOperationException("Could not determine the BenchmarkTools executable path."));
        try
        {
            return await application.RunAsync(arguments, Environment.CurrentDirectory, cancellation.Token);
        }
        catch (OperationCanceledException)
        {
            Console.Error.WriteLine("BenchmarkTools: cancelled.");
            return 130;
        }
        finally
        {
            Console.CancelKeyPress -= cancel;
        }
    }
}
