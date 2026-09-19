#include <ioj/sim/entity_identity_layout.h>
#include <ioj/sim/entity_tables.h>
#include <ioj/sim/health_table.h>

#include <algorithm>
#include <array>

#include <gtest/gtest.h>

namespace ioj::sim::tests {
namespace {
auto make_id(EntityType const type, std::uint32_t const index) -> EntityUniqueId {
    return EntityUniqueId::make(entity_identity_offset(type, index), type);
}

template <std::size_t Size>
void apply_move(std::array<EntityUniqueId, Size> const& owners,
                std::array<HealthIndex, Size>& indices,
                HealthMove const& move) {
    auto const found{std::ranges::find(owners, move.owner)};
    ASSERT_NE(found, owners.end());
    indices[static_cast<std::size_t>(found - owners.begin())] = move.new_index;
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

    auto healths{table.get_view(indices, owners)};
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
    table.get_const_view(indices, owners).copy_to(copied);
    EXPECT_EQ(copied, replacement);
}

TEST(HealthTable, DenseRemovalMovesFinalRowAndRepairsMapping) {
    HealthTable table;
    std::array const owners{
        make_id(EntityType::Fighter, 0),
        make_id(EntityType::Fighter, 1),
        make_id(EntityType::CapitalShip, 0),
        make_id(EntityType::Turret, 0),
    };
    std::array<Health, owners.size()> const values{10, 20, 30, 40};
    std::array<HealthIndex, owners.size()> indices{};
    table.add(owners, values, indices);

    std::array const removed_rows{1};
    table.remove_rows(removed_rows, indices, owners, [&](HealthMove const& move) {
        apply_move(owners, indices, move);
    });

    EXPECT_EQ(table.num_slots(), 3);
    EXPECT_EQ(table.get_owner(HealthIndex{0}), owners[0]);
    EXPECT_EQ(table.get_owner(HealthIndex{1}), owners[3]);
    EXPECT_EQ(table.get_owner(HealthIndex{2}), owners[2]);
    EXPECT_EQ(table.get_health(indices[3], owners[3]), values[3]);
    EXPECT_FALSE(table.contains(indices[1], owners[1]));
    EXPECT_EQ(indices[3], HealthIndex{1});

    auto const replacement_owner{make_id(EntityType::CapitalShip, 1)};
    HealthIndex replacement_index;
    table.add(std::span<EntityUniqueId const>{&replacement_owner, 1},
              500,
              std::span<HealthIndex>{&replacement_index, 1});
    EXPECT_EQ(replacement_index, HealthIndex{3});
    EXPECT_EQ(table.num_slots(), 4);
}

TEST(HealthTable, LastRowRemovalShrinksWithoutFixup) {
    HealthTable table;
    std::array const owners{make_id(EntityType::Fighter, 0), make_id(EntityType::Turret, 0)};
    std::array<HealthIndex, owners.size()> indices{};
    table.add(owners, 100, indices);

    std::array const rows{1};
    std::int32_t move_count{};
    table.remove_rows(rows, indices, owners, [&](HealthMove const&) { ++move_count; });

    EXPECT_EQ(move_count, 0);
    EXPECT_EQ(table.num_slots(), 1);
    EXPECT_TRUE(table.contains(indices[0], owners[0]));
    EXPECT_FALSE(table.contains(indices[1], owners[1]));
}

TEST(HealthTable, MultipleDenseRemovalsRepairEverySurvivor) {
    HealthTable table;
    std::array const owners{
        make_id(EntityType::Fighter, 0),
        make_id(EntityType::Fighter, 1),
        make_id(EntityType::Fighter, 2),
        make_id(EntityType::Fighter, 3),
        make_id(EntityType::Fighter, 4),
    };
    std::array<Health, owners.size()> const values{10, 20, 30, 40, 50};
    std::array<HealthIndex, owners.size()> indices{};
    table.add(owners, values, indices);

    std::array const rows{3, 1};
    table.remove_rows(
        rows, indices, owners, [&](HealthMove const& move) { apply_move(owners, indices, move); });

    EXPECT_EQ(table.num_slots(), 3);
    EXPECT_EQ(table.get_owner(HealthIndex{0}), owners[0]);
    EXPECT_EQ(table.get_owner(HealthIndex{1}), owners[4]);
    EXPECT_EQ(table.get_owner(HealthIndex{2}), owners[2]);
    EXPECT_EQ(table.get_health(indices[0], owners[0]), values[0]);
    EXPECT_EQ(table.get_health(indices[2], owners[2]), values[2]);
    EXPECT_EQ(table.get_health(indices[4], owners[4]), values[4]);
    EXPECT_FALSE(table.contains(indices[1], owners[1]));
    EXPECT_FALSE(table.contains(indices[3], owners[3]));
}

TEST(HealthTable, DenseBatchRemovalHandlesBoundaryAndBatchShapes) {
    auto run_case = [](std::span<std::int32_t const> const removed_rows) {
        HealthTable table;
        std::array const owners{
            make_id(EntityType::Fighter, 0),
            make_id(EntityType::Fighter, 1),
            make_id(EntityType::Fighter, 2),
            make_id(EntityType::Fighter, 3),
            make_id(EntityType::Fighter, 4),
            make_id(EntityType::Fighter, 5),
        };
        std::array<Health, owners.size()> const values{10, 20, 30, 40, 50, 60};
        std::array<HealthIndex, owners.size()> indices{};
        table.add(owners, values, indices);

        table.remove_rows(removed_rows, indices, owners, [&](HealthMove const& move) {
            apply_move(owners, indices, move);
        });

        EXPECT_EQ(table.num_slots(),
                  static_cast<std::int32_t>(owners.size() - removed_rows.size()));
        for (std::size_t row{}; row < owners.size(); ++row) {
            auto const removed{std::ranges::find(removed_rows, static_cast<std::int32_t>(row)) !=
                               removed_rows.end()};
            EXPECT_EQ(table.contains(indices[row], owners[row]), !removed);
            if (!removed) {
                EXPECT_EQ(table.get_health(indices[row], owners[row]), values[row]);
            }
        }
    };

    run_case({});
    run_case(std::array{0});
    run_case(std::array{2});
    run_case(std::array{5});
    run_case(std::array{3, 2});
    run_case(std::array{5, 3, 1});
    run_case(std::array{5, 4, 3, 2, 0});
    run_case(std::array{5, 4, 3, 2, 1, 0});
}

TEST(HealthTable, MovedRowCanAlsoBeRemovedLaterInBatch) {
    HealthTable table;
    std::array const owners{
        make_id(EntityType::Fighter, 0),
        make_id(EntityType::Fighter, 1),
        make_id(EntityType::Fighter, 2),
        make_id(EntityType::Fighter, 3),
    };
    std::array<HealthIndex, owners.size()> indices{};
    std::array const table_order{owners[0], owners[2], owners[3], owners[1]};
    std::array<HealthIndex, owners.size()> table_indices{};
    table.add(table_order, std::array<Health, owners.size()>{10, 30, 40, 20}, table_indices);
    indices = {table_indices[0], table_indices[3], table_indices[1], table_indices[2]};

    std::array const removed_rows{3, 1};
    table.remove_rows(removed_rows, indices, owners, [&](HealthMove const& move) {
        apply_move(owners, indices, move);
    });

    EXPECT_EQ(table.num_slots(), 2);
    EXPECT_TRUE(table.contains(indices[0], owners[0]));
    EXPECT_TRUE(table.contains(indices[2], owners[2]));
    EXPECT_FALSE(table.contains(indices[1], owners[1]));
    EXPECT_FALSE(table.contains(indices[3], owners[3]));
}

TEST(EntityTables, DenseRemovalRepairsCrossTypeMapping) {
    SimClock clock;
    AgentIndexes indexes{clock};
    EntityTables tables{indexes};
    std::array const owners{
        make_id(EntityType::Fighter, 0),
        make_id(EntityType::Fighter, 1),
        make_id(EntityType::Turret, 0),
    };
    std::array<HealthIndex, owners.size()> indices{};
    tables.health.add(owners, std::array<Health, owners.size()>{10, 20, 30}, indices);

    indexes.bind(EntityType::Fighter, std::span{owners}.first<2>());
    indexes.bind(EntityType::Turret, std::span{owners}.last<1>());
    tables.bind_health_indices(
        EntityType::Fighter, std::span{owners}.first<2>(), std::span{indices}.first<2>());
    tables.bind_health_indices(
        EntityType::Turret, std::span{owners}.last<1>(), std::span{indices}.last<1>());

    std::array const removed_rows{0};
    tables.remove_health_rows(
        removed_rows, std::span{indices}.first<2>(), std::span{owners}.first<2>());

    EXPECT_EQ(indices[2], HealthIndex{0});
    EXPECT_TRUE(tables.health.contains(indices[2], owners[2]));
    EXPECT_EQ(tables.health.get_health(indices[2], owners[2]), 30);
}

TEST(EntityTables, MultipleDenseRemovalsRepairSameTypeMappings) {
    SimClock clock;
    AgentIndexes indexes{clock};
    EntityTables tables{indexes};
    std::array const owners{
        make_id(EntityType::Fighter, 0),
        make_id(EntityType::Fighter, 1),
        make_id(EntityType::Fighter, 2),
        make_id(EntityType::Fighter, 3),
        make_id(EntityType::Fighter, 4),
    };
    std::array<HealthIndex, owners.size()> indices{};
    tables.health.add(owners, std::array<Health, owners.size()>{10, 20, 30, 40, 50}, indices);

    indexes.bind(EntityType::Fighter, owners);
    tables.bind_health_indices(EntityType::Fighter, owners, indices);

    std::array const removed_rows{4, 1, 0};
    tables.remove_health_rows(removed_rows, indices, owners);

    EXPECT_EQ(tables.health.num_slots(), 2);
    EXPECT_EQ(indices[2], HealthIndex{0});
    EXPECT_EQ(indices[3], HealthIndex{1});
    EXPECT_EQ(tables.health.get_health(indices[2], owners[2]), 30);
    EXPECT_EQ(tables.health.get_health(indices[3], owners[3]), 40);
}

TEST(EntityTables, RetainedPlayerUsesTheSameReverseMappingContract) {
    SimClock clock;
    AgentIndexes indexes{clock};
    EntityTables tables{indexes};
    std::array const owners{
        make_id(EntityType::Fighter, 0),
        make_id(EntityType::PlayerShip, 0),
    };
    std::array<HealthIndex, owners.size()> indices{};
    tables.health.add(owners, std::array<Health, owners.size()>{10, 0}, indices);

    indexes.bind(EntityType::Fighter, std::span{owners}.first<1>());
    indexes.bind(EntityType::PlayerShip, std::span{owners}.last<1>());
    tables.bind_health_indices(
        EntityType::Fighter, std::span{owners}.first<1>(), std::span{indices}.first<1>());
    tables.bind_health_indices(
        EntityType::PlayerShip, std::span{owners}.last<1>(), std::span{indices}.last<1>());

    std::array const removed_rows{0};
    tables.remove_health_rows(
        removed_rows, std::span{indices}.first<1>(), std::span{owners}.first<1>());

    EXPECT_EQ(indices[1], HealthIndex{0});
    EXPECT_TRUE(tables.health.contains(indices[1], owners[1]));
    EXPECT_EQ(tables.health.get_health(indices[1], owners[1]), 0);
}

TEST(HealthTable, OwnerValidatedViewRejectsMismatchedMapping) {
    HealthTable table;
    std::array const owners{make_id(EntityType::Fighter, 0), make_id(EntityType::Fighter, 1)};
    std::array<HealthIndex, owners.size()> indices{};
    table.add(owners, 100, indices);

    std::array const mismatched_owners{owners[1], owners[0]};
    EXPECT_DEATH_IF_SUPPORTED(static_cast<void>(table.get_const_view(indices, mismatched_owners)),
                              "contains");
    EXPECT_DEATH_IF_SUPPORTED(static_cast<void>(table.get_health(indices[0], owners[1])),
                              "contains");
}

} // namespace ioj::sim::tests
