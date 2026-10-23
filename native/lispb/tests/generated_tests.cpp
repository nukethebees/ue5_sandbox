#include "Generated.h"
#include "ScalarValues.h"
#include "SignatureDefinitions.h"
#include <ioj/enum_array.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>
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

TEST(GeneratedScalarAlias, UsesConfiguredCppType) {
    static_assert(std::is_same_v<Health, std::uint16_t>);
    static_assert(std::is_same_v<RegisteredInt, std::int32_t>);
    static_assert(std::is_same_v<StandardInt, std::int32_t>);
    Health const health{1000};
    EXPECT_EQ(health, 1000);
    EXPECT_EQ(DamageCode_Laser, 1);
    EXPECT_EQ(DamageReason_Laser, 1);
    EXPECT_EQ(DamageReason_name(DamageReason_Laser), "Laser");
    EXPECT_EQ(DamageReason_name(42), "");
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

TEST(GeneratedFieldMask, Operations) {
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
    static_assert(Vitals::health_field::bits == 8);
    static_assert(Vitals::health_maximum_encoded == 253);

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
    static_assert(Motion::velocity_field::bits == 12);
    static_assert(Motion::velocity_minimum_raw == -2048);
    static_assert(Motion::velocity_maximum_raw == 2047);
    static_assert(Motion::fraction_field::bits == 7);
    static_assert(Motion::fraction_minimum_raw == 0);
    static_assert(Motion::fraction_maximum_raw == 127);

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
    static_assert(PackedMiniFloat::component_field::bits == 12);
    static_assert(PackedMiniFloat::component_maximum_encoded == 0xfff);

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

TEST(GeneratedPackedValue, GeneratesConstructionValidationAndRangeHelpers) {
    static_assert(CheckedValue::invalid_value == 0x7fffffffu);
    static_assert(CheckedValue::serial_minimum == 0);
    static_assert(CheckedValue::serial_maximum == CheckedValue::serial_field::value_mask);
    static_assert(CheckedValue::serial_range_fits(0, CheckedValue::serial_field::value_mask + 1));
    static_assert(!CheckedValue::serial_range_fits(CheckedValue::serial_field::value_mask, 2));
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
    EXPECT_DEATH(static_cast<void>(inline_facade.get()), "");
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
    EXPECT_EQ(to_serialized_string(static_cast<EPlainFixture>(99)), "<invalid EPlainFixture>");
}

TEST(GeneratedIntegerScalar, ConstantsAndNameLookup) {
    static_assert(DamageReason_Unknown == 0);
    static_assert(DamageReason_Laser == 1);
    static_assert(DamageReason_Invalid == 255);
    static_assert(DamageReason_name(DamageReason_Unknown) == "Unknown");
    static_assert(DamageReason_name(DamageReason_Laser) == "Laser");
    static_assert(DamageReason_name(10).empty());
}

} // namespace
