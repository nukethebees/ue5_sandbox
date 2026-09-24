#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>

namespace codegen {
namespace {

auto compact(std::string text) -> std::string {
    std::erase_if(text, [](unsigned char const character) { return std::isspace(character) != 0; });
    return text;
}

void expect_type_only_in_alias_definitions(std::string const& header,
                                           std::string const& spelling,
                                           std::size_t const expected_definitions) {
    std::size_t count{};
    for (auto position{header.find(spelling)}; position != std::string::npos;
         position = header.find(spelling, position + spelling.size())) {
        ++count;
        EXPECT_EQ(header.substr(position - 3, 3), " = ");
        EXPECT_EQ(header[position + spelling.size()], ';');
    }
    EXPECT_EQ(count, expected_definitions);
}

auto valid_module() -> NormalModuleSchema {
    return NormalModuleSchema{
        .settings = ModuleSettings{.name = "packed", .header = "Packed.h"},
        .declarations = {PackedValueSchema{
            .name = "FighterState",
            .storage_type = TypeRef{"std::uint32_t"},
            .segments =
                {
                    PackedFieldSchema{"entity_index", TypeRef{"std::uint32_t"}, 24},
                    PackedFieldSchema{
                        "state", TypeRef{"FighterStateKind"}, 8, PackedFieldKind::enumeration},
                },
        }}};
}

auto field(PackedValueSchema& schema, std::size_t const index) -> PackedFieldSchema& {
    return std::get<PackedFieldSchema>(schema.segments[index]);
}

auto lower(NormalModuleSchema module) -> std::string {
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {std::move(module)},
    }))};
    EXPECT_EQ(files.size(), 1);
    return files.front().content;
}

auto lower_known_enum(NormalModuleSchema module, EnumSchema schema) -> std::string {
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).type =
        TypeRef{"@state"};
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .types = {{"state", RegisteredTypeSchema{.cpp_type = CppType{"FighterStateKind"}}}},
        .modules = {NormalModuleSchema{.settings =
                                           ModuleSettings{.name = "enums", .header = "Enums.h"},
                                       .declarations = {std::move(schema)}},
                    std::move(module)},
    }))};
    EXPECT_EQ(files.size(), 2);
    return files.back().content;
}

