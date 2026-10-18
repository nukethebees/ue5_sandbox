#include <ioj/range_fits.h>
#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/entities/entity_type.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <type_traits>

namespace ioj::sim::tests {
namespace {
static_assert(EntityId::index_field::offset == 0);
static_assert(EntityId::index_field::bits == 24);
static_assert(EntityId::index_field::value_mask == 0x00ffffffu);
static_assert(EntityId::index_field::mask == 0x00ffffffu);
static_assert(EntityId::entity_type_field::offset == 24);
static_assert(EntityId::entity_type_field::bits == 8);
static_assert(EntityId::entity_type_field::value_mask == 0x000000ffu);
static_assert(EntityId::entity_type_field::mask == 0xff000000u);
static_assert((EntityId::index_field::mask & EntityId::entity_type_field::mask) == 0);
static_assert((EntityId::index_field::mask | EntityId::entity_type_field::mask) == 0xffffffffu);
static_assert(EntityId::invalid_value == 0xffffffffu);
static_assert(sizeof(EntityId) == sizeof(std::uint32_t));
static_assert(std::is_trivially_copyable_v<EntityId>);
static_assert(std::is_standard_layout_v<EntityId>);

template <typename T>
concept HasTryMake = requires(T& result) { T::try_make(0, EntityType::PlayerShip, result); };

template <typename T>
concept HasTrySetIndex = requires(T id) { id.try_set_index(0); };

template <typename T>
concept HasSetIndex = requires(T id) { id.set_index(0); };

template <typename T>
concept HasTrySetEntityType = requires(T id) { id.try_set_entity_type(EntityType::PlayerShip); };

template <typename T>
concept HasSetEntityType = requires(T id) { id.set_entity_type(EntityType::PlayerShip); };

constexpr auto zero_player_id{EntityId(0, EntityType::PlayerShip)};
static_assert(zero_player_id.raw_value() == 0);
static_assert(zero_player_id.index() == 0);
static_assert(zero_player_id.entity_type() == EntityType::PlayerShip);
static_assert(zero_player_id.is_valid());

constexpr auto maximum_fighter_id{EntityId(EntityId::index_field::value_mask, EntityType::Fighter)};
static_assert(maximum_fighter_id.index() == EntityId::index_field::value_mask);
static_assert(maximum_fighter_id.entity_type() == EntityType::Fighter);
static_assert(!HasTryMake<EntityId>);
static_assert(!HasTrySetIndex<EntityId>);
static_assert(!HasSetIndex<EntityId>);
static_assert(!HasTrySetEntityType<EntityId>);
static_assert(!HasSetEntityType<EntityId>);

TEST(EntityId, RoundTripsRepresentativeIndicesAndEntityTypes) {
    constexpr std::array indices{
        std::uint32_t{0},
        std::uint32_t{1},
        std::uint32_t{0xff},
        std::uint32_t{0x100},
        std::uint32_t{0xffff},
        std::uint32_t{0x10000},
        std::uint32_t{0xfffffe},
        std::uint32_t{0xffffff},
    };
    constexpr std::array types{
        EntityType::PlayerShip,
        EntityType::Turret,
        EntityType::CapitalShip,
        EntityType::Fighter,
        EntityType::TubeSpinner,
    };

    for (auto const index : indices) {
        for (auto const type : types) {
            auto const id{EntityId(index, type)};
            EXPECT_EQ(id.index(), index);
            EXPECT_EQ(id.entity_type(), type);
        }
    }
}

TEST(EntityId, IndexAndTypeBitsDoNotOverlap) {
    auto const index{std::uint32_t{0x0055aa33}};
    auto const turret{EntityId(index, EntityType::Turret)};
    auto const fighter{EntityId(index, EntityType::Fighter)};
    EXPECT_EQ(turret.index(), fighter.index());
    EXPECT_NE(turret.raw_value(), fighter.raw_value());

    auto const first{EntityId(1, EntityType::CapitalShip)};
    auto const second{EntityId(0x00fedcba, EntityType::CapitalShip)};
    EXPECT_EQ(first.entity_type(), second.entity_type());
    EXPECT_NE(first.raw_value(), second.raw_value());
}

TEST(EntityId, PacksFieldsDirectly) {
    auto const id{EntityId(0x00123456, EntityType::CapitalShip)};
    EXPECT_EQ(id.raw_value(), std::uint32_t{0x02123456});
    EXPECT_EQ(id.index(), std::uint32_t{0x00123456});
    EXPECT_EQ(id.entity_type(), EntityType::CapitalShip);
}

TEST(EntityId, InvalidRepresentationAndDomainArePreserved) {
    EntityId const null_id{};
    EXPECT_FALSE(null_id.is_valid());
    EXPECT_EQ(null_id.raw_value(), std::uint32_t{0xffffffff});

    EXPECT_FALSE(EntityId::from_raw(std::uint32_t{0xffffffff}).is_valid());
}

TEST(EntityId, AssertsWhenConstructionViolatesThePackedDomain) {
    EXPECT_DEATH(
        static_cast<void>(EntityId(EntityId::index_field::value_mask + 1, EntityType::Turret)), "");
    EXPECT_DEATH(static_cast<void>(EntityId(0, static_cast<EntityType>(enum_count<EntityType>()))),
                 "");
}

TEST(EntityId, AllocationRangeDetectsExhaustionWithoutWrapping) {
    constexpr auto minimum{EntityId::index_minimum};
    constexpr auto maximum{EntityId::index_maximum};
    EXPECT_TRUE(range_fits(0u, 0u, minimum, maximum));
    EXPECT_TRUE(range_fits(0u, maximum + 1, minimum, maximum));
    EXPECT_TRUE(range_fits(maximum, 1u, minimum, maximum));
    EXPECT_FALSE(range_fits(maximum, 2u, minimum, maximum));
    EXPECT_FALSE(range_fits(maximum + 1, 1u, minimum, maximum));
}
} // namespace
} // namespace ioj::sim::tests
