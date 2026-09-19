namespace GamePackageTools;

public sealed class PackageInventory
{
    private readonly string contents;

    public PackageInventory(string pakListing, string ioStoreListing)
    {
        ArgumentNullException.ThrowIfNull(pakListing);
        ArgumentNullException.ThrowIfNull(ioStoreListing);

        contents = (pakListing + "\n" + ioStoreListing).Replace('\\', '/');
    }

    public bool Contains(string value)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(value);

        return contents.IndexOf(value, StringComparison.OrdinalIgnoreCase) >= 0;
    }
}