auto scalar_backed_manifest(NormalModuleSchema module, bool const signedness = false) -> Manifest {
    auto& packed_field{field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0)};
    packed_field.type = TypeRef{"project::Health"};
    packed_field.kind =
        signedness ? PackedFieldKind::signed_integer : PackedFieldKind::unsigned_integer;
    packed_field.bits.reset();
    module.settings.namespace_name = "project";
    std::get<codegen::PackedValueSchema>(module.declarations.front()).segments.resize(1);
    std::get<codegen::PackedValueSchema>(module.declarations.front()).mutable_value = true;

    return Manifest{
        .schema_version = manifest_schema_version,
        .modules = {NormalModuleSchema{
                        .settings = ModuleSettings{.name = "scalars",
                                                   .header = "Scalars.h",
                                                   .namespace_name = "project"},
                        .declarations = {IntegerScalarSchema{
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

auto lower_scalar_backed(NormalModuleSchema module, bool const signedness = false) -> std::string {
    auto const files{
        render_modules(lower_modules(scalar_backed_manifest(std::move(module), signedness)))};
    EXPECT_EQ(files.size(), 2);
    return files.back().content;
}

auto quantized_backed_manifest(std::uint64_t const reserved_codes = 2) -> Manifest {
    return Manifest{
        .schema_version = manifest_schema_version,
        .modules = {
            NormalModuleSchema{.settings = ModuleSettings{.name = "scalars",
                                                          .header = "Scalars.h",
                                                          .namespace_name = "project"},
                               .declarations = {IntegerScalarSchema{.name = "Health",
                                                                    .signedness = false,
                                                                    .minimum_value = 0,
                                                                    .maximum_value = 1000,
                                                                    .bit_width = std::nullopt}}},
            NormalModuleSchema{
                .settings = ModuleSettings{.name = "representations",
                                           .header = "Representations.h",
                                           .namespace_name = "project"},
                .declarations = {LinearQuantizedSchema{.name = "HealthQ8",
                                                       .source = TypeRef{"project::Health"},
                                                       .bit_width = 8,
                                                       .reserved_codes = reserved_codes,
                                                       .clipping = QuantizationClipping::clamp}}},
            NormalModuleSchema{
                .settings = ModuleSettings{.name = "packed",
                                           .header = "Packed.h",
                                           .namespace_name = "project"},
                .declarations = {PackedValueSchema{
                    .name = "Vitals",
                    .storage_type = TypeRef{"std::uint16_t"},
                    .segments = {PackedFieldSchema{.name = "health",
                                                   .type = TypeRef{"project::HealthQ8"},
                                                   .bits = std::nullopt,
                                                   .kind = PackedFieldKind::linear_quantized},
                                 PackedFieldSchema{
                                     .name = "state", .type = TypeRef{"std::uint8_t"}, .bits = 8}},
                    .mutable_value = true}}},
        }};
}

auto fixed_point_backed_manifest(bool const signedness = true) -> Manifest {
    return Manifest{
        .schema_version = manifest_schema_version,
        .modules = {
            NormalModuleSchema{
                .settings = ModuleSettings{.name = "representations",
                                           .header = "Representations.h",
                                           .namespace_name = "project"},
                .declarations = {FixedPointSchema{.name = "VelocityQ8_4",
                                                  .signedness = signedness,
                                                  .total_bits = 12,
                                                  .fractional_bits = 4,
                                                  .rounding = FixedPointRounding::nearest_even}}},
            NormalModuleSchema{
                .settings = ModuleSettings{.name = "packed",
                                           .header = "Packed.h",
                                           .namespace_name = "project"},
                .declarations = {PackedValueSchema{
                    .name = "Motion",
                    .storage_type = TypeRef{"std::uint16_t"},
                    .segments = {PackedFieldSchema{.name = "velocity",
                                                   .type = TypeRef{"project::VelocityQ8_4"},
                                                   .bits = std::nullopt,
                                                   .kind = PackedFieldKind::fixed_point},
                                 PackedFieldSchema{
                                     .name = "state", .type = TypeRef{"std::uint8_t"}, .bits = 4}},
                    .mutable_value = true}}},
        }};
}

auto mini_float_backed_manifest() -> Manifest {
    return Manifest{
        .schema_version = manifest_schema_version,
        .modules = {
            NormalModuleSchema{.settings = ModuleSettings{.name = "representations",
                                                          .header = "Representations.h",
                                                          .namespace_name = "project"},
                               .declarations = {MiniFloatSchema{.name = "PositionF12",
                                                                .sign_bits = 1,
                                                                .exponent_bits = 5,
                                                                .significand_bits = 6,
                                                                .exponent_bias = 15}}},
            NormalModuleSchema{
                .settings = ModuleSettings{.name = "packed",
                                           .header = "Packed.h",
                                           .namespace_name = "project"},
                .declarations = {PackedValueSchema{
                    .name = "Position",
                    .storage_type = TypeRef{"std::uint16_t"},
                    .segments = {PackedFieldSchema{.name = "component",
                                                   .type = TypeRef{"project::PositionF12"},
                                                   .bits = std::nullopt,
                                                   .kind = PackedFieldKind::mini_float},
                                 PackedFieldSchema{
                                     .name = "state", .type = TypeRef{"std::uint8_t"}, .bits = 4}},
                    .mutable_value = true}}},
        }};
}

TEST(PackedValue, LowersTypedFieldsAndThreeWayComparison) {
    auto module{valid_module()};
    std::get<codegen::PackedValueSchema>(module.declarations.front()).invalid_value = 0x7fffffffu;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).range_helper = true;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).bits.reset();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = 10;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value =
        1'000'000;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).named_codes = {
        {.name = "Player", .value = 42, .sentinel = false},
        {.name = "Invalid", .value = 0xffffff, .sentinel = true}};
    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("using storage_type = std::uint32_t;"), std::string::npos);
    EXPECT_NE(header.find("ml::valid_packed_storage<storage_type, 32>()"), std::string::npos);
    EXPECT_NE(header.find("ml::PackedField<storage_type, entity_index_type, 0, 24>"),
              std::string::npos);
    EXPECT_NE(header.find("ml::PackedField<storage_type, state_type, 24, 8>"), std::string::npos);
    EXPECT_NE(header.find("ml::packed_pack<entity_index_field>(entity_index_value)"),
              std::string::npos);
    EXPECT_NE(header.find("explicit constexpr FighterState("), std::string::npos);
    EXPECT_NE(header.find("from_raw(storage_type const raw)"), std::string::npos);
    EXPECT_EQ(header.find("auto make("), std::string::npos);
    EXPECT_EQ(header.find("entity_index_offset"), std::string::npos);
    EXPECT_EQ(header.find("entity_index_bits"), std::string::npos);
    EXPECT_EQ(header.find("entity_index_value_mask"), std::string::npos);
    EXPECT_EQ(header.find("entity_index_mask"), std::string::npos);
    EXPECT_NE(header.find("operator<=>(FighterState const&) const noexcept = default"),
              std::string::npos);
    EXPECT_NE(header.find("assert(entity_index_value <= static_cast<entity_index_type>("
                          "entity_index_field::value_mask));"),
              std::string::npos);
    EXPECT_NE(header.find("auto const raw{static_cast<storage_type>("), std::string::npos);
    EXPECT_NE(header.find("assert(raw != invalid_value);"), std::string::npos);
    EXPECT_EQ(header.find("try_set_entity_index"), std::string::npos);
    EXPECT_EQ(header.find("set_entity_index"), std::string::npos);
    EXPECT_EQ(header.find("try_make(entity_index_type const entity_index_value"),
              std::string::npos);
    EXPECT_NE(header.find("invalid_value{storage_type{0x7fffffff}}"), std::string::npos);
    EXPECT_NE(header.find("entity_index_range_fits"), std::string::npos);
    EXPECT_NE(header.find("entity_index_value >= static_cast<entity_index_type>(10)"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_value <= static_cast<entity_index_type>(1000000)"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Player{static_cast<entity_index_type>(42)}"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Invalid{static_cast<entity_index_type>(16777215)}"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_value == entity_index_Invalid"), std::string::npos);
    EXPECT_NE(header.find("entity_index() == entity_index_Invalid"), std::string::npos);
    EXPECT_NE(header.find("is_valid() const noexcept"), std::string::npos);
    EXPECT_NE(header.find("std::underlying_type_t<state_type>"), std::string::npos);
    EXPECT_NE(header.find("static_assert(std::is_enum_v<state_type>)"), std::string::npos);
    EXPECT_NE(header.find("std::is_standard_layout_v<FighterState>"), std::string::npos);
}

TEST(PackedValue, EmitsFallibleMutationOnlyWhenRequested) {
    auto module{valid_module()};
    std::get<codegen::PackedValueSchema>(module.declarations.front()).mutable_value = true;
    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("try_make(entity_index_type const entity_index_value"),
              std::string::npos);
    EXPECT_NE(header.find("try_set_entity_index"), std::string::npos);
    EXPECT_NE(header.find("set_entity_index"), std::string::npos);
}

