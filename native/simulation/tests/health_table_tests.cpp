#include <ioj/sim/entity_identity_layout.h>
#include <ioj/sim/health_table.h>

#include <array>

#include <gtest/gtest.h>

namespace ioj::sim::tests {
namespace {
auto make_id(EntityType const type, std::uint32_t const index) -> EntityUniqueId {
    return EntityUniqueId::make(entity_identity_offset(type, index), type);
}
}

TEST(HealthTable, AllocatesCrossTypeRowsAndExposesBulkViews) {
    HealthTable table;
    std::array const owners{
        make_id(EntityType::Fighter, 0),
        make_id(EntityType::CapitalShip, 0),
        make_id(EntityType::Turret, 0),
        make_id(EntityType::PlayerShip, 0),
    };
    std::array<Health, owners.size()> const values{100, 2000, 300, 400};
    std::array<HealthIndex, owners.size()> indices{};

    table.add(owners, values, indices);

    auto healths{table.get_view(indices)};
    ASSERT_EQ(healths.num(), static_cast<std::int32_t>(owners.size()));
    for (std::int32_t row{}; row < healths.num(); ++row) {
        EXPECT_EQ(healths.health(row), values[static_cast<std::size_t>(row)]);
        EXPECT_EQ(healths.owner(row), owners[static_cast<std::size_t>(row)]);
        EXPECT_TRUE(table.contains(indices[static_cast<std::size_t>(row)],
                                   owners[static_cast<std::size_t>(row)]));
    }

    std::array<Health, owners.size()> replacement{99, 1999, 299, 399};
    healths.copy_from(replacement);
    std::array<Health, owners.size()> copied{};
    table.get_const_view(indices).copy_to(copied);
    EXPECT_EQ(copied, replacement);
}

TEST(HealthTable, RemovesRowsAndReusesStableSlots) {
    HealthTable table;
    std::array const owners{make_id(EntityType::Fighter, 0), make_id(EntityType::Turret, 0)};
    std::array<HealthIndex, owners.size()> indices{};
    table.add(owners, 100, indices);

    std::array const removed_rows{1};
    table.remove_rows(removed_rows, indices, owners);
    EXPECT_FALSE(table.contains(indices[1], owners[1]));
    EXPECT_TRUE(table.contains(indices[0], owners[0]));

    auto const replacement_owner{make_id(EntityType::CapitalShip, 0)};
    HealthIndex replacement_index;
    table.add(std::span<EntityUniqueId const>{&replacement_owner, 1},
              500,
              std::span<HealthIndex>{&replacement_index, 1});

    EXPECT_EQ(replacement_index, indices[1]);
    EXPECT_TRUE(table.contains(replacement_index, replacement_owner));
    EXPECT_FALSE(table.contains(replacement_index, owners[1]));
    EXPECT_EQ(table.get_owner(replacement_index), replacement_owner);
    EXPECT_EQ(table.num_slots(), 2);
}

TEST(HealthTable, RemovesMultipleRowsBeforeSwapRemoval) {
    HealthTable table;
    std::array const owners{
        make_id(EntityType::Fighter, 0),
        make_id(EntityType::Fighter, 1),
        make_id(EntityType::Fighter, 2),
    };
    std::array<HealthIndex, owners.size()> indices{};
    table.add(owners, 100, indices);

    std::array const rows{2, 0};
    table.remove_rows(rows, indices, owners);

    EXPECT_FALSE(table.contains(indices[0], owners[0]));
    EXPECT_TRUE(table.contains(indices[1], owners[1]));
    EXPECT_FALSE(table.contains(indices[2], owners[2]));
}

} // namespace ioj::sim::tests
