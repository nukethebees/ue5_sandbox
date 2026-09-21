#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

namespace codegen {
namespace {

auto valid_module() -> PackedValueModuleSchema {
    return PackedValueModuleSchema{
        .settings = ModuleSettings{.name = "packed", .header = "Packed.h"},
        .values = {PackedValueSchema{
            .name = "FighterState",
            .storage_type = TypeRef{"std::uint32_t"},
            .segments =
                {
                    PackedFieldSchema{"entity_index", TypeRef{"std::uint32_t"}, 24},
                    PackedFieldSchema{
                        "state", TypeRef{"FighterStateKind"}, 8, PackedFieldKind::enumeration},
                },
        }},
    };
}

auto field(PackedValueSchema& schema, std::size_t const index) -> PackedFieldSchema& {
    return std::get<PackedFieldSchema>(schema.segments[index]);
}

auto lower(PackedValueModuleSchema module) -> std::string {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {std::move(module)},
    }))};
    EXPECT_EQ(files.size(), 1);
    return files.front().content;
}

auto lower_known_enum(PackedValueModuleSchema module, EnumSchema schema) -> std::string {
    field(module.values.front(), 1).type = TypeRef{"@state"};
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .types = {{"state", CppType{"FighterStateKind"}}},
        .modules = {EnumModuleSchema{
                        .settings = ModuleSettings{.name = "enums", .header = "Enums.h"},
                        .enums = {std::move(schema)},
                    },
                    std::move(module)},
    }))};
    EXPECT_EQ(files.size(), 2);
    return files.back().content;
}

auto scalar_backed_manifest(PackedValueModuleSchema module, bool const signedness = false)
    -> Manifest {
    auto& packed_field{field(module.values.front(), 0)};
    packed_field.type = TypeRef{"project::Health"};
    packed_field.kind =
        signedness ? PackedFieldKind::signed_integer : PackedFieldKind::unsigned_integer;
    packed_field.bits.reset();
    module.settings.namespace_name = "project";
    module.values.front().segments.resize(1);
    module.values.front().mutable_value = true;

    return Manifest{
        .schema_version = manifest_schema_version,
        .modules = {ScalarModuleSchema{
                        .settings = ModuleSettings{.name = "scalars",
                                                   .header = "Scalars.h",
                                                   .namespace_name = "project"},
                        .scalars = {IntegerScalarSchema{
                            .name = "Health",
                            .signedness = signedness,
                            .minimum_value =
                                signedness ? PackedIntegerValue{-100} : PackedIntegerValue{0},
                            .maximum_value =
                                signedness ? PackedIntegerValue{100} : PackedIntegerValue{1000},
                            .bit_width = std::nullopt,
                            .named_codes = {{.name = "Unknown",
                                             .value = signedness ? PackedIntegerValue{-128}
                                                                 : PackedIntegerValue{4095},
                                             .sentinel = true}},
                        }}},
                    std::move(module)},
    };
}

auto lower_scalar_backed(PackedValueModuleSchema module, bool const signedness = false)
    -> std::string {
    auto const files{
        render_modules(lower_modules(scalar_backed_manifest(std::move(module), signedness)))};
    EXPECT_EQ(files.size(), 2);
    return files.back().content;
}