TEST(PackedValue, UsesFieldAliasesAcrossImmutableAndMutablePublicApis) {
    for (auto const mutable_value : {false, true}) {
        auto module{valid_module()};
        auto& schema{std::get<PackedValueSchema>(module.declarations.front())};
        schema.mutable_value = mutable_value;
        field(schema, 0).range_helper = true;
        auto const header{lower(std::move(module))};
        auto const text{compact(header)};

        expect_type_only_in_alias_definitions(header, "std::uint32_t", 2);
        expect_type_only_in_alias_definitions(header, "FighterStateKind", 1);
        EXPECT_NE(
            text.find("explicitconstexprFighterState(entity_index_typeconstentity_index_value,"
                      "state_typeconststate_value)noexcept"),
            std::string::npos);
        EXPECT_NE(text.find("entity_index()constnoexcept->entity_index_type"), std::string::npos);
        EXPECT_NE(text.find("state()constnoexcept->state_type"), std::string::npos);
        EXPECT_NE(text.find("entity_index_range_fits(entity_index_typeconstfirst,"
                            "entity_index_typeconstcount)noexcept->bool"),
                  std::string::npos);
        if (mutable_value) {
            EXPECT_NE(
                text.find("try_make(entity_index_typeconstentity_index_value,"
                          "state_typeconststate_value,FighterState&out_result)noexcept->bool"),
                std::string::npos);
            EXPECT_NE(text.find("try_set_entity_index(entity_index_typeconstvalue)noexcept->bool"),
                      std::string::npos);
            EXPECT_NE(text.find("set_entity_index(entity_index_typeconstvalue)noexcept"),
                      std::string::npos);
            EXPECT_NE(text.find("try_set_state(state_typeconstvalue)noexcept->bool"),
                      std::string::npos);
            EXPECT_NE(text.find("set_state(state_typeconstvalue)noexcept"), std::string::npos);
        } else {
            EXPECT_EQ(text.find("try_make("), std::string::npos);
            EXPECT_EQ(text.find("set_entity_index("), std::string::npos);
            EXPECT_EQ(text.find("set_state("), std::string::npos);
        }
    }
}

TEST(PackedValue, UsesRepresentationAliasesAcrossPublicApis) {
    for (auto const& manifest : {quantized_backed_manifest(),
                                 fixed_point_backed_manifest(),
                                 mini_float_backed_manifest()}) {
        auto const& schema{std::get<PackedValueSchema>(
            std::get<NormalModuleSchema>(manifest.modules.back()).declarations.front())};
        auto const fixed_point{schema.name == "Motion"};
        auto const accessor{fixed_point               ? std::string{"velocity_raw"}
                            : schema.name == "Vitals" ? std::string{"health_encoded"}
                                                      : std::string{"component_encoded"}};
        auto const alias{accessor + "_type"};
        auto const parameter{alias + "const" + accessor + "_value"};
        auto const files{render_modules(lower_modules(manifest))};
        auto const& header{files.back().content};
        auto const text{compact(header)};

        expect_type_only_in_alias_definitions(
            header, "std::uint16_t", fixed_point || schema.name == "Vitals" ? 1 : 2);
        expect_type_only_in_alias_definitions(
            header, "std::uint8_t", schema.name == "Vitals" ? 2 : 1);
        if (fixed_point) {
            expect_type_only_in_alias_definitions(header, "std::int16_t", 1);
            EXPECT_NE(text.find("try_encode_velocity_value(doubleconstvalue,velocity_raw_type&out_"
                                "raw)noexcept->bool"),
                      std::string::npos);
            EXPECT_NE(text.find("velocity_raw_typeraw{};"), std::string::npos);
        }
        EXPECT_NE(text.find("explicitconstexpr" + schema.name + "(" + parameter +
                            ",state_typeconststate_value)noexcept"),
                  std::string::npos);
        EXPECT_NE(text.find("try_make(" + parameter + ",state_typeconststate_value," + schema.name +
                            "&out_result)noexcept->bool"),
                  std::string::npos);
        EXPECT_NE(text.find(accessor + "()constnoexcept->" + alias), std::string::npos);
        EXPECT_NE(text.find("try_set_" + accessor + "(" + alias + "constvalue)noexcept->bool"),
                  std::string::npos);
        EXPECT_NE(text.find("set_" + accessor + "(" + alias + "constvalue)noexcept"),
                  std::string::npos);
    }
}

