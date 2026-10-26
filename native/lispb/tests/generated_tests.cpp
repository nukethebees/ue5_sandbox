#include "Generated.h"
#include "Records.h"
#include "ScalarValues.h"
#include "SignatureDefinitions.h"
#include <ioj/enum_array.h>
#include <ioj/range_fits.h>

#include <gtest/gtest.h>

#include <array>
#include <compare>
#include <cstdint>
#include <format>
#include <functional>
#include <limits>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace codegen_compile_fixture {

auto first::A::make() -> second::B {
    return {};
}
void first::A::accept(second::B value) {
    (void)value;
}
auto first::A::pointer() -> second::B* {
    return nullptr;
}
auto first::A::ref(second::B const& value) -> second::B const& {
    return value;
}
auto first::A::move(second::B&& value) -> second::B&& {
    return std::move(value);
}
auto first::A::payload() -> second::Payload {
    return {};
}
auto second::B::make() -> first::A {
    return {};
}
auto second::B::pointer() -> first::A* {
    return nullptr;
}
auto second::B::ref(first::A const& value) -> first::A const& {
    return value;
}

auto mixed::DefaultArguments::value(DefaultValue value) -> int {
    return value.value;
}
auto mixed::DefaultArguments::reference(DefaultReference const& value) -> int {
    return value.value;
}
auto mixed::DefaultArguments::pointer(DefaultPointer* value) -> bool {
    return value == nullptr;
}
void mixed::DefaultArguments::cross(second::B value) {
    (void)value;
}

auto mixed::MethodConsumer::make() -> mixed::MethodProvider {
    return {};
}
auto mixed::MethodProvider::consumer() -> mixed::MethodConsumer {
    return {};
}

auto mixed::ValueDefaults::valid() const noexcept -> bool {
    return zero == 0 && seven == 7;
}

} // namespace codegen_compile_fixture