TEST(PackedValue, LowersTypedFieldsAndThreeWayComparison) {
    auto module{valid_module()};
    module.values.front().invalid_value = 0x7fffffffu;
    field(module.values.front(), 0).range_helper = true;
    field(module.values.front(), 0).bits.reset();
    field(module.values.front(), 0).minimum_value = 10;
    field(module.values.front(), 0).maximum_value = 1'000'000;
    field(module.values.front(), 0).named_codes = {
        {.name = "Player", .value = 42, .sentinel = false},
        {.name = "Invalid", .value = 0xffffff, .sentinel = true}};
    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("using storage_type = std::uint32_t;"), std::string::npos);
    EXPECT_NE(header.find("entity_index_offset{0}"), std::string::npos);
    EXPECT_NE(header.find("entity_index_value_mask{storage_type{0xffffff}}"), std::string::npos);
    EXPECT_NE(header.find("state_offset{24}"), std::string::npos);
    EXPECT_NE(header.find("state_mask{storage_type{0xff000000}}"), std::string::npos);
    EXPECT_NE(header.find("operator<=>(FighterState const&) const noexcept = default"),
              std::string::npos);
    EXPECT_NE(header.find("assert(entity_index_value <= static_cast<std::uint32_t>("
                          "entity_index_value_mask));"),
              std::string::npos);
    EXPECT_NE(header.find("auto const raw{static_cast<storage_type>("), std::string::npos);
    EXPECT_NE(header.find("assert(raw != invalid_value);"), std::string::npos);
    EXPECT_EQ(header.find("try_set_entity_index"), std::string::npos);
    EXPECT_EQ(header.find("set_entity_index"), std::string::npos);
    EXPECT_EQ(header.find("try_make(std::uint32_t const entity_index_value"), std::string::npos);
    EXPECT_NE(header.find("invalid_value{storage_type{0x7fffffff}}"), std::string::npos);
    EXPECT_NE(header.find("entity_index_range_fits"), std::string::npos);
    EXPECT_NE(header.find("entity_index_value >= static_cast<std::uint32_t>(10)"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_value <= static_cast<std::uint32_t>(1000000)"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Player{static_cast<std::uint32_t>(42)}"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Invalid{static_cast<std::uint32_t>(16777215)}"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_value == entity_index_Invalid"), std::string::npos);
    EXPECT_NE(header.find("entity_index() == entity_index_Invalid"), std::string::npos);
    EXPECT_NE(header.find("is_valid() const noexcept"), std::string::npos);
    EXPECT_NE(header.find("std::underlying_type_t<FighterStateKind>"), std::string::npos);
    EXPECT_NE(header.find("static_assert(std::is_enum_v<FighterStateKind>)"), std::string::npos);
    EXPECT_NE(header.find("std::is_standard_layout_v<FighterState>"), std::string::npos);
}

TEST(PackedValue, EmitsFallibleMutationOnlyWhenRequested) {
    auto module{valid_module()};
    module.values.front().mutable_value = true;
    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("try_make(std::uint32_t const entity_index_value"), std::string::npos);
    EXPECT_NE(header.find("try_set_entity_index"), std::string::npos);
    EXPECT_NE(header.find("set_entity_index"), std::string::npos);
}

TEST(PackedValue, ReservedSegmentsOccupyBitsWithoutGeneratingValueApi) {
    auto module{valid_module()};
    field(module.values.front(), 0).bits = 20;
    module.values.front().segments.insert(module.values.front().segments.begin() + 1,
                                          PackedReservedBitsSchema{.name = "future", .bits = 4});

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("entity_index_offset{0}"), std::string::npos);
    EXPECT_NE(header.find("state_offset{24}"), std::string::npos);
    EXPECT_EQ(header.find("future_offset"), std::string::npos);
    EXPECT_EQ(header.find("try_set_future"), std::string::npos);
    EXPECT_EQ(header.find("future_value"), std::string::npos);
}

TEST(PackedValue, MostSignificantFirstSegmentsDriveGeneratedNumericOffsets) {
    auto module{valid_module()};
    auto& value{module.values.front()};
    field(value, 0).bits = 20;
    value.segments.insert(value.segments.begin() + 1,
                          PackedReservedBitsSchema{.name = "future", .bits = 4});
    value.byte_order = PackedByteOrder::big_endian;
    value.bit_order = PackedBitOrder::most_significant_first;

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("entity_index_offset{12}"), std::string::npos);
    EXPECT_NE(header.find("entity_index_mask{storage_type{0xfffff000}}"), std::string::npos);
    EXPECT_NE(header.find("state_offset{0}"), std::string::npos);
    EXPECT_NE(header.find("state_mask{storage_type{0xff}}"), std::string::npos);
    EXPECT_EQ(header.find("future_offset"), std::string::npos);
}

TEST(PackedValue, SerializedByteOrderDoesNotChangeHostNumericOffsets) {
    auto module{valid_module()};
    module.values.front().byte_order = PackedByteOrder::big_endian;

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("entity_index_offset{0}"), std::string::npos);
    EXPECT_NE(header.find("state_offset{24}"), std::string::npos);
}

