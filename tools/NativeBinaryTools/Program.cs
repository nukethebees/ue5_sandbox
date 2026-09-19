namespace NativeBinaryTools;

public static class Program
{
    public static async Task<int> Main(string[] arguments)
    {
        var application = new NativeBinaryToolsApplication(
            new MimallocSymbolTool(new ProcessRunner(), Environment.CurrentDirectory),
            Console.Error);
        return await application.RunAsync(arguments);
    }
}
