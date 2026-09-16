#include "Generated.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <span>
#include <utility>
#include <variant>

#undef check
#define check(expression) EXPECT_TRUE(expression)

namespace codegen_compile_fixture {

auto FRows::manual_value() const -> int32 {
    return 77;
}

} // namespace codegen_compile_fixture

namespace {

using namespace codegen_compile_fixture;

template <typename T>
concept BorrowsOwner = requires(T&& owner) { std::forward<T>(owner).get_view(); };

template <typename T>
concept BorrowsConstOwner = requires(T&& owner) { std::forward<T>(owner).get_const_view(); };

template <typename T>
concept SlicesOwner = requires(T&& owner) { std::forward<T>(owner).slice(0, 0); };

struct FSettingsAccessFixture : TSettingsAccess<FSettingsAccessFixture> {
    auto settings_state() const -> FSettingsState const& { return state; }
    void set_setting(EGameSetting const id, FGameSettingValue const& value) {
        last_setting = id;
        EXPECT_TRUE(set_game_setting_value(state, id, value));
    }

    FSettingsState state;
    EGameSetting last_setting{EGameSetting::VSync};
};

TEST(GeneratedSingleAllocationSoa, Ownership) {
    using Owner = CountedParents;
    static_assert(BorrowsOwner<Owner&> && BorrowsOwner<Owner const&>);
    static_assert(!BorrowsOwner<Owner> && !BorrowsOwner<Owner const>);
    static_assert(!BorrowsConstOwner<Owner> && !BorrowsConstOwner<Owner const>);
    static_assert(!SlicesOwner<Owner> && !SlicesOwner<Owner const>);
    static_assert(SlicesOwner<Owner::View> && SlicesOwner<Owner::ConstView>);
    static_assert(std::is_nothrow_move_constructible_v<Owner>);
    static_assert(std::is_nothrow_move_assignable_v<Owner>);
    static_assert(!std::is_copy_assignable_v<Owner>);
    {
        Owner source;
        source.reserve(0);
        source.add_defaulted(0);
        check(CountingAllocator::allocations == 0);
        source.reserve(3);
        check(CountingAllocator::allocations == 1 && CountingAllocator::frees == 0);
        check(CountingAllocator::last_bytes == source.allocated_bytes());
        check(CountingAllocator::last_alignment == Owner::allocation_alignment);
        source.add_defaulted(source.capacity());
        source.get_view().keys()[0] = 42;
        check(CountingAllocator::allocations == 1);
        source.add_defaulted(1);
        check(CountingAllocator::allocations == 2 && CountingAllocator::frees == 1);
        check(source.get_view().keys()[0] == 42);
        auto* const pointer{source.get_view().keys().GetData()};
        Owner moved{std::move(source)};
        check(source.num() == 0 && source.capacity() == 0 && source.allocated_bytes() == 0);
        check(CountingAllocator::allocations == 2 && CountingAllocator::frees == 1);
        Owner destination;
        destination.reserve(17);
        check(CountingAllocator::allocations == 3);
        destination = std::move(moved);
        check(CountingAllocator::frees == 2);
        check(moved.num() == 0 && moved.capacity() == 0);
        auto& alias{destination};
        destination = std::move(alias);
        check(CountingAllocator::frees == 2);
        check(destination.get_view().keys().GetData() == pointer);
        auto const capacity{destination.capacity()};
        destination.reset();
        check(destination.num() == 0 && destination.capacity() == capacity);
        check(CountingAllocator::frees == 2);
        CountingAllocator::reject_allocation = true;
        bool rejected{};
        try {
            destination.reserve(Owner::max_capacity);
        } catch (std::bad_alloc const&) {
            rejected = true;
        }
        CountingAllocator::reject_allocation = false;
        check(rejected && destination.capacity() == capacity);
        check(CountingAllocator::last_bytes ==
              Owner::layout_bytes(Owner::max_capacity / Owner::capacity_granularity));
        check(CountingAllocator::allocations == 3 && CountingAllocator::frees == 2);
    }
    check(CountingAllocator::allocations == 3 && CountingAllocator::frees == 3);
}

TEST(GeneratedSingleAllocationSoa, BulkMutationAndAliasing) {
    FParents ordinary;
    ordinary.add_defaulted(3);
    for (int32 i{}; i < ordinary.num(); ++i) {
        ordinary.keys[i] = i + 10;
        ordinary.children.values[i] = i + 20;
    }
    SingleParents from_ordinary;
    from_ordinary.set_num(1);
    EXPECT_EQ(from_ordinary.append_from(ordinary.get_const_view().slice(1, 2)), 1);
    EXPECT_EQ(from_ordinary.get_const_view().keys()[1], 11);
    EXPECT_EQ(from_ordinary.get_const_view().view_children().values[1], 21);

    SingleParents rows;
    rows.set_num(65);
    for (int32 i{}; i < rows.num(); ++i) {
        rows.get_view().keys()[i] = i;
        rows.get_view().view_children().values[i] = i * 10;
    }

    auto const original_capacity{rows.capacity()};
    EXPECT_EQ(rows.append_from(rows), 65);
    EXPECT_GT(rows.capacity(), original_capacity);
    EXPECT_EQ(rows.num(), 130);
    EXPECT_EQ(rows.get_view().keys()[129], 64);
    EXPECT_EQ(rows.get_view().view_children().values[129], 640);

    auto const source{rows.slice(1, 3)};
    EXPECT_EQ(rows.append_from(source), 130);
    EXPECT_EQ(rows.get_view().keys()[130], 1);
    EXPECT_EQ(rows.get_view().view_children().values[132], 30);

    SingleParents removed;
    removed.set_num(8);
    for (int32 i{}; i < removed.num(); ++i) {
        removed.get_view().keys()[i] = i;
        removed.get_view().view_children().values[i] = i * 10;
    }
    std::array<int32, 3> const indices{6, 3, 1};
    removed.remove_at_swap(std::span<int32 const>{indices});

    std::array<int32, 5> const expected{0, 5, 2, 7, 4};
    ASSERT_EQ(removed.num(), static_cast<int32>(expected.size()));
    for (int32 i{}; i < removed.num(); ++i) {
        EXPECT_EQ(removed.get_view().keys()[i], expected[static_cast<std::size_t>(i)]);
        EXPECT_EQ(removed.get_view().view_children().values[i],
                  expected[static_cast<std::size_t>(i)] * 10);
    }
}

TEST(GeneratedHomogeneousStorage, Operations) {
    FValuesf values;
    values.add(3.0f, 30.0f);
    values.add(FVector2f{1.0f, 10.0f});
    values.add(FPoint2f{2.0f, 20.0f});
    values.validate_array_sizes();

    TArray<int32> scratch;
    scratch.AddUninitialized(values.num());
    values.sort([](auto const& rows,
                   int32 const lhs,
                   int32 const rhs) { return rows.xs[lhs] < rows.xs[rhs]; },
                scratch);
    check(values.at(0).X == 1.0f && values.at(0).Y == 10.0f);
    check(values.at(1).X == 2.0f && values.at(1).Y == 20.0f);
    check(values.get_const_view().at(2).X == 3.0f);

    FValuesf copy;
    copy.add_defaulted(values.num());
    copy.copy_elements(0, values.get_const_view(), 0, values.num());
    check(copy.at(1).Y == 20.0f);
    copy.remove_at_swap(0, 1, EAllowShrinking::No);
    check(copy.num() == 2);
    check(copy.at(0).X == 3.0f && copy.at(0).Y == 30.0f);

    FScalari scalar;
    scalar.values.Add(7);
    check(scalar.get_const_view().values[0] == 7);

    FValuesd doubles;
    doubles.add(4.0, 5.0);
    check(doubles.at(0).X == 4.0 && doubles.at(0).Y == 5.0);
}

TEST(GeneratedDynamicSoa, Operations) {
    FRows rows;
    check(rows.add(30, 3.0f) == 0);
    check(rows.add(10, 1.0f) == 1);
    check(rows.add(20, 2.0f) == 2);
    rows.validate_array_sizes();
    rows.reserve(16);
    check(rows.first_id() == 30);
    check(rows.weight_sum() == 4.0f);
    check(rows.manual_value() == 77);

    TArray<int32> scratch;
    scratch.AddUninitialized(rows.num());
    rows.sort([](auto const& values,
                 int32 const lhs,
                 int32 const rhs) { return values.ids[lhs] < values.ids[rhs]; },
              scratch);
    check(rows.at(0).id == 10 && rows.at(0).weight == 1.0f);
    check(rows.at(2).id == 30 && rows.at(2).weight == 3.0f);
    check(rows.slice(1, 1).at(0).id == 20);

    FRows copied;
    copied.add_defaulted(rows.num());
    copied.copy_elements(0, rows.get_const_view(), 0, rows.num());
    check(copied.at(2).weight == 3.0f);
    copied.copy_element(0, rows.get_const_view(), 2);
    check(copied.at(0).id == 30 && copied.at(0).weight == 3.0f);
    copied.copy_to_tail(rows.get_const_view().left(1));
    check(copied.at(2).id == 10);

    FRows appended;
    appended.append_from(rows.get_const_view());
    appended.append_from(copied.get_const_view().left(1));
    check(appended.num() == 4);
    check(appended.at(3).id == 30);
    appended.remove_at_swap(1, 2, EAllowShrinking::No);
    check(appended.num() == 2);
    appended.reset();
    check(appended.is_empty());
    appended.add_uninitialised(2);
    appended.set(0, 1, 10.0f);
    appended.get_view().set(1, 2, 20.0f);
    appended.set_num(1, EAllowShrinking::No);
    check(appended.num() == 1 && appended.at(0).weight == 10.0f);

    FParents parents;
    parents.keys.Add(1);
    parents.children.values.Add(10);
    parents.keys.Add(2);
    parents.children.values.Add(20);
    parents.remove_at_swap(0, 1, EAllowShrinking::No);
    parents.validate_array_sizes();
    check(parents.keys[0] == 2 && parents.children.values[0] == 20);

    FParents nested_copy;
    nested_copy.add_defaulted(parents.num());
    nested_copy.copy_elements(0, parents.get_const_view(), 0, parents.num());
    check(nested_copy.keys[0] == 2 && nested_copy.children.values[0] == 20);
    nested_copy.append_from(parents.get_const_view());
    nested_copy.validate_array_sizes();
    check(nested_copy.num() == 2);
    check(nested_copy.keys[1] == 2 && nested_copy.children.values[1] == 20);
}

TEST(GeneratedFieldMask, Operations) {
    static_assert(FFieldMask8::field_count == 8);
    static_assert(sizeof(FFieldMask8) == sizeof(uint8));
    static_assert(FFieldMask9::field_count == 9);
    static_assert(sizeof(FFieldMask9) == sizeof(uint16));
    static_assert(sizeof(FFieldMask16) == sizeof(uint16));
    static_assert(sizeof(FFieldMask17) == sizeof(uint32));
    static_assert(sizeof(FFieldMask32) == sizeof(uint32));
    static_assert(sizeof(FFieldMask33) == sizeof(uint64));

    constexpr auto last_value{FFieldMask8::values_field(7)};
    static_assert(FFieldMask8::index(last_value) == 7);

    FFieldMask8 mask;
    check(mask.is_empty());
    mask.set(last_value);
    check(mask.has(last_value) && mask.value() == uint8{0x80});
    mask.clear(last_value);
    check(mask.is_empty());

    FFieldMask9 wide;
    wide.set(EField9::Tail);
    check(wide.has(EField9::Tail) && wide.value() == uint16{0x100});
    FFieldMask9 merged;
    merged.set(wide);
    check(merged.has(EField9::Tail));

    FFieldMask16 mask16;
    auto const high16{FFieldMask16::values_field(15)};
    mask16.set(high16);
    EXPECT_EQ(mask16.value(), uint16{0x8000});
    EXPECT_EQ(FFieldMask16::index(high16), 15);

    FFieldMask17 mask17;
    auto const high17{FFieldMask17::values_field(16)};
    mask17.set(high17);
    EXPECT_EQ(mask17.value(), uint32{0x10000});
    EXPECT_EQ(FFieldMask17::index(high17), 16);

    FFieldMask32 mask32;
    auto const high32{FFieldMask32::values_field(31)};
    mask32.set(high32);
    EXPECT_EQ(mask32.value(), uint32{0x80000000});
    EXPECT_EQ(FFieldMask32::index(high32), 31);

    FFieldMask33 mask33;
    auto const high33{FFieldMask33::values_field(32)};
    mask33.set(high33);
    EXPECT_EQ(mask33.value(), uint64{0x100000000});
    EXPECT_EQ(FFieldMask33::index(high33), 32);
    mask33.clear(high33);
    EXPECT_TRUE(mask33.is_empty());
}

TEST(GeneratedPackedValue, HasStorageLayoutProperties) {
    static_assert(sizeof(FighterState) == sizeof(std::uint32_t));
    static_assert(std::is_trivially_copyable_v<FighterState>);
    static_assert(std::is_standard_layout_v<FighterState>);
    static_assert([] {
        FighterState value;
        value.set_entity_index(0x123456u);
        value.set_state(PackedState::AB);
        return value.raw_value() == 0xAB123456u;
    }());
}

TEST(GeneratedPackedValue, CombinesAndExtractsFields) {
    FighterState value;
    value.set_entity_index(0x123456u);
    value.set_state(PackedState::AB);
    EXPECT_EQ(value.raw_value(), 0xAB123456u);

    auto const from_raw{FighterState{0xAB123456u}};
    EXPECT_EQ(from_raw.entity_index(), 0x123456u);
    EXPECT_EQ(from_raw.state(), PackedState::AB);
}

TEST(GeneratedPackedValue, SettersPreserveOtherFieldsAndRejectOverflow) {
    FighterState value{0xAB123456u};
    value.set_entity_index(0xffffffu);
    EXPECT_EQ(value.raw_value(), 0xABffffffu);
    value.set_state(PackedState::Zero);
    EXPECT_EQ(value.raw_value(), 0x00ffffffu);
    value.set_state(PackedState::Max);
    EXPECT_EQ(value.raw_value(), 0xffffffffu);

    auto const before_failure{value.raw_value()};
    EXPECT_FALSE(value.try_set_entity_index(0x01000000u));
    EXPECT_EQ(value.raw_value(), before_failure);
}

TEST(GeneratedPackedValue, ComparesByRawValue) {
    EXPECT_LT(FighterState{1u}, FighterState{2u});
    EXPECT_LE(FighterState{1u}, FighterState{2u});
    EXPECT_GT(FighterState{2u}, FighterState{1u});
    EXPECT_GE(FighterState{2u}, FighterState{1u});
    EXPECT_EQ(FighterState{2u}, FighterState{2u});
    EXPECT_NE(FighterState{1u}, FighterState{2u});
}

TEST(GeneratedPackedValue, ExhaustivelyRoundTripsUint8Storage) {
    for (unsigned raw{}; raw <= 0xffu; ++raw) {
        auto const packed{PackedByte{static_cast<std::uint8_t>(raw)}};
        EXPECT_EQ(packed.low(), raw & 0x7u);
        EXPECT_EQ(packed.flag(), (raw & 0x8u) != 0);
        EXPECT_EQ(packed.high(), (raw >> 4u) & 0xfu);

        PackedByte rebuilt;
        rebuilt.set_low(static_cast<std::uint8_t>(raw & 0x7u));
        rebuilt.set_flag((raw & 0x8u) != 0);
        rebuilt.set_high(static_cast<std::uint8_t>((raw >> 4u) & 0xfu));
        EXPECT_EQ(rebuilt.raw_value(), raw);

        for (std::uint8_t low{}; low < 8; ++low) {
            auto changed{packed};
            changed.set_low(low);
            EXPECT_EQ(changed.raw_value(), static_cast<std::uint8_t>((raw & 0xf8u) | low));
        }
        for (std::uint8_t high{}; high < 16; ++high) {
            auto changed{packed};
            changed.set_high(high);
            EXPECT_EQ(changed.raw_value(), static_cast<std::uint8_t>((raw & 0x0fu) | (high << 4u)));
        }
    }
}

TEST(GeneratedPackedValue, HandlesFullWidthStorageAndNarrowEnums) {
    auto const maximum{std::numeric_limits<std::uint64_t>::max()};
    PackedWide wide;
    EXPECT_TRUE(wide.try_set_value(maximum));
    EXPECT_EQ(wide.value(), maximum);
    EXPECT_EQ(wide.raw_value(), maximum);
    EXPECT_EQ(PackedWide{std::uint64_t{0x123456789abcdef0}}.value(),
              std::uint64_t{0x123456789abcdef0});

    PackedTinyState tiny;
    tiny.set_state(TinyState::One);
    tiny.set_payload(std::uint8_t{42});
    EXPECT_EQ(tiny.raw_value(), std::uint8_t{0xa9});

    auto const before_failure{tiny.raw_value()};
    EXPECT_FALSE(tiny.try_set_state(static_cast<TinyState>(4)));
    EXPECT_EQ(tiny.raw_value(), before_failure);
    EXPECT_EQ(tiny.state(), TinyState::One);
    EXPECT_EQ(tiny.payload(), std::uint8_t{42});
}

TEST(GeneratedPackedValue, GeneratesConstructionValidationAndRangeHelpers) {
    static_assert(CheckedValue::invalid_value == 0x7fffffffu);
    static_assert(CheckedValue::serial_range_fits(0, CheckedValue::serial_value_mask + 1));
    static_assert(!CheckedValue::serial_range_fits(CheckedValue::serial_value_mask, 2));

    CheckedValue const null_value;
    EXPECT_FALSE(null_value.is_valid());
    EXPECT_EQ(null_value.raw_value(), CheckedValue::invalid_value);

    auto const value{CheckedValue::make(42, DomainState::One)};
    EXPECT_TRUE(value.is_valid());
    EXPECT_EQ(value.serial(), 42u);
    EXPECT_EQ(value.state(), DomainState::One);

    CheckedValue result;
    EXPECT_TRUE(CheckedValue::try_make(7, DomainState::Zero, result));
    EXPECT_EQ(result, CheckedValue::make(7, DomainState::Zero));
    EXPECT_FALSE(CheckedValue::try_make(0x01000000u, DomainState::Zero, result));
    EXPECT_FALSE(CheckedValue::try_make(7, DomainState::COUNT, result));
    EXPECT_FALSE(CheckedValue{0xffffffffu}.is_valid());
}

TEST(GeneratedFixedSoa, Lifetimes) {
    check(FTracked::alive == 0);
    {
        TFixedRows<4> rows;
        rows.add(1, FTracked{10});
        rows.add(2, FTracked{20});
        check(rows.num() == 2);
        check(FTracked::alive == 2);
        check(rows.get_const_view().children.tracked[1].value == 20);

        auto copied{rows};
        check(copied.num() == 2);
        check(FTracked::alive == 4);
        auto const* self{&copied};
        copied = *self;
        check(copied.num() == 2 && FTracked::alive == 4);

        auto moved{std::move(copied)};
        check(copied.is_empty());
        check(moved.num() == 2 && FTracked::alive == 4);
        moved.remove_at_swap(0, 1, EAllowShrinking::No);
        check(moved.num() == 1 && FTracked::alive == 3);
        moved.reset();
        check(FTracked::alive == 2);

        bool overflow_rejected{};
        try {
            rows.add_defaulted(3);
        } catch (std::runtime_error const&) {
            overflow_rejected = true;
        }
        check(overflow_rejected);
    }
    check(FTracked::alive == 0);

    TFixedRows<0> empty;
    check(empty.is_empty() && empty.is_full());
    {
        TFixedRowsAlt<1> alternate;
        alternate.add(9, FTracked{90});
        check(alternate.get_const_view().children.tracked[0].value == 90);
    }
    check(FTracked::alive == 0);
}

TEST(GeneratedFixedSoa, AssignmentResizeAndBulkOperationsPreserveLifetimes) {
    ASSERT_EQ(FTracked::alive, 0);
    {
        TFixedRows<6> source;
        source.add(1, FTracked{10});
        source.add(2, FTracked{20});
        source.add(3, FTracked{30});

        TFixedRows<6> destination;
        destination.add(9, FTracked{90});
        EXPECT_EQ(FTracked::alive, 4);

        destination = source;
        EXPECT_EQ(FTracked::alive, 6);
        EXPECT_EQ(destination.get_const_view().children.tracked[2].value, 30);

        destination.copy_element(0, source, 2);
        destination.copy_elements(1, source, 0, 2);
        EXPECT_EQ(destination.get_const_view().children.tracked[0].value, 30);
        EXPECT_EQ(destination.get_const_view().children.tracked[1].value, 10);
        EXPECT_EQ(destination.get_const_view().children.tracked[2].value, 20);

        destination.set_num(2);
        EXPECT_EQ(FTracked::alive, 5);
        destination.set_num(4);
        EXPECT_EQ(FTracked::alive, 7);
        destination.pop();
        EXPECT_EQ(FTracked::alive, 6);

        TFixedRows<6> appended;
        appended.append_from(source.left(2));
        EXPECT_EQ(FTracked::alive, 8);
        destination = std::move(appended);
        EXPECT_EQ(FTracked::alive, 5);
        EXPECT_TRUE(appended.is_empty());
        EXPECT_EQ(destination.num(), 2);
        EXPECT_EQ(destination.get_const_view().children.tracked[1].value, 20);

        auto& alias{destination};
        destination = std::move(alias);
        EXPECT_EQ(FTracked::alive, 5);
        EXPECT_EQ(destination.num(), 2);
    }
    EXPECT_EQ(FTracked::alive, 0);
}

TEST(GeneratedSettings, DescriptorsDispatchAndAccessors) {
    auto const categories{game_setting_category_descriptors()};
    ASSERT_EQ(categories.Num(), 1);
    EXPECT_EQ(categories[0].id, EGameSettingCategory::Video);
    EXPECT_EQ(categories[0].label.ToString(), "Video");

    auto const descriptors{game_setting_descriptors()};
    ASSERT_EQ(descriptors.Num(), 3);
    EXPECT_EQ(descriptors[0].id, EGameSetting::VSync);
    EXPECT_EQ(descriptors[1].id, EGameSetting::FrameLimit);
    EXPECT_EQ(descriptors[2].id, EGameSetting::ResolutionScale);
    EXPECT_EQ(descriptors[0].tooltip.ToString(), "Synchronize presentation.");
    EXPECT_EQ(descriptors[1].apply_mode, ESettingApplyMode::Immediate);
    EXPECT_EQ(descriptors[1].control_kind, ESettingControlKind::Choice);
    EXPECT_EQ(descriptors[1].options_provider, EGameSettingOptionProvider::FrameLimits);
    EXPECT_EQ(descriptors[1].availability_provider, EGameSettingAvailabilityProvider::FrameLimit);
    EXPECT_EQ(descriptors[2].minimum, 50.0);
    EXPECT_EQ(descriptors[2].maximum, 100.0);
    EXPECT_EQ(descriptors[2].step, 0.5);
    EXPECT_EQ(&game_setting_descriptor(EGameSetting::FrameLimit), &descriptors[1]);

    FSettingsState state{.vsync = false, .frame_limit = 60.0f, .resolution_scale = 75.0f};
    EXPECT_FALSE(std::get<bool>(game_setting_value(state, EGameSetting::VSync)));
    EXPECT_EQ(std::get<float>(game_setting_value(state, EGameSetting::FrameLimit)), 60.0f);
    EXPECT_TRUE(
        set_game_setting_value(state, EGameSetting::ResolutionScale, FGameSettingValue{80.0f}));
    EXPECT_EQ(state.resolution_scale, 80.0f);

    auto const unchanged{state};
    EXPECT_FALSE(set_game_setting_value(state, EGameSetting::VSync, FGameSettingValue{120.0f}));
    EXPECT_EQ(state, unchanged);

    FSettingsAccessFixture access;
    access.state.frame_limit = 30.0f;
    EXPECT_EQ(access.frame_limit(), 30.0f);
    access.set_vsync(true);
    EXPECT_TRUE(access.state.vsync);
    EXPECT_EQ(access.last_setting, EGameSetting::VSync);
}

TEST(GeneratedVector, Operations) {
    FVectors1f scalar;
    scalar.add(FScalar1f{4.0f});
    check(scalar.at(0).X == 4.0f);

    FVectors2f vector2;
    vector2.add(FVector2f{7.0f, 8.0f});
    check(vector2.at(0).X == 7.0f && vector2.at(0).Y == 8.0f);

    FVectors3f vectors;
    vectors.add(3.0f, 30.0f, 300.0f);
    vectors.add(FVector3f{1.0f, 10.0f, 100.0f});
    vectors.add(FVector3f{2.0f, 20.0f, 200.0f});
    TArray<int32> scratch;
    scratch.AddUninitialized(vectors.num());
    vectors.sort([](auto const& rows,
                    int32 const lhs,
                    int32 const rhs) { return rows.xs[lhs] < rows.xs[rhs]; },
                 scratch);
    check(vectors.at(0).X == 1.0f && vectors.at(0).Z == 100.0f);
    check(vectors.at(2).X == 3.0f && vectors.at(2).Y == 30.0f);

    TFixedVectors3<3> fixed;
    fixed.add(1.0f, 2.0f, 3.0f);
    fixed.add(4.0f, 5.0f, 6.0f);
    fixed.set(0, FVector3f{7.0f, 8.0f, 9.0f});
    check(fixed.at(0).X == 7.0f && fixed.at(0).Z == 9.0f);
    auto copied{fixed};
    check(copied.at(1).X == 4.0f && copied.at(1).Z == 6.0f);
    copied.remove_at_swap(0, 1, EAllowShrinking::No);
    check(copied.num() == 1 && copied.at(0).X == 4.0f);
}

TEST(GeneratedFacade, Operations) {
    FTarget target;
    target.value = 5;

    FInlineFacade inline_facade;
    bool unbound_rejected{};
    try {
        static_cast<void>(inline_facade.get());
    } catch (std::runtime_error const&) {
        unbound_rejected = true;
    }
    check(unbound_rejected);
    inline_facade.bind(target);
    check(inline_facade.get() == 5);
    check(inline_facade.get(3) == 8);
    inline_facade.reset();
    check(target.value == 0);

    FSourceFacade source_facade;
    source_facade.bind(target);
    source_facade.set_value(42);
    check(source_facade.get() == 42);

    FReferenceFacade reference_facade{target};
    reference_facade.set_value(17);
    check(reference_facade.get() == 17);
}

TEST(GeneratedEnum, ValuesAndConversions) {
    static_assert(TEnumTraits<EPlainFixture>::count == 2);
    static_assert(TEnumArray<EPlainFixture, float>::size() == 2);

    TEnumArray<EPlainFixture, float> radii;
    radii[EPlainFixture::First] = 100.0f;
    radii[EPlainFixture::ReadableName] = 200.0f;
    check(radii[EPlainFixture::First] == 100.0f);
    check(radii[EPlainFixture::ReadableName] == 200.0f);

    static_assert(TEnumTraits<EReflectedFixture>::count == 1);
    static_assert(TEnumArray<EReflectedFixture, int32>::size() == 1);
    TEnumArray<EReflectedFixture, int32> reflected_values;
    reflected_values[EReflectedFixture::Visible] = 42;
    check(reflected_values[EReflectedFixture::Visible] == 42);

    auto const& const_radii{radii};
    check(const_radii[EPlainFixture::First] == 100.0f);

    check(std::string_view{LexToString(EPlainFixture::First)} == "First");
    check(to_string_view(EPlainFixture::ReadableName) == "ReadableName");
    check(to_string(EPlainFixture::ReadableName) == "ReadableName");
    check(std::string_view{LexToDisplayString(EPlainFixture::ReadableName)} == "Readable Name");
    check(to_display_string_view(EPlainFixture::ReadableName) == "Readable Name");
    check(to_display_string(EPlainFixture::First) == "First");
    check(std::string_view{LexToSerializedString(EPlainFixture::ReadableName)} == "readable_name");
    auto parsed{EPlainFixture::First};
    check(try_parse_serialized(TEXT("readable_name"), parsed));
    check(parsed == EPlainFixture::ReadableName);
    check(!try_parse_serialized(TEXT("missing"), parsed));
    check(parsed == EPlainFixture::ReadableName);
    check(std::string_view{LexToSerializedString(static_cast<EPlainFixture>(99))} ==
          "<invalid EPlainFixture>");

    check(std::string_view{LexToString(EReflectedFixture::Visible)} == "Visible");
    check(to_display_string_view(EReflectedFixture::Visible) == "Visible Value");
    check(std::string_view{LexToString(static_cast<EReflectedFixture>(99))} ==
          "<invalid EReflectedFixture>");
}

TEST(GeneratedStaticTable, Operations) {
    static_assert(FStaticTableFixture::num() == 3);
    static_assert(FStaticTableFixture::first_index == 0);
    static_assert(FStaticTableFixture::third_index == 2);

    FStaticTableFixture values{
        .ids = {10, 20, 30},
        .weights = {1.0f, 2.0f, 3.0f},
    };
    values.apply_arrays([](auto& ids, auto& weights) {
        ids[FStaticTableFixture::second_index] = {};
        weights[FStaticTableFixture::second_index] = {};
    });
    check(values.ids[0] == 10 && values.ids[1] == 0);
    check(values.weights[1] == 0.0f && values.weights[2] == 3.0f);

    FStaticTableFixture other{};
    values.apply_array_pairs(other,
                             [](auto const& source_ids,
                                auto& destination_ids,
                                auto const& source_weights,
                                auto& destination_weights) {
                                 destination_ids[FStaticTableFixture::third_index] =
                                     source_ids[FStaticTableFixture::third_index];
                                 destination_weights[FStaticTableFixture::third_index] =
                                     source_weights[FStaticTableFixture::third_index];
                             });
    check(other.ids[2] == 30 && other.weights[2] == 3.0f);

    auto const& const_values{values};
    int visited{};
    const_values.apply_arrays(
        [&visited]<typename... Columns>(Columns const&...) { visited = sizeof...(Columns); });
    check(visited == 2);
    auto const row{const_values.get_row(FStaticTableFixture::third_index)};
    check(row.id == 30 && row.weight == 3.0f);

    FStaticGroupFixture groups{};
    groups.min_xs[FStaticGroupFixture::second_index] = 1.0f;
    groups.min_ys[FStaticGroupFixture::second_index] = 2.0f;
    groups.min_zs[FStaticGroupFixture::second_index] = 3.0f;
    groups.max_xs[FStaticGroupFixture::second_index] = 4.0f;
    groups.max_ys[FStaticGroupFixture::second_index] = 5.0f;
    groups.max_zs[FStaticGroupFixture::second_index] = 6.0f;

    auto const& const_groups{groups};
    auto const min_point{const_groups.get_min_point(FStaticGroupFixture::second_index)};
    auto const max_point{const_groups.get_max_point(FStaticGroupFixture::second_index)};
    check(min_point.X == 1.0f && min_point.Y == 2.0f && min_point.Z == 3.0f);
    check(max_point.X == 4.0f && max_point.Y == 5.0f && max_point.Z == 6.0f);
}

TEST(GeneratedSingleAllocationSoa, LayoutAndAccess) {
    using SingleParents = codegen_compile_fixture::SingleParents;
    static_assert(sizeof(SingleParents::View) == 16);
    static_assert(!std::is_copy_constructible_v<SingleParents>);
    using KeysColumn = std::remove_cvref_t<decltype(SingleParents::Keys)>;
    static_assert(std::is_same_v<KeysColumn::pointer, int32*>);
    static_assert(std::is_same_v<KeysColumn::const_pointer, int32 const*>);
    static_assert(SingleParents::Keys.offset(1) == 0);
    static_assert(SingleParents::ChildrenValues.offset(1) == 64 * sizeof(int32) + 192);
    static_assert(SingleParents::ChildrenValues.capacity_granularity == 64);
    static_assert(SingleParents::ChildrenValues.column_gap == 192);
    static_assert(SingleParents::layout_bytes(1) == 128 * sizeof(int32) + 192);

    SingleParents parents;
    check(parents.get_view().columns().keys.GetData() == nullptr);
    parents.add_defaulted(65);
    parents.get_view().columns().children.values[64] = 37;
    parents.reserve(129);
    check(parents.capacity() == 192);
    check(parents.get_const_view().view_children().values[64] == 37);

    SingleParents moved{std::move(parents)};
    check(parents.capacity() == 0);
    moved.remove_at_swap(0, 1);
    check(moved.get_view().columns().children.values[0] == 37);
}

} // namespace
