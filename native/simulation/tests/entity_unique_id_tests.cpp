#include <ioj/sim/entity_types.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <type_traits>

namespace ioj::sim::tests {
namespace {
static_assert(EntityUniqueId::index_offset == 0);
static_assert(EntityUniqueId::index_bits == 24);
static_assert(EntityUniqueId::index_value_mask == 0x00ffffffu);
static_assert(EntityUniqueId::index_mask == 0x00ffffffu);
static_assert(EntityUniqueId::entity_type_offset == 24);
static_assert(EntityUniqueId::entity_type_bits == 8);
static_assert(EntityUniqueId::entity_type_value_mask == 0x000000ffu);
static_assert(EntityUniqueId::entity_type_mask == 0xff000000u);
static_assert((EntityUniqueId::index_mask & EntityUniqueId::entity_type_mask) == 0);
static_assert((EntityUniqueId::index_mask | EntityUniqueId::entity_type_mask) == 0xffffffffu);
static_assert(EntityUniqueId::invalid_value == 0xffffffffu);
static_assert(sizeof(EntityUniqueId) == sizeof(std::uint32_t));
static_assert(std::is_trivially_copyable_v<EntityUniqueId>);
static_assert(std::is_standard_layout_v<EntityUniqueId>);

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

constexpr auto zero_player_id{EntityUniqueId::make(0, EntityType::PlayerShip)};
static_assert(zero_player_id.raw_value() == 0);
static_assert(zero_player_id.index() == 0);
static_assert(zero_player_id.entity_type() == EntityType::PlayerShip);
static_assert(zero_player_id.is_valid());

constexpr auto maximum_fighter_id{
    EntityUniqueId::make(EntityUniqueId::index_value_mask, EntityType::Fighter)};
static_assert(maximum_fighter_id.index() == EntityUniqueId::index_value_mask);
static_assert(maximum_fighter_id.entity_type() == EntityType::Fighter);
static_assert(!HasTryMake<EntityUniqueId>);
static_assert(!HasTrySetIndex<EntityUniqueId>);
static_assert(!HasSetIndex<EntityUniqueId>);
static_assert(!HasTrySetEntityType<EntityUniqueId>);
static_assert(!HasSetEntityType<EntityUniqueId>);

TEST(EntityUniqueId, RoundTripsRepresentativeIndicesAndEntityTypes) {
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
            auto const id{EntityUniqueId::make(index, type)};
            EXPECT_EQ(id.index(), index);
            EXPECT_EQ(id.entity_type(), type);
        }
    }
}

TEST(EntityUniqueId, IndexAndTypeBitsDoNotOverlap) {
    auto const index{std::uint32_t{0x0055aa33}};
    auto const turret{EntityUniqueId::make(index, EntityType::Turret)};
    auto const fighter{EntityUniqueId::make(index, EntityType::Fighter)};
    EXPECT_EQ(turret.index(), fighter.index());
    EXPECT_NE(turret.raw_value(), fighter.raw_value());

    auto const first{EntityUniqueId::make(1, EntityType::CapitalShip)};
    auto const second{EntityUniqueId::make(0x00fedcba, EntityType::CapitalShip)};
    EXPECT_EQ(first.entity_type(), second.entity_type());
    EXPECT_NE(first.raw_value(), second.raw_value());
}

TEST(EntityUniqueId, PacksFieldsDirectly) {
    auto const id{EntityUniqueId::make(0x00123456, EntityType::CapitalShip)};
    EXPECT_EQ(id.raw_value(), std::uint32_t{0x02123456});
    EXPECT_EQ(id.index(), std::uint32_t{0x00123456});
    EXPECT_EQ(id.entity_type(), EntityType::CapitalShip);
}

TEST(EntityUniqueId, InvalidRepresentationAndDomainArePreserved) {
    EntityUniqueId const null_id{};
    EXPECT_FALSE(null_id.is_valid());
    EXPECT_EQ(null_id.raw_value(), std::uint32_t{0xffffffff});

    EXPECT_FALSE(EntityUniqueId{std::uint32_t{0xffffffff}}.is_valid());
}

TEST(EntityUniqueId, AssertsWhenConstructionViolatesThePackedDomain) {
    EXPECT_DEATH(static_cast<void>(EntityUniqueId::make(EntityUniqueId::index_value_mask + 1,
                                                        EntityType::Turret)),
                 "");
    EXPECT_DEATH(static_cast<void>(EntityUniqueId::make(0, EntityType::COUNT)), "");
}

TEST(EntityUniqueId, AllocationRangeDetectsExhaustionWithoutWrapping) {
    EXPECT_TRUE(EntityUniqueId::index_range_fits(0, 0));
    EXPECT_TRUE(EntityUniqueId::index_range_fits(0, EntityUniqueId::index_value_mask + 1));
    EXPECT_TRUE(EntityUniqueId::index_range_fits(EntityUniqueId::index_value_mask, 1));
    EXPECT_FALSE(EntityUniqueId::index_range_fits(EntityUniqueId::index_value_mask, 2));
    EXPECT_FALSE(EntityUniqueId::index_range_fits(EntityUniqueId::index_value_mask + 1, 1));
}
} // namespace
} // namespace ioj::sim::tests