TEST(PackedValue, LowersSignedArbitraryWidthFieldWithSafeSignExtension) {
    auto module{valid_module()};
    module.values.front().mutable_value = true;
    module.values.front().name = "SignedDelta";
    module.values.front().segments = {
        PackedFieldSchema{"delta", TypeRef{"std::int32_t"}, 17, PackedFieldKind::signed_integer},
        PackedReservedBitsSchema{.name = "future", .bits = 15}};

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("using delta_type = std::int32_t;"), std::string::npos);
    EXPECT_NE(header.find("delta_minimum{static_cast<std::int32_t>(-65536)}"), std::string::npos);
    EXPECT_NE(header.find("delta_maximum{static_cast<std::int32_t>(65535)}"), std::string::npos);
    EXPECT_NE(header.find("sign_bit{storage_type{0x10000}}"), std::string::npos);
    EXPECT_NE(header.find("~encoded & delta_value_mask"), std::string::npos);
    EXPECT_NE(header.find("value < delta_minimum || value > delta_maximum"), std::string::npos);
    EXPECT_NE(header.find("static_cast<storage_type>(value)"), std::string::npos);
}

TEST(PackedValue, DerivesAndEnforcesSignedSemanticRangeWithNamedSentinel) {
    auto module{valid_module()};
    module.values.front().mutable_value = true;
    module.values.front().name = "SignedTemperature";
    module.values.front().segments = {
        PackedFieldSchema{.name = "temperature",
                          .type = TypeRef{"std::int16_t"},
                          .bits = std::nullopt,
                          .kind = PackedFieldKind::signed_integer,
                          .range_helper = false,
                          .minimum_value = -100,
                          .maximum_value = 100,
                          .named_codes = {{.name = "Freezing", .value = 0, .sentinel = false},
                                          {.name = "Unknown", .value = -128, .sentinel = true}}},
        PackedReservedBitsSchema{.name = "future", .bits = 24}};

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("temperature_bits{8}"), std::string::npos);
    EXPECT_NE(header.find("temperature_Unknown{static_cast<std::int16_t>(-128)}"),
              std::string::npos);
    EXPECT_NE(header.find("value < static_cast<std::int16_t>(-100)"), std::string::npos);
    EXPECT_NE(header.find("value > static_cast<std::int16_t>(100)"), std::string::npos);
    EXPECT_NE(header.find("value != temperature_Unknown"), std::string::npos);
    EXPECT_NE(header.find("temperature() == temperature_Unknown"), std::string::npos);
}

TEST(PackedValue, LowersSharedIntegerScalarDomainForPackedPlacement) {
    auto module{valid_module()};
    auto const header{lower_scalar_backed(std::move(module))};

    EXPECT_NE(header.find("using entity_index_type = std::uint16_t;"), std::string::npos);
    EXPECT_NE(header.find("entity_index_bits{12}"), std::string::npos);
    EXPECT_NE(header.find("entity_index_Unknown{static_cast<std::uint16_t>(4095)}"),
              std::string::npos);
    EXPECT_NE(header.find("value < static_cast<std::uint16_t>(0)"), std::string::npos);
    EXPECT_NE(header.find("value > static_cast<std::uint16_t>(1000)"), std::string::npos);
    EXPECT_NE(header.find("value != entity_index_Unknown"), std::string::npos);
    EXPECT_NE(header.find("entity_index() == entity_index_Unknown"), std::string::npos);
    EXPECT_EQ(header.find("using entity_index_type = project::Health;"), std::string::npos);
}

TEST(PackedValue, LowersSignedSharedIntegerScalarToSmallestNativeAccessor) {
    auto module{valid_module()};
    auto const header{lower_scalar_backed(std::move(module), true)};

    EXPECT_NE(header.find("using entity_index_type = std::int8_t;"), std::string::npos);
    EXPECT_NE(header.find("entity_index_bits{8}"), std::string::npos);
    EXPECT_NE(header.find("entity_index_Unknown{static_cast<std::int8_t>(-128)}"),
              std::string::npos);
    EXPECT_NE(header.find("sign_bit{storage_type{0x80}}"), std::string::npos);
}

