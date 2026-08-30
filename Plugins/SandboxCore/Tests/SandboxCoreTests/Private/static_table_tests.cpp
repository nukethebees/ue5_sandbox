#include "static_table_test_types.h"

#include "CoreMinimal.h"
#include "TestHarness.h"

TEST_CASE("SandboxCore.StaticTable exposes fixed rows and table-level functions") {
    using Table = ml::static_table_tests::FTestStaticTable;

    static_assert(Table::num() == 3);
    static_assert(Table::first_index == 0);
    static_assert(Table::second_index == 1);
    static_assert(Table::third_index == 2);

    Table table{};
    CHECK(table.ids[Table::first_index] == 0);
    CHECK(table.weights[Table::third_index] == 0.0f);

    table.ids[Table::first_index] = 10;
    table.ids[Table::second_index] = 20;
    table.weights[Table::first_index] = 1.0f;
    table.weights[Table::second_index] = 2.0f;

    table.apply_arrays([](auto& ids, auto& weights) {
        ids[Table::third_index] = {};
        weights[Table::third_index] = {};
    });
    CHECK(table.ids[Table::third_index] == 0);
    CHECK(table.weights[Table::third_index] == 0.0f);

    Table copy{};
    table.apply_array_pairs(copy, [](auto const& source_ids, auto& destination_ids, auto const& source_weights, auto& destination_weights) {
        destination_ids[Table::second_index] = source_ids[Table::second_index];
        destination_weights[Table::second_index] = source_weights[Table::second_index];
    });
    CHECK(copy.ids[Table::second_index] == 20);
    CHECK(copy.weights[Table::second_index] == 2.0f);

    auto const& const_table{table};
    int32 visited{};
    const_table.apply_arrays([&visited]<typename... Columns>(Columns const&...) { visited = sizeof...(Columns); });
    CHECK(visited == 2);
}

TEST_CASE("SandboxCore.StaticTable constructs grouped aggregate values") {
    using Table = ml::static_table_tests::FTestStaticGroupTable;

    Table table{};
    table.min_xs[Table::second_index] = 1.0f;
    table.min_ys[Table::second_index] = 2.0f;
    table.min_zs[Table::second_index] = 3.0f;
    table.max_xs[Table::second_index] = 4.0f;
    table.max_ys[Table::second_index] = 5.0f;
    table.max_zs[Table::second_index] = 6.0f;

    CHECK(table.get_min_point(Table::second_index) == FVector3f{1.0f, 2.0f, 3.0f});
    CHECK(table.get_max_point(Table::second_index) == FVector3f{4.0f, 5.0f, 6.0f});
}