TEST(PackedValue, UsesBooleanFieldAliasWithoutChangingStatusTypes) {
    auto module{valid_module()};
    auto& schema{std::get<PackedValueSchema>(module.declarations.front())};
    schema.mutable_value = true;
    schema.storage_type = TypeRef{"std::uint8_t"};
    schema.segments = {PackedFieldSchema{"flag", TypeRef{"bool"}, 1}};
    auto const text{compact(lower(std::move(module)))};

    EXPECT_NE(text.find("usingflag_type=bool;"), std::string::npos);
    EXPECT_NE(text.find("explicitconstexprFighterState(flag_typeconstflag_value)noexcept"),
              std::string::npos);
    EXPECT_NE(text.find("flag()constnoexcept->flag_type"), std::string::npos);
    EXPECT_NE(text.find("try_set_flag(flag_typeconstvalue)noexcept->bool"), std::string::npos);
    EXPECT_NE(text.find("set_flag(flag_typeconstvalue)noexcept"), std::string::npos);
    EXPECT_NE(text.find("is_valid()constnoexcept->bool"), std::string::npos);
}

TEST(PackedValue, ReservedSegmentsOccupyBitsWithoutGeneratingValueApi) {
    auto module{valid_module()};
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).bits = 20;
    std::get<codegen::PackedValueSchema>(module.declarations.front())
        .segments.insert(
            std::get<codegen::PackedValueSchema>(module.declarations.front()).segments.begin() + 1,
            PackedReservedBitsSchema{.name = "future", .bits = 4});

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("ml::PackedField<storage_type, entity_index_type, 0, 20>"),
              std::string::npos);
    EXPECT_NE(header.find("ml::PackedField<storage_type, state_type, 24, 8>"), std::string::npos);
    EXPECT_EQ(header.find("future_field"), std::string::npos);
    EXPECT_EQ(header.find("try_set_future"), std::string::npos);
    EXPECT_EQ(header.find("future_value"), std::string::npos);
}

TEST(PackedValue, MostSignificantFirstSegmentsDriveGeneratedNumericOffsets) {
    auto module{valid_module()};
    auto& value{std::get<codegen::PackedValueSchema>(module.declarations.front())};
    field(value, 0).bits = 20;
    value.segments.insert(value.segments.begin() + 1,
                          PackedReservedBitsSchema{.name = "future", .bits = 4});
    value.byte_order = PackedByteOrder::big_endian;
    value.bit_order = PackedBitOrder::most_significant_first;

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("ml::PackedField<storage_type, entity_index_type, 12, 20>"),
              std::string::npos);
    EXPECT_NE(header.find("ml::PackedField<storage_type, state_type, 0, 8>"), std::string::npos);
    EXPECT_EQ(header.find("future_field"), std::string::npos);
}

TEST(PackedValue, MostSignificantFirstEnumDomainCanExcludeInvalidRawValue) {
    auto module{valid_module()};
    auto& value{std::get<codegen::PackedValueSchema>(module.declarations.front())};
    value.invalid_value = 7;
    value.bit_order = PackedBitOrder::most_significant_first;

    auto const header{
        lower_known_enum(std::move(module),
                         EnumSchema{.name = "FighterStateKind",
                                    .underlying_type = TypeRef{"uint8"},
                                    .values = {EnumeratorSchema{"Zero", "0"},
                                               EnumeratorSchema{"COUNT", "5", std::nullopt, true}},
                                    .count = "COUNT"})};

    EXPECT_EQ(header.find("assert(raw != invalid_value);"), std::string::npos);
}

TEST(PackedValue, SerializedByteOrderDoesNotChangeHostNumericOffsets) {
    auto module{valid_module()};
    std::get<codegen::PackedValueSchema>(module.declarations.front()).byte_order =
        PackedByteOrder::big_endian;

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("ml::PackedField<storage_type, entity_index_type, 0, 24>"),
              std::string::npos);
    EXPECT_NE(header.find("ml::PackedField<storage_type, state_type, 24, 8>"), std::string::npos);
}

TEST(PackedValue, LowersSignedArbitraryWidthFieldWithSafeSignExtension) {
    auto module{valid_module()};
    std::get<codegen::PackedValueSchema>(module.declarations.front()).mutable_value = true;
    std::get<codegen::PackedValueSchema>(module.declarations.front()).name = "SignedDelta";
    std::get<codegen::PackedValueSchema>(module.declarations.front()).segments = {
        PackedFieldSchema{"delta", TypeRef{"std::int32_t"}, 17, PackedFieldKind::signed_integer},
        PackedReservedBitsSchema{.name = "future", .bits = 15}};

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("using delta_type = std::int32_t;"), std::string::npos);
    EXPECT_NE(header.find("delta() const noexcept -> delta_type"), std::string::npos);
    EXPECT_NE(header.find("try_set_delta(delta_type const value)"), std::string::npos);
    EXPECT_NE(header.find("delta_minimum{static_cast<delta_type>(-65536)}"), std::string::npos);
    EXPECT_NE(header.find("delta_maximum{static_cast<delta_type>(65535)}"), std::string::npos);
    EXPECT_NE(header.find("ml::packed_extract<delta_field>(value_)"), std::string::npos);
    EXPECT_NE(header.find("ml::packed_insert<delta_field>(value_, value)"), std::string::npos);
    EXPECT_NE(header.find("value < delta_minimum || value > delta_maximum"), std::string::npos);
}

