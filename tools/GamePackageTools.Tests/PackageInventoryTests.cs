using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace GamePackageTools.Tests;

[TestClass]
public sealed class PackageInventoryTests
{
    [TestMethod]
    public void Contains_is_case_insensitive_and_normalizes_backslashes()
    {
        var inventory = new PackageInventory("../../../Sandbox\\Content\\UI\\DA_UI_DATA", string.Empty);

        Assert.IsTrue(inventory.Contains("../../../sandbox/content/ui/da_ui_data"));
    }
}
