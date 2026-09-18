using System.Text;

namespace SetLiveCodingDisabled;

public static class Program
{
    public static int Main(string[] arguments)
    {
        if (arguments.Length != 2 ||
            !string.Equals(arguments[0], "--settings-path", StringComparison.Ordinal) ||
            string.IsNullOrWhiteSpace(arguments[1]))
        {
            Console.Error.WriteLine("Usage: SetLiveCodingDisabled --settings-path <path>");
            return 2;
        }

        try
        {
            var normalizer = new LiveCodingSettingsNormalizer();
            var changed = normalizer.Disable(arguments[1]);

            if (changed)
            {
                Console.WriteLine($"Disabled Live Coding in '{Path.GetFullPath(arguments[1])}'.");
            }

            return 0;
        }
        catch (Exception exception) when (exception is ArgumentException or DecoderFallbackException or IOException or UnauthorizedAccessException)
        {
            Console.Error.WriteLine($"SetLiveCodingDisabled: {exception.Message}");
            return 1;
        }
    }
}