TEST(PackedValue, DerivesAndEnforcesSignedSemanticRangeWithNamedSentinel) {
    auto module{valid_module()};
    std::get<codegen::PackedValueSchema>(module.declarations.front()).mutable_value = true;
    std::get<codegen::PackedValueSchema>(module.declarations.front()).name = "SignedTemperature";
    std::get<codegen::PackedValueSchema>(module.declarations.front()).segments = {
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

    EXPECT_NE(header.find("ml::PackedField<storage_type, temperature_type, 0, 8>"),
              std::string::npos);
    EXPECT_NE(header.find("temperature_Unknown{static_cast<temperature_type>(-128)}"),
              std::string::npos);
    EXPECT_NE(header.find("value < static_cast<temperature_type>(-100)"), std::string::npos);
    EXPECT_NE(header.find("value > static_cast<temperature_type>(100)"), std::string::npos);
    EXPECT_NE(header.find("value != temperature_Unknown"), std::string::npos);
    EXPECT_NE(header.find("temperature() == temperature_Unknown"), std::string::npos);
}

TEST(PackedValue, LowersSharedIntegerScalarDomainForPackedPlacement) {
    auto module{valid_module()};
    auto const header{lower_scalar_backed(std::move(module))};

    EXPECT_NE(header.find("using entity_index_type = std::uint16_t;"), std::string::npos);
    EXPECT_NE(header.find("ml::PackedField<storage_type, entity_index_type, 0, 12>"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Unknown{static_cast<entity_index_type>(4095)}"),
              std::string::npos);
    EXPECT_NE(header.find("value < static_cast<entity_index_type>(0)"), std::string::npos);
    EXPECT_NE(header.find("value > static_cast<entity_index_type>(1000)"), std::string::npos);
    EXPECT_NE(header.find("value != entity_index_Unknown"), std::string::npos);
    EXPECT_NE(header.find("entity_index() == entity_index_Unknown"), std::string::npos);
    EXPECT_EQ(header.find("using entity_index_type = project::Health;"), std::string::npos);
}

TEST(PackedValue, LowersSignedSharedIntegerScalarToSmallestNativeAccessor) {
    auto module{valid_module()};
    auto const header{lower_scalar_backed(std::move(module), true)};

    EXPECT_NE(header.find("using entity_index_type = std::int8_t;"), std::string::npos);
    EXPECT_NE(header.find("ml::PackedField<storage_type, entity_index_type, 0, 8>"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Unknown{static_cast<entity_index_type>(-128)}"),
              std::string::npos);
    EXPECT_NE(header.find("ml::packed_extract<entity_index_field>(value_)"), std::string::npos);
}

TEST(PackedValue, LowersLinearQuantizedPlacementToExplicitEncodedCodeApi) {
    auto const files{render_modules(lower_modules(quantized_backed_manifest()))};
    ASSERT_EQ(files.size(), 3);
    auto const& header{files.back().content};

    EXPECT_NE(header.find("using health_encoded_type = std::uint8_t;"), std::string::npos);
    EXPECT_NE(header.find("health_maximum_encoded{health_encoded_type{0xfd}}"), std::string::npos);
    EXPECT_NE(header.find("try_make(health_encoded_type const health_encoded_value"),
              std::string::npos);
    EXPECT_NE(header.find("auto health_encoded() const noexcept -> health_encoded_type"),
              std::string::npos);
    EXPECT_NE(header.find("try_set_health_encoded(health_encoded_type const value)"),
              std::string::npos);
    EXPECT_NE(header.find("value > health_maximum_encoded"), std::string::npos);
    EXPECT_NE(header.find("health_encoded() <= health_maximum_encoded"), std::string::npos);
    EXPECT_EQ(header.find("project::HealthQ8"), std::string::npos);
    EXPECT_EQ(header.find("health()"), std::string::npos);
}

TEST(PackedValue, RejectsCompetingLinearQuantizedPlacementFacts) {
    auto manifest{quantized_backed_manifest()};
    auto& packed{std::get<NormalModuleSchema>(manifest.modules.back())};
    auto& health{field(std::get<PackedValueSchema>(packed.declarations.front()), 0)};

    health.bits = 7;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    health.bits.reset();
    health.kind = PackedFieldKind::unsigned_integer;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    health.kind = PackedFieldKind::linear_quantized;
    health.minimum_value = 0;
    health.maximum_value = 1000;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    health.minimum_value.reset();
    health.maximum_value.reset();
    health.range_helper = true;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(PackedValue, LowersFixedPointPlacementToExplicitRawCodeApi) {
    auto const files{render_modules(lower_modules(fixed_point_backed_manifest()))};
    ASSERT_EQ(files.size(), 2);
    auto const& header{files.back().content};

    EXPECT_NE(header.find("using velocity_raw_type = std::int16_t;"), std::string::npos);
    EXPECT_NE(header.find("velocity_minimum_raw{static_cast<velocity_raw_type>(-2048)}"),
              std::string::npos);
    EXPECT_NE(header.find("velocity_maximum_raw{static_cast<velocity_raw_type>(2047)}"),
              std::string::npos);
    EXPECT_NE(header.find("try_make(velocity_raw_type const velocity_raw_value"),
              std::string::npos);
    EXPECT_NE(header.find("auto velocity_raw() const noexcept -> velocity_raw_type"),
              std::string::npos);
    EXPECT_NE(header.find("try_set_velocity_raw(velocity_raw_type const value)"),
              std::string::npos);
    EXPECT_NE(header.find("value < velocity_minimum_allowed_raw || value > "
                          "velocity_maximum_allowed_raw"),
              std::string::npos);
    EXPECT_NE(header.find("auto velocity_value() const noexcept -> double"), std::string::npos);
    EXPECT_NE(header.find("try_encode_velocity_value(double const value"), std::string::npos);
    EXPECT_NE(header.find("try_set_velocity_value(double const value"), std::string::npos);
    EXPECT_EQ(header.find("project::VelocityQ8_4"), std::string::npos);
    EXPECT_EQ(header.find("velocity()"), std::string::npos);

    auto const unsigned_files{render_modules(lower_modules(fixed_point_backed_manifest(false)))};
    ASSERT_EQ(unsigned_files.size(), 2);
    auto const& unsigned_header{unsigned_files.back().content};
    EXPECT_NE(unsigned_header.find("using velocity_raw_type = std::uint16_t;"), std::string::npos);
    EXPECT_NE(unsigned_header.find("velocity_maximum_raw{velocity_raw_type{0xfff}}"),
              std::string::npos);

    auto bounded_manifest{fixed_point_backed_manifest()};
    auto& fixed_schema{std::get<FixedPointSchema>(
        std::get<NormalModuleSchema>(bounded_manifest.modules.front()).declarations.front())};
    fixed_schema.minimum_value = "-1.5";
    fixed_schema.maximum_value = "2.25";
    auto const bounded_files{render_modules(lower_modules(bounded_manifest))};
    auto const& bounded_header{bounded_files.back().content};
    EXPECT_NE(
        bounded_header.find("velocity_minimum_allowed_raw{static_cast<velocity_raw_type>(-24)}"),
        std::string::npos);
    EXPECT_NE(
        bounded_header.find("velocity_maximum_allowed_raw{static_cast<velocity_raw_type>(36)}"),
        std::string::npos);

    auto immutable_manifest{fixed_point_backed_manifest()};
    std::get<PackedValueSchema>(
        std::get<NormalModuleSchema>(immutable_manifest.modules.back()).declarations.front())
        .mutable_value = false;
    auto const immutable_files{render_modules(lower_modules(immutable_manifest))};
    ASSERT_EQ(immutable_files.size(), 2);
    EXPECT_NE(immutable_files.back().content.find(
                  "assert(velocity_raw_value >= velocity_minimum_allowed_raw && "
                  "velocity_raw_value <= velocity_maximum_allowed_raw);"),
              std::string::npos);
}

TEST(PackedValue, RejectsCompetingFixedPointPlacementFacts) {
    auto manifest{fixed_point_backed_manifest()};
    auto& packed{std::get<NormalModuleSchema>(manifest.modules.back())};
    auto& velocity{field(std::get<PackedValueSchema>(packed.declarations.front()), 0)};

    velocity.bits = 11;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    velocity.bits.reset();
    velocity.kind = PackedFieldKind::signed_integer;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    velocity.kind = PackedFieldKind::fixed_point;
    velocity.minimum_value = -10;
    velocity.maximum_value = 10;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    velocity.minimum_value.reset();
    velocity.maximum_value.reset();
    velocity.named_codes.push_back({.name = "Zero", .value = 0, .sentinel = false});
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    velocity.named_codes.clear();
    velocity.range_helper = true;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    velocity.range_helper = false;
    velocity.relationship = SemanticRelationSchema{.kind = SemanticRelationKind::encoded_as,
                                                   .target = TypeRef{"project::VelocityQ8_4"},
                                                   .unit = std::nullopt};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(PackedValue, LowersMiniFloatPlacementAsRawEncodedBits) {
    auto const files{render_modules(lower_modules(mini_float_backed_manifest()))};
    ASSERT_EQ(files.size(), 2U);
    auto const& header{files.back().content};
    EXPECT_NE(header.find("using component_encoded_type = std::uint16_t;"), std::string::npos);
    EXPECT_NE(header.find("ml::PackedField<storage_type, component_encoded_type, 0, 12>"),
              std::string::npos);
    EXPECT_NE(header.find("component_maximum_encoded{component_encoded_type{0xfff}}"),
              std::string::npos);
    EXPECT_NE(header.find("auto component_encoded() const noexcept -> component_encoded_type"),
              std::string::npos);
    EXPECT_NE(header.find("try_set_component_encoded(component_encoded_type const value)"),
              std::string::npos);
    EXPECT_NE(header.find("value > component_maximum_encoded"), std::string::npos);
    EXPECT_EQ(header.find("project::PositionF12"), std::string::npos);
    EXPECT_EQ(header.find("component()"), std::string::npos);

    auto wide{mini_float_backed_manifest()};
    auto& wide_representations{std::get<NormalModuleSchema>(wide.modules.front())};
    std::get<codegen::MiniFloatSchema>(wide_representations.declarations.front()).significand_bits =
        58;
    auto& wide_packed{std::get<codegen::PackedValueSchema>(
        std::get<NormalModuleSchema>(wide.modules.back()).declarations.front())};
    wide_packed.storage_type = TypeRef{"std::uint64_t"};
    wide_packed.segments.resize(1);
    wide_packed.mutable_value = false;
    auto const wide_files{render_modules(lower_modules(wide))};
    ASSERT_EQ(wide_files.size(), 2U);
    EXPECT_NE(wide_files.back().content.find(
                  "ml::PackedField<storage_type, component_encoded_type, 0, 64>"),
              std::string::npos);
    EXPECT_NE(wide_files.back().content.find("component_maximum_encoded{"
                                             "component_encoded_type{0xffffffffffffffff}}"),
              std::string::npos);
    EXPECT_NE(wide_files.back().content.find(
                  "assert(component_encoded_value <= component_maximum_encoded);"),
              std::string::npos);
}

TEST(PackedValue, RejectsCompetingMiniFloatPlacementFacts) {
    auto manifest{mini_float_backed_manifest()};
    auto& packed{std::get<NormalModuleSchema>(manifest.modules.back())};
    auto& component{field(std::get<PackedValueSchema>(packed.declarations.front()), 0)};
    component.bits = 11;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
    component.bits.reset();
    component.kind = PackedFieldKind::unsigned_integer;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
    component.kind = PackedFieldKind::mini_float;
    component.minimum_value = 0;
    component.maximum_value = 1;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
    component.minimum_value.reset();
    component.maximum_value.reset();
    component.named_codes.push_back({.name = "Zero", .value = 0, .sentinel = false});
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
    component.named_codes.clear();
    component.range_helper = true;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
    component.range_helper = false;
    component.relationship = SemanticRelationSchema{.kind = SemanticRelationKind::encoded_as,
                                                    .target = TypeRef{"project::PositionF12"},
                                                    .unit = std::nullopt};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(PackedValue, RejectsCompetingOrMismatchedIntegerScalarFieldDomain) {
    auto module{valid_module()};
    auto manifest{scalar_backed_manifest(std::move(module))};
    auto& packed{std::get<NormalModuleSchema>(manifest.modules.back())};
    auto& packed_field{field(std::get<PackedValueSchema>(packed.declarations.front()), 0)};

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
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).bits = 0;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).bits = 25;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    std::get<codegen::PackedValueSchema>(module.declarations.front()).storage_type =
        TypeRef{"int32"};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).type =
        TypeRef{"int32"};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).kind =
        PackedFieldKind::signed_integer;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).type =
        TypeRef{"int32"};
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).kind =
        PackedFieldKind::signed_integer;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).bits.reset();
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).type =
        TypeRef{"int16"};
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).kind =
        PackedFieldKind::signed_integer;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).bits = 17;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    std::get<codegen::PackedValueSchema>(module.declarations.front()).segments.front() =
        PackedFieldSchema{"flag", TypeRef{"bool"}, 2};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    module.settings.source = "Packed.cpp";
    EXPECT_EQ(render_modules(lower_modules(Manifest{.schema_version = manifest_schema_version,
                                                    .modules = {module}}))
                  .size(),
              2U);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).range_helper = true;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    std::get<codegen::PackedValueSchema>(module.declarations.front()).invalid_value =
        std::uint64_t{1} << 32;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    std::get<codegen::PackedValueSchema>(module.declarations.front()).segments = {
        PackedReservedBitsSchema{.name = "future", .bits = 32}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    std::get<codegen::PackedValueSchema>(module.declarations.front())
        .segments.insert(
            std::get<codegen::PackedValueSchema>(module.declarations.front()).segments.begin() + 1,
            PackedReservedBitsSchema{.name = "future", .bits = 0});
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    std::get<codegen::PackedValueSchema>(module.declarations.front())
        .segments.insert(
            std::get<codegen::PackedValueSchema>(module.declarations.front()).segments.begin() + 1,
            PackedReservedBitsSchema{.name = "entity_index", .bits = 1});
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = 1;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).bits.reset();
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits.reset();
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = 100;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value = 10;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = -1;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value = 100;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).type =
        TypeRef{"std::int32_t"};
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).kind =
        PackedFieldKind::signed_integer;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).bits = 7;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value =
        -100;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value = 100;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).type =
        TypeRef{"std::int32_t"};
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).kind =
        PackedFieldKind::signed_integer;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).bits = 8;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value =
        -100;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value = 100;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).named_codes = {
        {.name = "Invalid", .value = 0, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = 0;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value =
        std::uint64_t{1} << 24;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).minimum_value = 0;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).maximum_value = 3;
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).named_codes = {
        {.name = "Invalid", .value = 0xffffff, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = 0;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value = 100;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).named_codes = {
        {.name = "Invalid", .value = 100, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = 0;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value = 100;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).named_codes = {
        {.name = "Named", .value = 101, .sentinel = false}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = 0;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value = 100;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).named_codes = {
        {.name = "Invalid", .value = 0x1000000, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = 0;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value = 100;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).named_codes = {
        {.name = "Invalid", .value = 0xffffff, .sentinel = true},
        {.name = "Invalid", .value = 0xfffffe, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).minimum_value = 0;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).maximum_value = 100;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).named_codes = {
        {.name = "Invalid", .value = 0xffffff, .sentinel = true},
        {.name = "Pending", .value = 0xffffff, .sentinel = true}};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::discriminates,
                               .target = TypeRef{"ExternalPayload"},
                               .unit = std::nullopt};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::index_into,
                               .target = TypeRef{"ExternalTable"},
                               .unit = std::nullopt};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);

    module = valid_module();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::references,
                               .target = TypeRef{"ExternalType"},
                               .unit = std::nullopt};
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);
}