TEST(PackedValue, RejectsCompetingOrMismatchedIntegerScalarFieldDomain) {
    auto module{valid_module()};
    auto manifest{scalar_backed_manifest(std::move(module))};
    auto& packed{std::get<PackedValueModuleSchema>(manifest.modules.back())};
    auto& packed_field{field(packed.values.front(), 0)};

    packed_field.minimum_value = 0;
    packed_field.maximum_value = 1000;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    packed_field.minimum_value.reset();
    packed_field.maximum_value.reset();
    packed_field.kind = PackedFieldKind::signed_integer;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    packed_field.kind = PackedFieldKind::unsigned_integer;
    packed_field.bits = 11;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(PackedValue, RejectsInvalidLayoutsAndTypes) {
    auto module{valid_module()};
    field(module.values.front(), 0).bits = 0;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).bits = 25;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().storage_type = TypeRef{"int32"};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).type = TypeRef{"int32"};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).kind = PackedFieldKind::signed_integer;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).type = TypeRef{"int32"};
    field(module.values.front(), 0).kind = PackedFieldKind::signed_integer;
    field(module.values.front(), 0).bits.reset();
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).type = TypeRef{"int16"};
    field(module.values.front(), 0).kind = PackedFieldKind::signed_integer;
    field(module.values.front(), 0).bits = 17;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().segments.front() = PackedFieldSchema{"flag", TypeRef{"bool"}, 2};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.settings.source = "Packed.cpp";
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 1).range_helper = true;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().invalid_value = std::uint64_t{1} << 32;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().segments = {PackedReservedBitsSchema{.name = "future", .bits = 32}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().segments.insert(module.values.front().segments.begin() + 1,
                                          PackedReservedBitsSchema{.name = "future", .bits = 0});
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.values.front().segments.insert(
        module.values.front().segments.begin() + 1,
        PackedReservedBitsSchema{.name = "entity_index", .bits = 1});
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).minimum_value = 1;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).bits.reset();
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 1).bits.reset();
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).minimum_value = 100;
    field(module.values.front(), 0).maximum_value = 10;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).minimum_value = -1;
    field(module.values.front(), 0).maximum_value = 100;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).type = TypeRef{"std::int32_t"};
    field(module.values.front(), 0).kind = PackedFieldKind::signed_integer;
    field(module.values.front(), 0).bits = 7;
    field(module.values.front(), 0).minimum_value = -100;
    field(module.values.front(), 0).maximum_value = 100;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).type = TypeRef{"std::int32_t"};
    field(module.values.front(), 0).kind = PackedFieldKind::signed_integer;
    field(module.values.front(), 0).bits = 8;
    field(module.values.front(), 0).minimum_value = -100;
    field(module.values.front(), 0).maximum_value = 100;
    field(module.values.front(), 0).named_codes = {
        {.name = "Invalid", .value = 0, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).minimum_value = 0;
    field(module.values.front(), 0).maximum_value = std::uint64_t{1} << 24;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 1).minimum_value = 0;
    field(module.values.front(), 1).maximum_value = 3;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).named_codes = {
        {.name = "Invalid", .value = 0xffffff, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).minimum_value = 0;
    field(module.values.front(), 0).maximum_value = 100;
    field(module.values.front(), 0).named_codes = {
        {.name = "Invalid", .value = 100, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).minimum_value = 0;
    field(module.values.front(), 0).maximum_value = 100;
    field(module.values.front(), 0).named_codes = {
        {.name = "Named", .value = 101, .sentinel = false}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).minimum_value = 0;
    field(module.values.front(), 0).maximum_value = 100;
    field(module.values.front(), 0).named_codes = {
        {.name = "Invalid", .value = 0x1000000, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).minimum_value = 0;
    field(module.values.front(), 0).maximum_value = 100;
    field(module.values.front(), 0).named_codes = {
        {.name = "Invalid", .value = 0xffffff, .sentinel = true},
        {.name = "Invalid", .value = 0xfffffe, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).minimum_value = 0;
    field(module.values.front(), 0).maximum_value = 100;
    field(module.values.front(), 0).named_codes = {
        {.name = "Invalid", .value = 0xffffff, .sentinel = true},
        {.name = "Pending", .value = 0xffffff, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::discriminates,
                               .target = TypeRef{"ExternalPayload"},
                               .unit = std::nullopt};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 1).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::index_into,
                               .target = TypeRef{"ExternalTable"},
                               .unit = std::nullopt};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(module.values.front(), 0).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::references,
                               .target = TypeRef{"ExternalType"},
                               .unit = std::nullopt};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);
}

TEST(PackedValue, RejectsGeneratedApiCollisions) {
    auto module{valid_module()};
    module.values.front().mutable_value = true;
    field(module.values.front(), 1).name = "set_entity_index";
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);
}