namespace {

using namespace codegen_compile_fixture;

TEST(GeneratedRecords, PreserveAggregateDefaultsComparisonsAndMemberApi) {
    static_assert(std::is_aggregate_v<mixed::ValueDefaults>);
    static_assert(std::is_standard_layout_v<mixed::ValueDefaults>);
    static_assert(std::is_trivially_copyable_v<mixed::ValueDefaults>);
    constexpr mixed::SpanValue span{.offset = 4, .count = 3};
    static_assert(span.end() == 7);
    static_assert(noexcept(span.end()));
    static_assert(span > mixed::SpanValue{});
    mixed::ValueDefaults value{.uninitialized = 1};
    EXPECT_TRUE(value.valid());
    EXPECT_EQ(value, (mixed::ValueDefaults{1, 0, 7}));
    static_assert(std::is_trivially_default_constructible_v<mixed::Uninitialized>);
    static_assert(!std::is_trivially_default_constructible_v<mixed::ValueDefaults>);

    static_assert(std::is_aggregate_v<ApiPair>);
    static_assert(std::is_standard_layout_v<ApiPair>);
    static_assert(std::is_trivially_copyable_v<ApiPair>);
    static_assert(sizeof(ApiPair) == 2 * sizeof(float));
    constexpr auto pair{[] {
        ApiPair result{1.f, 2.f};
        ApiPair::visit_field(result, ApiPair::Field::y, []<auto field>(auto& member) {
            static_assert(std::same_as<decltype(member), float&>);
            member = 7.f;
        });
        return result;
    }()};
    static_assert(pair.x == 1.f && pair.y == 7.f);
    float total{};
    int visits{};
    for (auto field : {ApiPair::Field::x, ApiPair::Field::y}) {
        ApiPair::visit_field(pair, field, [&]<auto selected>(auto& member) {
            static_assert(std::same_as<decltype(member), float const&>);
            EXPECT_EQ(selected, field);
            total += member;
            ++visits;
        });
    }
    EXPECT_EQ(total, 8.f);
    EXPECT_EQ(visits, 2);

    WeaponSettings weapon;
    WeaponSettings::visit_field(
        weapon, WeaponSettings::Field::asset_distance, []<auto field>(auto& member) {
            if constexpr (field == WeaponSettings::Field::asset_distance) {
                member = 42.f;
            } else {
                ADD_FAILURE() << "Visited the wrong member";
            }
        });
    EXPECT_EQ(weapon.asset_distance, 42.f);
}

TEST(GeneratedRecords, NamespaceShorteningPreservesTypeLookup) {
    mixed::NamespaceLookup values{};
    static_assert(std::is_same_v<decltype(values.local), std::array<mixed::Position, 2>>);
    static_assert(std::is_same_v<decltype(values.parent), ApiPair>);
    static_assert(std::is_same_v<decltype(values.sibling), second::B>);
    mixed::HiddenLookup hidden;
    static_assert(std::is_same_v<decltype(hidden.token), mixed::Position>);
    EXPECT_EQ(hidden.Position, 7);
    EXPECT_EQ(mixed::ParameterLookup::select(0, {.x = 3.f}).x, 3.f);
}

TEST(GeneratedRecords, MethodSignaturesCompileWithForwardAndCompleteDependencies) {
    mixed::MethodConsumer consumer;
    EXPECT_TRUE((std::is_same_v<decltype(consumer.make().consumer()), mixed::MethodConsumer>));
    static_assert(std::is_same_v<decltype(consumer.mode()), mixed::Kind>);
    static_assert(std::is_same_v<decltype(consumer.raw()), mixed::Payload>);
    static_assert(std::is_same_v<decltype(consumer.count()), mixed::MethodCount const&>);
    static_assert(std::is_same_v<decltype(consumer.ref()), mixed::MethodProvider const&>);
    mixed::ValueFactory factory;
    auto provider{factory.make()};
    factory.accept({});
    mixed::PointerFactory pointers;
    EXPECT_EQ(pointers.pointer(), nullptr);
    EXPECT_EQ(&pointers.ref(provider), &provider);
    static_assert(
        std::is_same_v<decltype(std::declval<mixed::CrossModuleReader>().make()), ApiPair>);
    static_assert(
        std::is_same_v<decltype(std::declval<mixed::CrossModuleReader>().pointer()), ApiPair*>);
    static_assert(mixed::PointerFactory::empty_pointer() == nullptr);
}

TEST(GeneratedRecords, ParameterDefaultsConstructCompleteTypesAtDeclarationSite) {
    mixed::DefaultArguments arguments;
    EXPECT_EQ(arguments.value(), 7);
    EXPECT_EQ(arguments.reference(), 9);
    EXPECT_TRUE(arguments.pointer());
    arguments.cross();
}

TEST(GeneratedRecords, CrossModuleSignaturesSupportOutOfLineDefinitions) {
    first::A a;
    second::B b;
    static_assert(std::is_same_v<decltype(a.make()), second::B>);
    static_assert(std::is_same_v<decltype(b.make()), first::A>);
    static_assert(std::is_same_v<decltype(a.move(std::declval<second::B&&>())), second::B&&>);
    a.accept(a.make());
    b.make().accept(b);
    EXPECT_EQ(a.pointer(), nullptr);
    EXPECT_EQ(b.pointer(), nullptr);
    EXPECT_EQ(&a.ref(b), &b);
    EXPECT_EQ(&b.ref(a), &a);
    auto&& moved{a.move(static_cast<second::B&&>(b))};
    EXPECT_EQ(&moved, &b);
    EXPECT_EQ(a.payload().value, 0);
    CrossModuleFactory factory;
    factory.accept(factory.make());
    factory.make_second().make().accept(b);
    static_assert(std::is_same_v<decltype(factory.count()), StandardInt>);
    static_assert(std::is_same_v<decltype(factory.tag()), mixed::Kind>);
}

TEST(GeneratedScalar, UsesConfiguredStorageAndTypedValues) {
    static_assert(std::is_same_v<Health::storage_type, std::uint16_t>);
    static_assert(std::is_same_v<RegisteredInt::storage_type, std::uint32_t>);
    static_assert(std::is_same_v<StandardInt::storage_type, std::uint32_t>);
    static_assert(std::is_signed_v<RegisteredInt::value_type>);
    static_assert(std::is_trivially_copyable_v<Health>);
    static_assert(!std::is_convertible_v<int, Health>);
    Health const health{1000};
    EXPECT_EQ(health.value(), 1000);
    EXPECT_EQ(RegisteredInt{-100}.value(), -100);
    EXPECT_EQ(StandardInt{100}.value(), 100);
}

TEST(GeneratedPackedDefaults, ConstructsDefaultsAndSupportsPartialRawConstruction) {
    static_assert(std::is_default_constructible_v<Defaults>);
    static_assert(!std::is_default_constructible_v<PartialDefaults>);
    constexpr Defaults value{};
    static_assert(value.low() == 5 && value.delta() == -3);
    EXPECT_EQ(value.raw_value(), 5U | (29U << 5));
    constexpr DefaultsMsb msb{};
    static_assert(msb.low() == 5 && msb.delta() == -3);
    EXPECT_EQ(msb.raw_value(), (5U << 13) | (29U << 6));
    constexpr auto raw_partial{PartialDefaults::from_raw(0xff)};
    static_assert(raw_partial.raw_value() == 0xff);
    static_assert(raw_partial.first() == 3 && raw_partial.gap() == 3 && raw_partial.last() == 3);
    PartialDefaults const partial{2, 1, 0};
    EXPECT_EQ(partial.first(), 2);
    EXPECT_EQ(partial.gap(), 1);
    EXPECT_EQ(partial.last(), 0);
    EXPECT_EQ((PartialDefaults{1, 2, 3}).last(), 3);
}

TEST(GeneratedMixedModule, ResolvesForwardPhysicalDependenciesAcrossDeclarationKinds) {
    mixed::Event const event{
        .tag = mixed::Kind::Data,
        .payload = {.data = {.payload = {.position = {.x = 3.0f, .y = 4.0f}}}}};
    EXPECT_EQ(event.tag, mixed::Kind::Data);
    auto const& position{event.payload.data.payload.position};
    EXPECT_EQ(position.x, 3.0f);
    EXPECT_EQ(position.y, 4.0f);
}

template <typename Mask, typename Value>
concept CanSetMask = requires(Mask& mask, Value const& value) { mask.set(value); };

TEST(GeneratedFieldMask, Operations) {
    static_assert(CanSetMask<FFieldMask8, FFieldMask8>);
    static_assert(!CanSetMask<FFieldMask8, FFieldMask9>);
    static_assert(!CanSetMask<FFieldMask8, EField9>);
    static_assert(!CanSetMask<FFieldMask8 const, FFieldMask8>);
    static_assert(!CanSetMask<FFieldMask8 const, EField8>);
    static_assert(std::is_standard_layout_v<FFieldMask8>);
    static_assert(!std::is_convertible_v<std::uint8_t, FFieldMask8>);
    constexpr FFieldMask8 raw{std::uint8_t{0x80}};
    static_assert(raw.has(FFieldMask8::values_field(7)));
    static_assert(FFieldMask8::field_count == 8);
    static_assert(sizeof(FFieldMask8) == sizeof(std::uint8_t));
    static_assert(FFieldMask9::field_count == 9);
    static_assert(sizeof(FFieldMask9) == sizeof(std::uint16_t));
    static_assert(sizeof(FFieldMask16) == sizeof(std::uint16_t));
    static_assert(sizeof(FFieldMask17) == sizeof(std::uint32_t));
    static_assert(sizeof(FFieldMask32) == sizeof(std::uint32_t));
    static_assert(sizeof(FFieldMask33) == sizeof(std::uint64_t));

    constexpr auto last_value{FFieldMask8::values_field(7)};
    static_assert(FFieldMask8::index(last_value) == 7);

    FFieldMask8 mask;
    EXPECT_TRUE(mask.is_empty());
    mask.set(last_value);
    EXPECT_TRUE(mask.has(last_value) && mask.value() == std::uint8_t{0x80});
    mask.clear(last_value);
    EXPECT_TRUE(mask.is_empty());

    FFieldMask9 wide;
    wide.set(EField9::Tail);
    EXPECT_TRUE(wide.has(EField9::Tail) && wide.value() == std::uint16_t{0x100});
    FFieldMask9 merged;
    merged.set(wide);
    EXPECT_TRUE(merged.has(EField9::Tail));

    FFieldMask16 mask16;
    auto const high16{FFieldMask16::values_field(15)};
    mask16.set(high16);
    EXPECT_EQ(mask16.value(), std::uint16_t{0x8000});
    EXPECT_EQ(FFieldMask16::index(high16), 15);

    FFieldMask17 mask17;
    auto const high17{FFieldMask17::values_field(16)};
    mask17.set(high17);
    EXPECT_EQ(mask17.value(), std::uint32_t{0x10000});
    EXPECT_EQ(FFieldMask17::index(high17), 16);

    FFieldMask32 mask32;
    auto const high32{FFieldMask32::values_field(31)};
    mask32.set(high32);
    EXPECT_EQ(mask32.value(), std::uint32_t{0x80000000});
    EXPECT_EQ(FFieldMask32::index(high32), 31);

    FFieldMask33 mask33;
    auto const high33{FFieldMask33::values_field(32)};
    mask33.set(high33);
    EXPECT_EQ(mask33.value(), std::uint64_t{0x100000000});
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

TEST(GeneratedPackedValue, ConstructorsDistinguishFieldValuesFromRawStorage) {
    constexpr PackedSingle value{std::uint8_t{3}};
    constexpr auto raw{PackedSingle::from_raw(std::uint8_t{3})};
    static_assert(value.raw_value() == 0x30 && value.value() == 3);
    static_assert(raw.raw_value() == 3 && raw.value() == 0);
    static_assert(PackedSingle{}.raw_value() == 0);
    static_assert(sizeof(PackedSingle) == sizeof(std::uint8_t));
    static_assert(std::is_trivially_copyable_v<PackedSingle>);
    static_assert(std::is_standard_layout_v<PackedSingle>);
    static_assert(!std::is_convertible_v<std::uint8_t, PackedSingle>);
    static_assert(std::is_same_v<PackedSingle::value_type, std::uint8_t>);
    static_assert(std::is_same_v<Vitals::storage_type, std::uint16_t>);
    static_assert(std::is_same_v<FighterState::storage_type, std::uint32_t>);
    static_assert(std::is_same_v<PackedWide::storage_type, std::uint64_t>);
    static_assert(FighterState{42, PackedState::AB}.raw_value() == 0xab00002a);
    static_assert(SignedWide{std::numeric_limits<std::int64_t>::min()}.delta() ==
                  std::numeric_limits<std::int64_t>::min());
    static_assert(CheckedValue{42, DomainState::One}.serial() == 42);
#ifndef NDEBUG
    EXPECT_DEATH(static_cast<void>(PackedSingle{std::uint8_t{16}}), "");
    EXPECT_DEATH(static_cast<void>(SignedDelta{-65'537}), "");
#endif
}

TEST(GeneratedPackedValue, CombinesAndExtractsFields) {
    FighterState value;
    value.set_entity_index(0x123456u);
    value.set_state(PackedState::AB);
    EXPECT_EQ(value.raw_value(), 0xAB123456u);

    auto const from_raw{FighterState::from_raw(0xAB123456u)};
    EXPECT_EQ(from_raw.entity_index(), 0x123456u);
    EXPECT_EQ(from_raw.state(), PackedState::AB);
}

TEST(GeneratedPackedValue, SettersPreserveOtherFieldsAndRejectOverflow) {
    auto value{FighterState::from_raw(0xAB123456u)};
    value.set_entity_index(0xffffffu);
    EXPECT_EQ(value.raw_value(), 0xABffffffu);
    value.set_state(PackedState::Zero);
    EXPECT_EQ(value.raw_value(), 0x00ffffffu);
    value.set_state(PackedState::Max);
    EXPECT_EQ(value.raw_value(), 0xffffffffu);

#ifndef NDEBUG
    EXPECT_DEATH(value.set_entity_index(0x01000000u), "");
#endif
}

TEST(GeneratedPackedValue, ComparesByRawValue) {
    auto const first{FighterState::from_raw(1u)};
    auto const second{FighterState::from_raw(2u)};
    EXPECT_LT(first, second);
    EXPECT_LE(first, second);
    EXPECT_GT(second, first);
    EXPECT_GE(second, first);
    EXPECT_EQ(second, FighterState::from_raw(2u));
    EXPECT_NE(first, second);
}

TEST(GeneratedPackedValue, ExhaustivelyRoundTripsUint8Storage) {
    for (unsigned raw{}; raw <= 0xffu; ++raw) {
        auto const packed{PackedByte::from_raw(static_cast<std::uint8_t>(raw))};
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

TEST(GeneratedPackedValue, RoundTripsMostSignificantFirstFields) {
    static_assert(NetworkHeader::version_field::offset == 28);
    static_assert(NetworkHeader::kind_field::offset == 20);
    static_assert(NetworkHeader::length_field::offset == 0);

    NetworkHeader header;
    header.set_version(std::uint8_t{0xa});
    header.set_kind(std::uint8_t{0xbc});
    header.set_length(std::uint16_t{0x1234});
    EXPECT_EQ(header.raw_value(), std::uint32_t{0xabc01234});

    auto const from_raw{NetworkHeader::from_raw(std::uint32_t{0x5a70beef})};
    EXPECT_EQ(from_raw.version(), std::uint8_t{0x5});
    EXPECT_EQ(from_raw.kind(), std::uint8_t{0xa7});
    EXPECT_EQ(from_raw.length(), std::uint16_t{0xbeef});
}

TEST(GeneratedPackedValue, HandlesFullWidthStorageAndNarrowEnums) {
    auto const maximum{std::numeric_limits<std::uint64_t>::max()};
    static_assert(PackedWide::value_field::bits == 64);
    PackedWide wide;
    wide.set_value(maximum);
    EXPECT_EQ(wide.value(), maximum);
    EXPECT_EQ(wide.raw_value(), maximum);
    EXPECT_EQ(PackedWide::from_raw(std::uint64_t{0x123456789abcdef0}).value(),
              std::uint64_t{0x123456789abcdef0});

    PackedTinyState tiny;
    tiny.set_state(TinyState::One);
    tiny.set_payload(std::uint8_t{42});
    EXPECT_EQ(tiny.raw_value(), std::uint8_t{0xa9});

#ifndef NDEBUG
    EXPECT_DEATH(tiny.set_state(static_cast<TinyState>(4)), "");
#endif
    EXPECT_EQ(tiny.state(), TinyState::One);
    EXPECT_EQ(tiny.payload(), std::uint8_t{42});
}

TEST(GeneratedPackedValue, RoundTripsSignedArbitraryWidthBoundaries) {
    static_assert(SignedWide::delta_field::bits == 64);
    SignedDelta delta;
    delta.set_delta(-65'536);
    EXPECT_EQ(delta.delta(), -65'536);
    EXPECT_EQ(delta.raw_value(), std::uint32_t{0x10000});
    delta.set_delta(-1);
    EXPECT_EQ(delta.delta(), -1);
    EXPECT_EQ(delta.raw_value(), std::uint32_t{0x1ffff});
    delta.set_delta(65'535);
    EXPECT_EQ(delta.delta(), 65'535);
    EXPECT_EQ(delta.raw_value(), std::uint32_t{0xffff});

#ifndef NDEBUG
    EXPECT_DEATH(delta.set_delta(-65'537), "");
    EXPECT_DEATH(delta.set_delta(65'536), "");
#endif
    EXPECT_EQ(SignedDelta::from_raw(std::uint32_t{0x1ffff}).delta(), -1);
    EXPECT_EQ(SignedDelta::from_raw(std::uint32_t{0x10000}).delta(), -65'536);

    SignedWide wide;
    wide.set_delta(std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(wide.delta(), std::numeric_limits<std::int64_t>::min());
    wide.set_delta(std::numeric_limits<std::int64_t>::max());
    EXPECT_EQ(wide.delta(), std::numeric_limits<std::int64_t>::max());
    EXPECT_EQ(SignedWide::from_raw(std::numeric_limits<std::uint64_t>::max()).delta(), -1);
}

TEST(GeneratedPackedValue, EnforcesSignedSemanticRangeAndSentinel) {
    static_assert(SignedTemperature::temperature_field::bits == 8);
    static_assert(SignedTemperature::temperature_Freezing == 0);
    static_assert(SignedTemperature::temperature_Unknown == -128);

    SignedTemperature temperature;
    temperature.set_temperature(-100);
    EXPECT_EQ(temperature.temperature(), -100);
    EXPECT_TRUE(temperature.is_valid());
    temperature.set_temperature(100);
    EXPECT_EQ(temperature.temperature(), 100);
    temperature.set_temperature(SignedTemperature::temperature_Unknown);
    EXPECT_EQ(temperature.temperature(), -128);
    EXPECT_TRUE(temperature.is_valid());

#ifndef NDEBUG
    EXPECT_DEATH(temperature.set_temperature(-101), "");
    EXPECT_DEATH(temperature.set_temperature(101), "");
#endif
    EXPECT_FALSE(SignedTemperature::from_raw(std::uint32_t{0x9b}).is_valid());
}

TEST(GeneratedPackedValue, RoundTripsLinearQuantizedEncodedCodes) {
    static_assert(Vitals::health_encoded_field::bits == 8);

    Vitals value;
    value.set_health_encoded(253);
    value.set_state(0xab);
    EXPECT_EQ(value.health_encoded(), 253);
    EXPECT_EQ(value.state(), 0xab);
    EXPECT_EQ(value.raw_value(), 0xabfdu);
    EXPECT_TRUE(value.is_valid());

#ifndef NDEBUG
    EXPECT_DEATH(value.set_health_encoded(254), "");
#endif
    EXPECT_FALSE(Vitals::from_raw(0x00feu).is_valid());

    Vitals const made{42, 7};
    EXPECT_EQ(made.health_encoded(), 42);
    EXPECT_EQ(made.state(), 7);
}

TEST(GeneratedPackedValue, RoundTripsFixedPointRawCodes) {
    static_assert(Motion::velocity_raw_field::bits == 12);
    static_assert(Motion::velocity_minimum_allowed_raw == -2048);
    static_assert(Motion::velocity_maximum_allowed_raw == 2047);
    static_assert(Motion::fraction_raw_field::bits == 7);
    static_assert(Motion::fraction_minimum_allowed_raw == 0);
    static_assert(Motion::fraction_maximum_allowed_raw == 127);

    Motion value;
    value.set_velocity_raw(-2048);
    value.set_fraction_raw(127);
    value.set_state(0x1abc);
    EXPECT_EQ(value.velocity_raw(), -2048);
    EXPECT_EQ(value.fraction_raw(), 127);
    EXPECT_EQ(value.state(), 0x1abc);
    EXPECT_EQ(value.raw_value(), 0xd5e7f800u);
    EXPECT_EQ(value.velocity_value(), -128.0);
    EXPECT_EQ(value.fraction_value(), 127.0 / 128.0);

    Motion::velocity_raw_type converted{};
    EXPECT_TRUE(Motion::try_encode_velocity_value(1.5, converted));
    EXPECT_EQ(converted, 24);
    EXPECT_TRUE(Motion::try_encode_velocity_value(0.03125, converted));
    EXPECT_EQ(converted, 0);
    EXPECT_TRUE(Motion::try_encode_velocity_value(0.09375, converted));
    EXPECT_EQ(converted, 2);
    EXPECT_FALSE(
        Motion::try_encode_velocity_value((std::numeric_limits<double>::infinity)(), converted));
    EXPECT_FALSE(Motion::try_encode_velocity_value(128.0, converted));
    value.set_velocity_value(-1.5);
    EXPECT_EQ(value.velocity_raw(), -24);

#ifndef NDEBUG
    EXPECT_DEATH(value.set_velocity_raw(-2049), "");
    EXPECT_DEATH(value.set_velocity_raw(2048), "");
    EXPECT_DEATH(value.set_fraction_raw(128), "");
#endif

    Motion const made{42, 64, 7};
    EXPECT_EQ(made.velocity_raw(), 42);
    EXPECT_EQ(made.fraction_raw(), 64);
    EXPECT_EQ(made.state(), 7);
}

TEST(GeneratedPackedValue, RoundTripsMiniFloatEncodedCodes) {
    static_assert(PackedMiniFloat::component_encoded_field::bits == 12);

    PackedMiniFloat value;
    value.set_component_encoded(0xfff);
    value.set_state(0xa);
    EXPECT_EQ(value.component_encoded(), 0xfff);
    EXPECT_EQ(value.state(), 0xa);
    EXPECT_EQ(value.raw_value(), 0xafff);

#ifndef NDEBUG
    EXPECT_DEATH(value.set_component_encoded(0x1000), "");
#endif

    PackedMiniFloat const made{0x7c0, 3};
    EXPECT_EQ(made.component_encoded(), 0x7c0);
    EXPECT_EQ(made.state(), 3);
}

TEST(GeneratedPackedValue, GeneratesConstructionValidationAndScalarRanges) {
    static_assert(CheckedValue::invalid_value == 0x7fffffffu);
    static_assert(CheckedValue::serial_range.min == 0);
    static_assert(CheckedValue::serial_range.max == CheckedValue::serial_field::value_mask);
    static_assert(ioj::range_fits(0u,
                                  CheckedValue::serial_range.max + 1,
                                  CheckedValue::serial_range.min,
                                  CheckedValue::serial_range.max));
    static_assert(!ioj::range_fits(CheckedValue::serial_range.max,
                                   2u,
                                   CheckedValue::serial_range.min,
                                   CheckedValue::serial_range.max));
    static_assert(ioj::range_fits(10u, 11u, 10u, 20u));
    static_assert(!ioj::range_fits(9u, 1u, 10u, 20u));
    static_assert(!ioj::range_fits(10u, 12u, 10u, 20u));
    static_assert(ioj::range_fits(0xffffffffu, 0u, 10u, 20u));
    static_assert(ioj::range_fits(0xffffffffu, 1u, 0u, 0xffffffffu));
    static_assert(!ioj::range_fits(0xffffffffu, 2u, 0u, 0xffffffffu));

    CheckedValue const null_value;
    EXPECT_FALSE(null_value.is_valid());
    EXPECT_EQ(null_value.raw_value(), CheckedValue::invalid_value);

    auto const value{CheckedValue(42, DomainState::One)};
    EXPECT_TRUE(value.is_valid());
    EXPECT_EQ(value.serial(), 42u);
    EXPECT_EQ(value.state(), DomainState::One);

    auto const zero{CheckedValue(7, DomainState::Zero)};
    EXPECT_EQ(zero.serial(), 7u);
    EXPECT_EQ(zero.state(), DomainState::Zero);
    EXPECT_EQ(zero.raw_value(), 7u);
    EXPECT_FALSE(CheckedValue::from_raw(0xffffffffu).is_valid());
}

TEST(GeneratedFacade, Operations) {
    FTarget target;
    target.value = 5;

    FInlineFacade inline_facade;
#ifndef NDEBUG
    EXPECT_DEATH(static_cast<void>(inline_facade.get()), "");
#endif
    inline_facade.bind(target);
    EXPECT_TRUE(inline_facade.get() == 5);
    EXPECT_TRUE(inline_facade.get(3) == 8);
    inline_facade.reset();
    EXPECT_TRUE(target.value == 0);

    FSourceFacade source_facade;
    source_facade.bind(target);
    source_facade.set_value(42);
    EXPECT_TRUE(source_facade.get() == 42);

    FReferenceFacade reference_facade{target};
    reference_facade.set_value(17);
    EXPECT_TRUE(reference_facade.get() == 17);
}

TEST(GeneratedEnum, ValuesAndConversions) {
    static_assert(ioj::enum_count<EPlainFixture>() == 2);
    static_assert(ioj::EnumArray<EPlainFixture, float>::size() == 2);

    ioj::EnumArray<EPlainFixture, float> radii;
    radii[EPlainFixture::First] = 100.0f;
    radii[EPlainFixture::ReadableName] = 200.0f;
    EXPECT_EQ(radii[EPlainFixture::First], 100.0f);
    EXPECT_EQ(std::as_const(radii)[EPlainFixture::ReadableName], 200.0f);

    EXPECT_EQ(to_string(EPlainFixture::First), "First");
    EXPECT_EQ(to_string(EPlainFixture::ReadableName), "ReadableName");
    EXPECT_EQ(to_display_string(EPlainFixture::ReadableName), "Readable Name");
    EXPECT_EQ(to_display_string(EPlainFixture::First), "First");
    EXPECT_EQ(to_serialized_string(EPlainFixture::ReadableName), "readable_name");
    EXPECT_EQ(try_parse_plain_fixture("First"), EPlainFixture::First);
    EXPECT_EQ(try_parse_serialized_plain_fixture("readable_name"), EPlainFixture::ReadableName);
    EXPECT_FALSE(try_parse_serialized_plain_fixture("missing"));
    EXPECT_FALSE(try_parse_plain_fixture("first"));
    static_assert(try_parse_plain_fixture("ReadableName") == EPlainFixture::ReadableName);
    static_assert(!try_parse_serialized_plain_fixture("missing"));
    EXPECT_EQ(try_parse_name_only_fixture("First"), ENameOnlyFixture::First);
    EXPECT_EQ(try_parse_serialized_name_only_fixture("second"), ENameOnlyFixture::Second);
    EXPECT_FALSE(try_parse_name_only_fixture("missing"));
    EXPECT_EQ(try_parse_serialized_reordered_fixture("first"), EReorderedFixture::First);
    EXPECT_EQ(try_parse_serialized_reordered_fixture("later"), EReorderedFixture::Later);
    EXPECT_FALSE(try_parse_serialized_reordered_fixture("missing"));
}

#ifndef NDEBUG
TEST(GeneratedEnum, RejectsValuesOutsideTheTable) {
    EXPECT_DEATH(static_cast<void>(to_string(static_cast<EPlainFixture>(255))), "index");
    EXPECT_DEATH(static_cast<void>(to_string(static_cast<ENameOnlyFixture>(-1))), "index");
}
#endif

TEST(GeneratedEnum, PackedStringsPreserveCategoriesLayoutsAndTerminators) {
    using ioj::Display;
    using ioj::enum_string;
    using ioj::Name;
    using ioj::Serialized;
    static_assert(enum_string<Name>(EPlainFixture::First) == "First");
    static_assert(enum_string<Display>(EPlainFixture::ReadableName) == "Readable Name");
    static_assert(enum_string<Serialized>(EPlainFixture::ReadableName) == "readable_name");
    static_assert(noexcept(enum_string<Name>(EPlainFixture::First)));
    static_assert(!ioj::HasEnumStrings<int, Name>);
    static_assert(!ioj::HasEnumStrings<ENameOnlyFixture, Serialized>);
    static_assert(to_string(ENameOnlyFixture::First) == "First");
    static_assert(to_string(EReorderedFixture::Later) == "Later");
    static_assert(to_string(EReorderedFixture::Second) == "Second");
    static_assert(to_string(EMissingNames::Count) == "Count");
    static_assert(to_serialized_string(EMissingNames::Count) == "<invalid EMissingNames>");
    static_assert(to_display_string(EMissingNames::Unnamed) == "Unnamed");
    static_assert(to_serialized_string(EMissingNames::Empty) == "empty");
    static_assert(to_display_string(EMissingNames::Escaped) == "quote\"\\line\n");
    static_assert(to_serialized_string(EMissingNames::Escaped) == "2digits");

    for (auto const value : {EMissingNames::Unnamed,
                             EMissingNames::Empty,
                             EMissingNames::Escaped,
                             EMissingNames::Count}) {
        for (auto const text :
             {to_string(value), to_display_string(value), to_serialized_string(value)}) {
            EXPECT_EQ(text.data()[text.size()], '\0');
        }
    }
    auto const& table{ioj::enum_strings<EPlainFixture, Name>};
    EXPECT_EQ(to_string(EPlainFixture::First).data(), table.data);
    EXPECT_EQ(to_string(EPlainFixture::ReadableName).data(), table.data + table.offsets[1]);
}

TEST(GeneratedEnum, PackedStringsShareLookupAcrossCharacterAndOffsetTypes) {
    enum class Value { First, Second };
    constexpr ioj::PackedStrings utf16{u"\U0001f680\0second",
                                       std::array<std::uint16_t, 3>{0, 3, 10}};
    constexpr ioj::PackedStrings utf32{U"\U0001f680\0second",
                                       std::array<std::uint32_t, 3>{0, 2, 9}};
    static_assert(utf16(Value::First) == u"\U0001f680");
    static_assert(utf32(Value::First) == U"\U0001f680");
    static_assert(utf16(Value::Second) == u"second");
    static_assert(utf32(Value::Second) == U"second");
    EXPECT_EQ(utf16(Value::First).data()[utf16(Value::First).size()], u'\0');
    EXPECT_EQ(utf32(Value::Second).data()[utf32(Value::Second).size()], U'\0');
}

TEST(GeneratedIntegerScalar, PreservesRawSentinelsWithoutImplicitConversions) {
    constexpr auto invalid{DamageReason::from_raw(255)};
    static_assert(invalid.raw_value() == 255);
    static_assert(invalid.value() == 255);
    EXPECT_TRUE(invalid.is_valid());
    EXPECT_FALSE(DamageReason::from_raw(42).is_valid());
    EXPECT_TRUE(DamageReason{1}.is_valid());
}

TEST(GeneratedIntegerScalar, FormatsDecodedNumbersWithStandardNumericSpecifications) {
    constexpr RegisteredInt negative{-42};
    EXPECT_EQ(std::format("{}", negative), "-42");
    EXPECT_EQ(std::format("{:+06d}", negative), "-00042");
    EXPECT_EQ(std::format("{:>6}", negative), "   -42");
    EXPECT_EQ(std::format("{:#x}", unified::EntityIndex{0xabcdef}), "0xabcdef");
    EXPECT_EQ(std::format("{}", DamageCode{7}), "7");
    EXPECT_EQ(std::format("{:08b}", DamageReason::from_raw(255)), "11111111");
    EXPECT_EQ(std::format(L"{:04X}", DamageReason::from_raw(255)), L"00FF");
    EXPECT_EQ(std::format(L"{:+06d}", negative), L"-00042");
    EXPECT_EQ(std::format("{}", RegisteredInt::from_raw(0xabcdefd6)), "-42");
    EXPECT_EQ(std::format("{}", Health::from_raw(1001)), "1001");
}

TEST(GeneratedIntegerScalar, HashesDecodedValuesConsistentlyWithEquality) {
    constexpr RegisteredInt canonical{-42};
    constexpr auto padded{RegisteredInt::from_raw(0xabcdefd6)};
    static_assert(canonical == padded);
    static_assert(canonical.raw_value() != padded.raw_value());
    static_assert(noexcept(std::hash<RegisteredInt>{}(canonical)));
    EXPECT_EQ(std::hash<RegisteredInt>{}(canonical),
              std::hash<RegisteredInt::value_type>{}(canonical.value()));
    EXPECT_EQ(std::hash<RegisteredInt>{}(canonical), std::hash<RegisteredInt>{}(padded));
    std::unordered_set<RegisteredInt> values{canonical, padded, RegisteredInt{-41}};
    EXPECT_EQ(values.size(), 2U);
    EXPECT_TRUE(values.contains(RegisteredInt::from_raw(0x123456d6)));
    EXPECT_EQ(std::hash<DamageReason>{}(DamageReason::from_raw(255)),
              std::hash<DamageReason::value_type>{}(255));
    constexpr unified::EntityIndex index{42};
    constexpr auto padded_index{unified::EntityIndex::from_raw(0xff00002a)};
    static_assert(index == padded_index);
    static_assert((index <=> padded_index) == std::strong_ordering::equal);
    EXPECT_EQ(std::hash<unified::EntityIndex>{}(index),
              std::hash<unified::EntityIndex>{}(padded_index));
    static_assert(unified::SignedOffset{-32768} < unified::SignedOffset{32767});
    static_assert(unified::SignedOffset{-1} < unified::SignedOffset{0});
    static_assert(DamageReason{0} < DamageReason::from_raw(255));
}

TEST(UnifiedPacked, SharedValidationHelpersRetainRangeAndEncodingBoundaries) {
    static_assert(Health::is_valid_value(0));
    static_assert(Health::is_valid_value(1000));
    static_assert(!Health::is_valid_value(1001));
    static_assert(RegisteredInt::is_valid_value(-100));
    static_assert(RegisteredInt::is_valid_value(100));
    static_assert(!RegisteredInt::is_valid_value(-101));
    static_assert(!RegisteredInt::is_valid_value(101));
    static_assert(CheckedValue::is_valid_serial(0xffffff));
    static_assert(!CheckedValue::is_valid_serial(0x1000000));
    static_assert(SignedTemperature::is_valid_temperature(-128));
    static_assert(SignedTemperature::is_valid_temperature(-100));
    static_assert(SignedTemperature::is_valid_temperature(100));
    static_assert(!SignedTemperature::is_valid_temperature(-101));
    static_assert(!SignedTemperature::is_valid_temperature(101));
    static_assert(Vitals::is_valid_health_encoded(253));
    static_assert(!Vitals::is_valid_health_encoded(254));
}

TEST(UnifiedPacked, ExposesCompileTimeEncodingStorageDomainsAndDefaults) {
    static_assert(unified::EntityIndex::encoded_bits == 24);
    static_assert(unified::EntityIndex::storage_bits == 32);
    static_assert(unified::EntityIndex::minimum_value == 0);
    static_assert(unified::EntityIndex::maximum_value == 0xffffff);
    static_assert(RegisteredInt::minimum_value == -100);
    static_assert(RegisteredInt::maximum_value == 100);
    static_assert(Health::minimum_value == 0);
    static_assert(Health::maximum_value == 1000);
    static_assert(unified::WideEntityId::encoded_bits == 32);
    static_assert(unified::WideEntityId::storage_bits == 64);
    static_assert(unified::NestedId::encoded_bits == 64);
    static_assert(unified::NestedId::target_field::offset == 8);
    static_assert(unified::NestedId::target_field::bits == 32);
    static_assert(unified::NestedId::target_field::mask == 0xffffffff00ULL);
    static_assert(unified::Instruction::encoded_bits == 64);
    static_assert(unified::Instruction::storage_bits == 64);
    static_assert(unified::Instruction::opcode_field::bits == 6);
    static_assert(unified::Instruction::payload_kill_entity_target_index_field::offset == 6);
    static_assert(unified::Instruction::payload_kill_entity_target_index_field::bits == 24);
    static_assert(unified::Instruction::payload_kill_entity_target_index_field::mask ==
                  (0xffffffULL << 6));
    static_assert(unified::Target::encoded_bits == 32);
    static_assert(unified::Target::entity_index_field::offset == 0);
    static_assert(unified::Target::entity_index_field::bits == 24);
    static_assert(Defaults::default_raw_value == (5U | (29U << 5)));
    static_assert(DefaultsMsb::default_raw_value == ((5U << 13) | (29U << 6)));
    static_assert(CheckedValue::default_raw_value == CheckedValue::invalid_value);
}

TEST(UnifiedPacked, FormatsRecordAndUnionRawStorageIncludingUnusedBits) {
    constexpr auto value{unified::WideEntityId::from_raw(0xabcd000007123456ULL)};
    EXPECT_EQ(std::format("{:016x}", value), "abcd000007123456");
    EXPECT_EQ(std::format(L"{:#018X}", value), L"0XABCD000007123456");
    EXPECT_EQ(std::format("{}", unified::EntityId{42, 0}), "42");
    EXPECT_EQ(std::format("{:08X}", unified::Target::from_raw(0xabcdef01)), "ABCDEF01");
    EXPECT_EQ(std::format("{:016x}", unified::Instruction::from_raw(0xffffffffffffffffULL)),
              "ffffffffffffffff");
}

TEST(UnifiedPacked, HashesEveryStorageBitConsistentlyWithRawEquality) {
    using unified::WideEntityId;
    constexpr auto canonical{WideEntityId::from_raw(42)};
    constexpr auto padded{WideEntityId::from_raw(0xabcd00000000002aULL)};
    static_assert(canonical.index() == padded.index());
    static_assert(canonical.entity_type() == padded.entity_type());
    static_assert(canonical != padded);
    static_assert(noexcept(std::hash<WideEntityId>{}(canonical)));
    EXPECT_EQ(std::hash<WideEntityId>{}(canonical),
              std::hash<WideEntityId::storage_type>{}(canonical.raw_value()));
    EXPECT_EQ(std::hash<WideEntityId>{}(padded),
              std::hash<WideEntityId::storage_type>{}(padded.raw_value()));
    std::unordered_set<WideEntityId> values{canonical, padded, WideEntityId::from_raw(42)};
    EXPECT_EQ(values.size(), 2U);
    EXPECT_TRUE(values.contains(padded));
    constexpr auto alternative{unified::Target::from_raw(0xabcdef01)};
    EXPECT_EQ(std::hash<unified::Target>{}(alternative),
              std::hash<unified::Target::storage_type>{}(alternative.raw_value()));
}

TEST(UnifiedPacked, ScalarWidthsDomainsAndSignedValuesAreIndependentOfStorage) {
    using unified::EntityIndex;
    using unified::SignedOffset;
    static_assert(sizeof(EntityIndex) == 4);
    static_assert(std::is_same_v<EntityIndex::storage_type, std::uint32_t>);
    static_assert(std::is_same_v<EntityIndex::value_type, std::uint32_t>);
    static_assert(std::is_trivially_copyable_v<EntityIndex>);
    static_assert(std::is_standard_layout_v<EntityIndex>);
    constexpr EntityIndex index{0xabcdef};
    static_assert(index.value() == 0xabcdef);
    static_assert(index.raw_value() == 0xabcdef);
    static_assert(EntityIndex::from_raw(0xffabcdef).raw_value() == 0xffabcdef);
    constexpr SignedOffset negative{-123};
    static_assert(negative.value() == -123);
    static_assert(SignedOffset{-123} < SignedOffset{1});
    EXPECT_TRUE(EntityIndex::from_raw(0xffffffff).is_valid());
    EXPECT_EQ(EntityIndex::from_raw(0xffffffff).value(), 0xffffffU);
}

TEST(UnifiedPacked, NestedAccessorsUseLogicalWidthAndPreserveOutsideBits) {
    using unified::EntityId;
    using unified::NestedId;
    using unified::WideEntityId;
    static_assert(sizeof(EntityId) == 4);
    static_assert(sizeof(WideEntityId) == 8);
    static_assert(sizeof(NestedId) == 8);
    static_assert(alignof(NestedId) == alignof(std::uint64_t));
    static_assert(std::is_trivially_copyable_v<NestedId>);
    static_assert(std::is_standard_layout_v<NestedId>);
    constexpr auto nested{NestedId{0x12, WideEntityId{0x345678, 0x9a}}};
    static_assert(nested.raw_value() == 0x9a34567812ULL);
    static_assert(nested.target_index() == 0x345678);
    static_assert(nested.target_entity_type() == 0x9a);
    static_assert(nested.target().raw_value() == 0x9a345678ULL);
    auto value{NestedId::from_raw(0xffffff9a34567812ULL)};
    value.set_target_index(0xabcdef);
    EXPECT_EQ(value.raw_value(), 0xffffff9aabcdef12ULL);
    value.set_target(WideEntityId{0x123456, 0x78});
    EXPECT_EQ(value.raw_value(), 0xffffff7812345612ULL);
}

TEST(UnifiedPacked, InstructionFactoriesSetTagAndClearInactivePayload) {
    using unified::EntityId;
    using unified::Instruction;
    using unified::Opcode;
    static_assert(sizeof(Instruction) == 8);
    static_assert(alignof(Instruction) == alignof(std::uint64_t));
    static_assert(std::is_trivially_copyable_v<Instruction>);
    static_assert(std::is_standard_layout_v<Instruction>);
    static_assert(!std::is_default_constructible_v<Instruction>);
    constexpr auto kill{Instruction::make_payload_kill_entity(EntityId{0x123456, 7})};
    static_assert(kill.opcode() == Opcode::KillEntity);
    static_assert(kill.payload_kill_entity_target_index() == 0x123456);
    static_assert(kill.payload_kill_entity_target_entity_type() == 7);
    static_assert(kill.payload_kill_entity_target().raw_value() == 0x07123456);
    static_assert(kill.raw_value() == (0x07123456ULL << 6));
    constexpr auto move{Instruction::make_payload_move_to(0xabcdef12)};
    static_assert(move.opcode() == Opcode::MoveTo);
    static_assert(move.payload_move_to_destination() == 0xabcdef12);
    static_assert(move.raw_value() == (0xabcdef12ULL << 6) + 1);
    constexpr auto jump{Instruction::make_payload_jump(0x654321)};
    static_assert(jump.opcode() == Opcode::Jump);
    static_assert(jump.payload_jump_destination() == 0x654321);
    static_assert(jump.raw_value() == (0x654321ULL << 6) + 2);
    EXPECT_TRUE(kill.is_valid());
    EXPECT_TRUE(move.is_valid());
    EXPECT_TRUE(jump.is_valid());
    EXPECT_FALSE(Instruction::from_raw(63).is_valid());
}

TEST(UnifiedPacked, AlternativeMutationPreservesTagOtherFieldsAndUnusedBits) {
    using unified::EntityId;
    using unified::Instruction;
    using unified::Opcode;
    auto value{Instruction::from_raw(0xffffffc000000000ULL | (0x07123456ULL << 6))};
    value.set_payload_kill_entity_target_index(42);
    EXPECT_EQ(value.opcode(), Opcode::KillEntity);
    EXPECT_EQ(value.payload_kill_entity_target_entity_type(), 7);
    EXPECT_EQ(value.raw_value(), 0xffffffc000000000ULL | (0x0700002aULL << 6));
    value.set_payload_kill_entity_target(EntityId{19, 3});
    EXPECT_EQ(value.raw_value(), 0xffffffc000000000ULL | (0x03000013ULL << 6));
#ifndef NDEBUG
    auto jump{Instruction::make_payload_jump(1)};
    EXPECT_DEATH(jump.set_payload_kill_entity_target_index(2), "");
    EXPECT_DEATH(static_cast<void>(jump.payload_kill_entity_target()), "");
#endif
}

TEST(UnifiedPacked, FactoriesPreserveAnIndependentUnionWithoutCartesianOverloads) {
    using unified::IndependentUnions;
    using unified::Opcode;
    constexpr auto value{IndependentUnions::make_payload_a_kill_entity(17, Opcode::MoveTo, 42)};
    static_assert(value.opcode_a() == Opcode::KillEntity);
    static_assert(value.payload_a_kill_entity() == 17);
    static_assert(value.opcode_b() == Opcode::MoveTo);
    static_assert(value.payload_b_move_to() == 42);
    static_assert(value.raw_value() == (17U << 6) + (1U << 14) + (42U << 20));
    EXPECT_TRUE(value.is_valid());
}

TEST(UnifiedPacked, UntaggedAlternativesShareStorageAndArraysUseFlatAccessors) {
    auto target{unified::Target::from_raw(0xabcdef12)};
    EXPECT_EQ(target.location_index(), 0xabcdef12U);
    EXPECT_EQ(target.entity_index(), 0xcdef12U);
    target.set_entity_index(0x123456);
    EXPECT_EQ(target.raw_value(), 0xab123456U);
    auto samples{unified::Samples::from_raw(0x04030201)};
    EXPECT_EQ(samples.samples_0_value(), 1);
    EXPECT_EQ(samples.samples_3_value(), 4);
    samples.set_samples_2_value(0xff);
    EXPECT_EQ(samples.raw_value(), 0x04ff0201U);
}

TEST(UnifiedPacked, WholeUnionSetterValidatesTheActiveCaseIndependently) {
    using unified::CheckedPayload;
    using unified::Opcode;
    using unified::TaggedProjected;
    static_assert(std::is_same_v<decltype(unified::PayloadPointer::value), CheckedPayload*>);
    auto value{TaggedProjected::from_raw(255U << 14)};
    value.set_payload(CheckedPayload::make_kill_entity(7));
    EXPECT_EQ(value.opcode(), Opcode::KillEntity);
    EXPECT_EQ(value.payload_kill_entity(), 7);
    EXPECT_EQ(value.unrelated(), 255);
    EXPECT_FALSE(value.is_valid());
#ifndef NDEBUG
    EXPECT_DEATH(value.set_payload(CheckedPayload::make_move_to(200)), "");
#endif
    auto move{TaggedProjected::from_raw(1U | (255U << 14))};
    move.set_payload(CheckedPayload::make_move_to(200));
    EXPECT_EQ(move.opcode(), Opcode::MoveTo);
    EXPECT_EQ(move.payload_move_to(), 200);
    EXPECT_EQ(move.unrelated(), 255);
}

TEST(UnifiedPacked, OptionalEncodingsComposeWithPresenceAboveThePayload) {
    using unified::Optionals;
    static_assert(Optionals::sentinel_encoded_field::bits == 8);
    static_assert(Optionals::present_encoded_field::offset == 8);
    static_assert(Optionals::present_encoded_field::bits == 9);
    auto value{Optionals::from_raw(255)};
    EXPECT_TRUE(value.is_valid());
    value.set_present_encoded(0x100 | 42);
    EXPECT_EQ(value.present_encoded(), 0x100 | 42);
    EXPECT_EQ(value.sentinel_encoded(), 255);
    EXPECT_TRUE(value.is_valid());
    EXPECT_FALSE(Optionals::from_raw(101).is_valid());
    EXPECT_FALSE(Optionals::from_raw((0x100U | 101U) << 8).is_valid());
    EXPECT_TRUE(Optionals::from_raw(101U << 8).is_valid());
}

} // namespace
