#include <sbx/memory.h>
#include <sbx/memory_resource.h>

#include <gtest/gtest.h>
#include <native_soa_types.h>

#include <cstdint>

TEST(NativeSoaMimalloc, AllocatesAlignedColumnsAndPreservesValuesOnGrowth) {
    ioj::native_soa_fixture::SingleAlignmentRows rows{sbx::memory::mimalloc_resource()};
    rows.set_num(1);
    rows.get_view().view_nested().xs()[0] = 42.f;
    rows.reserve(129);

    auto const view{rows.get_view()};
    EXPECT_TRUE(sbx::memory::owns(view.bytes().data()));
    view.each_column([](auto column) {
        using Element = typename decltype(column)::value_type;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(column.data()) % alignof(Element), 0u);
    });
    EXPECT_EQ(view.view_nested().xs()[0], 42.f);
}