TEST(PackedValue, ValidatesKnownEnumUnderlyingType) {
    auto module{valid_module()};
    field(module.values.front(), 1).type = TypeRef{"@state"};
    EnumModuleSchema enums{
        .settings = ModuleSettings{.name = "enums", .header = "Enums.h"},
        .enums = {EnumSchema{
            .name = "FighterStateKind",
            .underlying_type = TypeRef{"int8"},
            .values = {EnumeratorSchema{"Value"}},
        }},
    };
    Manifest manifest{
        .schema_version = manifest_schema_version,
        .types = {{"state", CppType{"FighterStateKind"}}},
        .modules = {std::move(enums), std::move(module)},
    };

    try {
        static_cast<void>(lower_modules(manifest));
        FAIL() << "Expected validation error";
    } catch (std::invalid_argument const& error) {
        EXPECT_NE(std::string{error.what()}.find(
                      "enum must have an unsigned fixed-width underlying type"),
                  std::string::npos);
    }
}

TEST(PackedValue, ValidatesKnownEnumEncodedWidthWithoutEmittingPerValueAssertions) {
    auto module{valid_module()};
    module.values.front().invalid_value = 0x07ffffffu;
    field(module.values.front(), 1).bits = 4;
    auto schema{EnumSchema{
        .name = "FighterStateKind",
        .underlying_type = TypeRef{"uint8"},
        .values = {EnumeratorSchema{"Zero", "0"},
                   EnumeratorSchema{"High", "7"},
                   EnumeratorSchema{"COUNT", "8", std::nullopt, true}},
        .count = "COUNT",
    }};

    auto const header{lower_known_enum(module, schema)};
    EXPECT_EQ(header.find("static_assert(static_cast<state_underlying_type>"), std::string::npos);
    EXPECT_EQ(header.find("static_assert(state"), std::string::npos);
    EXPECT_EQ(header.find("assert(static_cast<state_underlying_type>(state_value)"),
              std::string::npos);
    EXPECT_NE(header.find("assert(raw != invalid_value);"), std::string::npos);

    field(module.values.front(), 1).bits = 3;
    EXPECT_THROW(lower_known_enum(std::move(module), std::move(schema)), std::invalid_argument);
}

TEST(PackedValue, OmitsSentinelAssertionWhenTheKnownEnumDomainExcludesIt) {
    auto module{valid_module()};
    module.values.front().invalid_value = 0xffffffffu;
    auto const header{
        lower_known_enum(std::move(module),
                         EnumSchema{
                             .name = "FighterStateKind",
                             .underlying_type = TypeRef{"uint8"},
                             .values = {EnumeratorSchema{"Zero", "0"},
                                        EnumeratorSchema{"COUNT", "5", std::nullopt, true}},
                             .count = "COUNT",
                         })};

    EXPECT_EQ(header.find("assert(raw != invalid_value);"), std::string::npos);
}

TEST(PackedValue, ResolvesImplicitValuesAfterExplicitHexadecimalValues) {
    auto module{valid_module()};
    field(module.values.front(), 1).bits = 4;
    auto schema{EnumSchema{
        .name = "FighterStateKind",
        .underlying_type = TypeRef{"uint8"},
        .values = {EnumeratorSchema{"Zero", "0"},
                   EnumeratorSchema{"High", "0x7"},
                   EnumeratorSchema{"Next"},
                   EnumeratorSchema{"COUNT", std::nullopt, std::nullopt, true}},
        .count = "COUNT",
    }};

    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(module, schema)));

    field(module.values.front(), 1).bits = 3;
    EXPECT_THROW(lower_known_enum(std::move(module), std::move(schema)), std::invalid_argument);
}

TEST(PackedValue, RejectsKnownEnumValuesOutsideTheirUnderlyingType) {
    auto module{valid_module()};
    field(module.values.front(), 1).bits = 8;
    auto schema{EnumSchema{
        .name = "FighterStateKind",
        .underlying_type = TypeRef{"uint8"},
        .values = {EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Maximum", "255"}},
    }};
    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(module, schema)));

    schema.values.back().initializer = "256";
    EXPECT_THROW(lower_known_enum(module, schema), std::invalid_argument);
}

