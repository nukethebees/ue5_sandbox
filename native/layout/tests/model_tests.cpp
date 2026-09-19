#include <ioj/layout/model.hpp>

#include <gtest/gtest.h>

namespace ioj::layout {
namespace {

TEST(SchemaCatalog, StoresUniqueSchemaIdentities) {
    SchemaCatalog catalog;
    PackedLayout const layout{
        .id = {.kind = SchemaKind::packed_value, .module_name = "module", .schema_name = "Value"},
        .storage_type = "std::uint32_t",
        .fields = {},
        .invalid_raw_value = std::nullopt,
    };

    EXPECT_TRUE(catalog.add(layout));
    EXPECT_FALSE(catalog.add(layout));
    EXPECT_EQ(catalog.items().size(), 1);
    EXPECT_EQ(catalog.find(layout.id), &catalog.items().front());
}

} // namespace
} // namespace ioj::layout