TEST(PackedValue, RejectsGeneratedApiCollisions) {
    auto module{valid_module()};
    std::get<codegen::PackedValueSchema>(module.declarations.front()).mutable_value = true;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).name =
        "set_entity_index";
    EXPECT_THROW(lower(std::move(module)), std::invalid_argument);
}

TEST(PackedValue, RejectsDescriptorAndRawFactoryNameCollisions) {
    auto module{valid_module()};
    auto& schema{std::get<codegen::PackedValueSchema>(module.declarations.front())};
    field(schema, 1).name = "entity_index_field";
    EXPECT_THROW(lower(module), std::invalid_argument);
    field(schema, 1).name = "from_raw";
    EXPECT_THROW(lower(module), std::invalid_argument);
}

TEST(PackedValue, ValidatesKnownEnumUnderlyingType) {
    auto module{valid_module()};
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).type =
        TypeRef{"@state"};
    NormalModuleSchema enums{.settings = ModuleSettings{.name = "enums", .header = "Enums.h"},
                             .declarations = {EnumSchema{
                                 .name = "FighterStateKind",
                                 .underlying_type = TypeRef{"int8"},
                                 .values = {EnumeratorSchema{"Value"}},
                             }}};
    Manifest manifest{
        .schema_version = manifest_schema_version,
        .types = {{"state", RegisteredTypeSchema{.cpp_type = CppType{"FighterStateKind"}}}},
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
    std::get<codegen::PackedValueSchema>(module.declarations.front()).invalid_value = 0x07ffffffu;
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits = 4;
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

    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits = 3;
    EXPECT_THROW(lower_known_enum(std::move(module), std::move(schema)), std::invalid_argument);
}