TEST(PackedValue, AllowsEnumAliasesAndUsesStructuralCountSemantics) {
    auto module{valid_module()};
    field(module.values.front(), 1).bits = 8;
    auto schema{EnumSchema{
        .name = "FighterStateKind",
        .underlying_type = TypeRef{"uint8"},
        .values = {EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Alias", "0"}},
    }};
    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(module, schema)));

    schema.values = {EnumeratorSchema{"Zero", "0"},
                     EnumeratorSchema{"High", "7"},
                     EnumeratorSchema{"COUNT", "6", std::nullopt, true}};
    schema.count = "COUNT";
    EXPECT_NO_THROW(static_cast<void>(lower_known_enum(std::move(module), std::move(schema))));
}

TEST(PackedValue, EmitsCompactFallbackForOpaqueKnownEnumValues) {
    auto module{valid_module()};
    field(module.values.front(), 1).bits = 3;
    auto const header{
        lower_known_enum(std::move(module),
                         EnumSchema{
                             .name = "FighterStateKind",
                             .underlying_type = TypeRef{"uint8"},
                             .values = {EnumeratorSchema{"Zero", "static_cast<uint8>(0)"},
                                        EnumeratorSchema{"High", "static_cast<uint8>(7)"}},
                         })};

    EXPECT_NE(header.find("static_assert([]<auto... values>() consteval -> bool"),
              std::string::npos);
    EXPECT_NE(header.find("operator()<"), std::string::npos);
    EXPECT_EQ(header.find("static_assert(static_cast<state_underlying_type>"), std::string::npos);
}

TEST(PackedValue, RejectsFieldsNarrowerThanEnumSemanticDomain) {
    auto manifest = [](std::optional<std::uint32_t> const bit_width,
                       std::string maximum,
                       std::optional<bool> const signedness = std::nullopt) -> Manifest {
        auto module{valid_module()};
        field(module.values.front(), 1).bits = 2;
        EnumModuleSchema enums{
            .settings = ModuleSettings{.name = "enums", .header = "Enums.h"},
            .enums = {EnumSchema{
                .name = "FighterStateKind",
                .underlying_type = TypeRef{"uint8"},
                .bit_width = bit_width,
                .signedness = signedness,
                .values = {EnumeratorSchema{"Idle", "0"},
                           EnumeratorSchema{"Maximum", std::move(maximum)}},
            }},
        };
        return {.schema_version = manifest_schema_version,
                .modules = {std::move(enums), std::move(module)}};
    };

    try {
        static_cast<void>(lower_modules(manifest(3, "1")));
        FAIL() << "Expected validation error";
    } catch (std::invalid_argument const& error) {
        EXPECT_NE(std::string{error.what()}.find("smaller than the enum's 3-bit semantic domain"),
                  std::string::npos);
    }

    EXPECT_THROW(static_cast<void>(lower_modules(manifest(std::nullopt, "7"))),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(lower_modules(manifest(std::nullopt, "3", true))),
                 std::invalid_argument);
}

TEST(PackedValue, DerivesAutoWidthFromEnumSemanticDomain) {
    auto module{valid_module()};
    field(module.values.front(), 1).bits.reset();
    field(module.values.front(), 0).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::index_into,
                               .target = TypeRef{"FighterStateKind"},
                               .unit = std::nullopt};
    EnumModuleSchema enums{
        .settings = ModuleSettings{.name = "enums", .header = "Enums.h"},
        .enums = {EnumSchema{
            .name = "FighterStateKind",
            .underlying_type = TypeRef{"uint8"},
            .bit_width = std::nullopt,
            .signedness = false,
            .values = {EnumeratorSchema{"Idle", "0"}, EnumeratorSchema{"Maximum", "7"}},
        }},
    };
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {std::move(enums), std::move(module)},
    }))};

    auto const generated{
        std::ranges::find(files, std::filesystem::path{"Packed.h"}, &GeneratedFile::path)};
    ASSERT_NE(generated, files.end());
    EXPECT_NE(generated->content.find("state_bits{3}"), std::string::npos);
}

TEST(PackedValue, SentinelCodeCanIncreaseDerivedIntegerWidth) {
    auto module{valid_module()};
    auto& index{field(module.values.front(), 0)};
    index.bits.reset();
    index.minimum_value = 0;
    index.maximum_value = 7;
    index.named_codes = {{.name = "Invalid", .value = 8, .sentinel = true}};

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("entity_index_bits{4}"), std::string::npos);
    EXPECT_NE(header.find("entity_index_Invalid{static_cast<std::uint32_t>(8)}"),
              std::string::npos);
}

} // namespace
} // namespace codegen