TEST(PackedValue, OmitsSentinelAssertionWhenTheKnownEnumDomainExcludesIt) {
    auto module{valid_module()};
    std::get<codegen::PackedValueSchema>(module.declarations.front()).invalid_value = 0xffffffffu;
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
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits = 4;
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

    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits = 3;
    EXPECT_THROW(lower_known_enum(std::move(module), std::move(schema)), std::invalid_argument);
}

TEST(PackedValue, RejectsKnownEnumValuesOutsideTheirUnderlyingType) {
    auto module{valid_module()};
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits = 8;
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
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits = 8;
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
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits = 3;
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
        field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits = 2;
        NormalModuleSchema enums{.settings = ModuleSettings{.name = "enums", .header = "Enums.h"},
                                 .declarations = {EnumSchema{
                                     .name = "FighterStateKind",
                                     .underlying_type = TypeRef{"uint8"},
                                     .bit_width = bit_width,
                                     .signedness = signedness,
                                     .values = {EnumeratorSchema{"Idle", "0"},
                                                EnumeratorSchema{"Maximum", std::move(maximum)}},
                                 }}};
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
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 1).bits.reset();
    field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::index_into,
                               .target = TypeRef{"FighterStateKind"},
                               .unit = std::nullopt};
    NormalModuleSchema enums{
        .settings = ModuleSettings{.name = "enums", .header = "Enums.h"},
        .declarations = {EnumSchema{
            .name = "FighterStateKind",
            .underlying_type = TypeRef{"uint8"},
            .bit_width = std::nullopt,
            .signedness = false,
            .values = {EnumeratorSchema{"Idle", "0"}, EnumeratorSchema{"Maximum", "7"}},
        }}};
    auto const files{render_modules(lower_modules(Manifest{
        .schema_version = manifest_schema_version,
        .modules = {std::move(enums), std::move(module)},
    }))};

    auto const generated{
        std::ranges::find(files, std::filesystem::path{"Packed.h"}, &GeneratedFile::path)};
    ASSERT_NE(generated, files.end());
    EXPECT_NE(generated->content.find("ml::PackedField<storage_type, state_type, 24, 3>"),
              std::string::npos);
}

TEST(PackedValue, SentinelCodeCanIncreaseDerivedIntegerWidth) {
    auto module{valid_module()};
    auto& index{field(std::get<codegen::PackedValueSchema>(module.declarations.front()), 0)};
    index.bits.reset();
    index.minimum_value = 0;
    index.maximum_value = 7;
    index.named_codes = {{.name = "Invalid", .value = 8, .sentinel = true}};

    auto const header{lower(std::move(module))};

    EXPECT_NE(header.find("ml::PackedField<storage_type, entity_index_type, 0, 4>"),
              std::string::npos);
    EXPECT_NE(header.find("entity_index_Invalid{static_cast<entity_index_type>(8)}"),
              std::string::npos);
}

} // namespace
} // namespace codegen
