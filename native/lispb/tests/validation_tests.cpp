#include <codegen/generator.h>
#include <codegen/validation.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace codegen {
namespace {

template <typename T>
auto manifest_with(T module, TypeRegistry types = {}) -> Manifest {
    return Manifest{
        .schema_version = manifest_schema_version,
        .types = std::move(types),
        .modules = {ModuleSchema{std::move(module)}},
    };
}

auto valid_soa_module() -> NormalModuleSchema {
    return NormalModuleSchema{
        .settings = ModuleSettings{.name = "soa", .header = "Soa.h", .source = "Soa.cpp"},
        .declarations = {SoaSchema{
            .name = "FData",
            .members =
                {
                    SoaMemberSchema{"values", SoaMemberKind::array, TypeRef{"int32"}},
                },
        }}};
}

auto valid_mask_soa_module() -> NormalModuleSchema {
    auto module{valid_soa_module()};
    auto& schema{std::get<codegen::SoaSchema>(module.declarations.front())};
    schema.field_mask_name = "FFieldMask";
    schema.field_enum_name = "EField";
    schema.members.insert(schema.members.begin(),
                          SoaMemberSchema{"masks", SoaMemberKind::array, TypeRef{"FFieldMask"}});
    schema.members.back().mask_field = true;
    return module;
}

auto valid_vector_module() -> NormalModuleSchema {
    return NormalModuleSchema{
        .settings =
            ModuleSettings{.name = "vectors", .header = "Vectors.h", .source = "Vectors.cpp"},
        .declarations = {codegen::VectorSoaSchema{.name = "FVectors",
                                                  .value_type = TypeRef{"float"},
                                                  .components = {"xs", "ys"},
                                                  .equivalent_type = TypeRef{"FVector2f"}}}};
}

auto valid_facade_module() -> NormalModuleSchema {
    return NormalModuleSchema{.settings = ModuleSettings{.name = "facade", .header = "Facade.h"},
                              .declarations = {FacadeSchema{
                                  .name = "FFacade",
                                  .target_type = TypeRef{"FTarget"},
                                  .target_member_name = "target",
                                  .methods = {FacadeMethodSchema{
                                      .name = "reset",
                                      .return_type = TypeRef{"void"},
                                  }},
                              }}};
}

auto valid_homogeneous_module() -> NormalModuleSchema {
    return NormalModuleSchema{.settings =
                                  ModuleSettings{
                                      .name = "homogeneous",
                                      .header = "Values.h",
                                      .source = "Values.cpp",
                                  },
                              .declarations = {HomogeneousLayoutSchema{
                                  .name = "Values",
                                  .components = {"xs", "ys"},
                                  .value_types = {HomogeneousValueSchema{TypeRef{"float"}, "f"}},
                              }}};
}

auto normal_module(std::vector<DeclarationSchema> declarations,
                   std::optional<std::filesystem::path> source = std::nullopt)
    -> NormalModuleSchema {
    return NormalModuleSchema{
        .settings =
            ModuleSettings{.name = "mixed", .header = "Mixed.h", .source = std::move(source)},
        .declarations = std::move(declarations),
    };
}

auto normal_soa_module(SoaSchema schema,
                       std::vector<SoaAllocatorVariant> allocators = {},
                       SoaBackend backend = SoaBackend::unreal) -> NormalModuleSchema {
    return NormalModuleSchema{
        .settings = ModuleSettings{.name = "mixed", .header = "Mixed.h", .source = "Mixed.cpp"},
        .declarations = {std::move(schema)},
        .soa_backend = backend,
        .soa_array_allocators = std::move(allocators),
    };
}

auto plain_soa(std::string name = "Data") -> SoaSchema {
    return SoaSchema{
        .name = std::move(name),
        .members = {{.name = "values", .kind = SoaMemberKind::array, .type = TypeRef{"int32"}}}};
}

auto valid_enum_module() -> NormalModuleSchema {
    return NormalModuleSchema{.settings =
                                  ModuleSettings{
                                      .name = "enums",
                                      .header = "Enums.h",
                                      .source = "Enums.cpp",
                                  },
                              .declarations = {EnumSchema{
                                  .name = "EMode",
                                  .underlying_type = TypeRef{"uint8"},
                                  .reflection = EnumReflection::uenum,
                                  .values = {EnumeratorSchema{"Value"}},
                                  .conversions = {EnumConversion::string_view},
                              }},
                              .enum_helper_namespace = "project"};
}

auto valid_native_enum_module() -> NormalModuleSchema {
    return NormalModuleSchema{
        .settings =
            ModuleSettings{
                .name = "native_enums",
                .header = "NativeEnums.h",
                .namespace_name = "project",
            },
        .declarations = {EnumSchema{
            .name = "NativeMode",
            .underlying_type = TypeRef{"std::uint8_t"},
            .values = {EnumeratorSchema{"Idle", "0", std::nullopt, false, "idle"},
                       EnumeratorSchema{"COUNT", "1", std::nullopt, true}},
            .count = "COUNT",
            .native_api = true,
        }}};
}

auto valid_integer_scalar_module() -> NormalModuleSchema {
    return NormalModuleSchema{
        .settings = ModuleSettings{.name = "scalars", .header = "Scalars.h"},
        .declarations = {IntegerScalarSchema{
            .name = "DamageReason",
            .signedness = false,
            .minimum_value = 0,
            .maximum_value = 10,
            .bit_width = std::nullopt,
            .named_codes = {{.name = "Unknown", .value = 0, .sentinel = false},
                            {.name = "Invalid", .value = 15, .sentinel = true}},
        }}};
}

auto valid_linear_quantized_manifest() -> Manifest {
    auto scalar{valid_integer_scalar_module()};
    NormalModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .declarations = {LinearQuantizedSchema{
            .name = "DamageReasonQ4",
            .source = TypeRef{"DamageReason"},
            .bit_width = 4,
            .reserved_codes = 1,
            .clipping = QuantizationClipping::reject,
        }}};
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(scalar), std::move(representations)}};
}

auto valid_integer_varint_manifest(
    bool const signedness = false,
    IntegerVarintEncoding const encoding = IntegerVarintEncoding::unsigned_varint) -> Manifest {
    auto scalar{valid_integer_scalar_module()};
    if (signedness) {
        std::get<codegen::IntegerScalarSchema>(scalar.declarations.front()).signedness = true;
        std::get<codegen::IntegerScalarSchema>(scalar.declarations.front()).minimum_value = -100;
        std::get<codegen::IntegerScalarSchema>(scalar.declarations.front()).maximum_value = 100;
        std::get<codegen::IntegerScalarSchema>(scalar.declarations.front()).named_codes.clear();
    }
    NormalModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .declarations = {IntegerVarintSchema{.name = "DamageReasonVarint",
                                             .source = TypeRef{"DamageReason"},
                                             .encoding = encoding}}};
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(scalar), std::move(representations)}};
}

auto valid_fixed_point_manifest(bool const signedness = true) -> Manifest {
    NormalModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .declarations = {FixedPointSchema{.name = "VelocityQ12_4",
                                          .signedness = signedness,
                                          .total_bits = 16,
                                          .fractional_bits = 4,
                                          .rounding = FixedPointRounding::nearest_even}}};
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(representations)}};
}

auto valid_mini_float_manifest() -> Manifest {
    NormalModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .declarations = {MiniFloatSchema{.name = "CompactFloat",
                                         .sign_bits = 1,
                                         .exponent_bits = 5,
                                         .significand_bits = 10,
                                         .exponent_bias = 15}}};
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(representations)}};
}

auto valid_optional_sentinel_manifest() -> Manifest {
    auto scalar{valid_integer_scalar_module()};
    NormalModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .declarations = {OptionalSentinelSchema{.name = "OptionalDamageReason",
                                                .source = TypeRef{"DamageReason"},
                                                .sentinel = "Invalid"}}};
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(scalar), std::move(representations)}};
}

auto valid_optional_presence_bit_manifest() -> Manifest {
    auto scalar{valid_integer_scalar_module()};
    NormalModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .declarations = {OptionalPresenceBitSchema{.name = "PresentDamageReason",
                                                   .source = TypeRef{"DamageReason"}}}};
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(scalar), std::move(representations)}};
}

auto valid_static_table_module() -> NormalModuleSchema {
    return NormalModuleSchema{
        .settings = ModuleSettings{.name = "tables", .header = "Tables.h"},
        .declarations = {StaticTableSchema{
            .name = "FValues",
            .rows = {StaticTableRowSchema{"first"}, StaticTableRowSchema{"second"}},
            .columns = {StaticTableColumnSchema{"ids", TypeRef{"int32"}}},
        }}};
}

TEST(Validation, RejectsEmptyModuleNames) {
    auto module{valid_soa_module()};
    module.settings.name.clear();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumDefinitions) {
    auto module{valid_enum_module()};
    std::get<codegen::EnumSchema>(module.declarations.front()).values.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front())
        .values.push_back(EnumeratorSchema{"Value"});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).values.front().name = "bad-name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).underlying_type =
        TypeRef{"@missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsEnumSemanticWidthsThatCannotRepresentKnownValues) {
    auto module{valid_enum_module()};
    std::get<codegen::EnumSchema>(module.declarations.front()).reflection = EnumReflection::none;
    std::get<codegen::EnumSchema>(module.declarations.front()).bit_width = 0;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).reflection = EnumReflection::none;
    std::get<codegen::EnumSchema>(module.declarations.front()).bit_width = 65;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).reflection = EnumReflection::none;
    std::get<codegen::EnumSchema>(module.declarations.front()).values = {
        EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Seven", "7"}};
    std::get<codegen::EnumSchema>(module.declarations.front()).bit_width = 2;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).reflection = EnumReflection::none;
    std::get<codegen::EnumSchema>(module.declarations.front()).values = {
        EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Seven", "7"}};
    std::get<codegen::EnumSchema>(module.declarations.front()).bit_width = 3;
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, AppliesExplicitEnumSignednessToTheSemanticDomain) {
    auto module{valid_enum_module()};
    std::get<codegen::EnumSchema>(module.declarations.front()).reflection = EnumReflection::none;
    std::get<codegen::EnumSchema>(module.declarations.front()).values = {
        EnumeratorSchema{"Negative", "-1"}, EnumeratorSchema{"Positive", "1"}};
    std::get<codegen::EnumSchema>(module.declarations.front()).signedness = false;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).reflection = EnumReflection::none;
    std::get<codegen::EnumSchema>(module.declarations.front()).values = {
        EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Seven", "7"}};
    std::get<codegen::EnumSchema>(module.declarations.front()).signedness = true;
    std::get<codegen::EnumSchema>(module.declarations.front()).bit_width = 3;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).reflection = EnumReflection::none;
    std::get<codegen::EnumSchema>(module.declarations.front()).values = {
        EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Seven", "7"}};
    std::get<codegen::EnumSchema>(module.declarations.front()).signedness = true;
    std::get<codegen::EnumSchema>(module.declarations.front()).bit_width = 4;
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, RejectsInvalidIntegerScalarDomainsAndNamedCodes) {
    auto module{valid_integer_scalar_module()};
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).bit_width = 3;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).minimum_value = -1;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).named_codes[1].value = 10;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).named_codes[0].value = 11;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).named_codes[1].name =
        "Unknown";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).named_codes[1].value = 0;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    module.settings.source = "Scalars.cpp";
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));

    module = valid_integer_scalar_module();
    auto const lowered{lower_modules(manifest_with(std::move(module)))};
    ASSERT_EQ(lowered.size(), 1U);
    ASSERT_TRUE(lowered.front().header.has_value());
    EXPECT_EQ(lowered.front().header->path, "Scalars.h");
    EXPECT_EQ(render_modules(lowered).front().content.find("struct "), std::string::npos);
}

TEST(Validation, ValidatesIntegerScalarCppConstantsPolicy) {
    auto module{valid_integer_scalar_module()};
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_type =
        TypeRef{"std::uint8_t"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_emission =
        IntegerScalarCppEmission::constants;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_emission =
        IntegerScalarCppEmission::constants;
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_type =
        TypeRef{"std::uint8_t"};
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_emission =
        IntegerScalarCppEmission::constants;
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_type =
        TypeRef{"std::int8_t"};
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).maximum_value = 127;
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).named_codes[1].value = 128;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_emission =
        IntegerScalarCppEmission::constants;
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_type = TypeRef{"float"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_emission =
        IntegerScalarCppEmission::constants;
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_type =
        TypeRef{"std::uint8_t"};
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).named_codes.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_emission =
        IntegerScalarCppEmission::constants_with_names;
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).cpp_type =
        TypeRef{"std::uint8_t"};
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).named_codes.front().name =
        "name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, AllowsEmptyEditableModulesToRemainGenerationDestinations) {
    auto settings = [](std::string const& name) {
        return ModuleSettings{.name = name, .header = name + ".h"};
    };
    auto module{NormalModuleSchema{.settings = settings("empty")}};
    EXPECT_NO_THROW(render_modules(lower_modules(manifest_with(module))));
    module.settings.source = "empty.cpp";
    EXPECT_EQ(render_modules(lower_modules(manifest_with(module))).size(), 2U);
}

TEST(Validation, ValidatesRecordMemberRelationshipUnits) {
    auto module{NormalModuleSchema{
        .settings = ModuleSettings{.name = "records", .header = "Records.h"},
        .declarations = {
            RecordSchema{.name = "Target",
                         .members = {{.name = "value", .type = TypeRef{"std::uint32_t"}}}},
            RecordSchema{.name = "User",
                         .members = {{.name = "value",
                                      .type = TypeRef{"std::uint32_t"},
                                      .relationship = SemanticRelationSchema{
                                          .kind = SemanticRelationKind::references,
                                          .target = TypeRef{"Target"},
                                          .unit = std::nullopt}}}}}}};
    EXPECT_NO_THROW(lower_modules(manifest_with(module)));

    std::get<codegen::RecordSchema>(module.declarations[1]).members[0].relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::offset_into,
                               .target = TypeRef{"Target"},
                               .unit = std::nullopt};
    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    std::get<codegen::RecordSchema>(module.declarations[1]).members[0].relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::references,
                               .target = TypeRef{"Target"},
                               .unit = SemanticRelationUnit::elements};
    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    std::get<codegen::RecordSchema>(module.declarations[1]).members[0].relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::offset_into,
                               .target = TypeRef{"Target"},
                               .unit = SemanticRelationUnit::bytes};
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, ValidatesSoaMemberRelationshipUnits) {
    auto module{valid_soa_module()};
    module.declarations.push_back(
        SoaSchema{.name = "Target",
                  .members = {SoaMemberSchema{.name = "values",
                                              .kind = SoaMemberKind::array,
                                              .type = TypeRef{"std::uint32_t"},
                                              .relationship = std::nullopt}}});
    auto& relationship{
        std::get<codegen::SoaSchema>(module.declarations.front()).members.front().relationship};
    relationship = SemanticRelationSchema{.kind = SemanticRelationKind::references,
                                          .target = TypeRef{"Target"},
                                          .unit = std::nullopt};
    EXPECT_NO_THROW(lower_modules(manifest_with(module)));

    relationship = SemanticRelationSchema{.kind = SemanticRelationKind::offset_into,
                                          .target = TypeRef{"Target"},
                                          .unit = std::nullopt};
    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    relationship = SemanticRelationSchema{.kind = SemanticRelationKind::references,
                                          .target = TypeRef{"Target"},
                                          .unit = SemanticRelationUnit::elements};
    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    relationship = SemanticRelationSchema{.kind = SemanticRelationKind::offset_into,
                                          .target = TypeRef{"Target"},
                                          .unit = SemanticRelationUnit::bytes};
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, RequiresUnsignedIndexCountAndOffsetIntegerScalars) {
    for (auto const kind : {SemanticRelationKind::index_into,
                            SemanticRelationKind::count_of,
                            SemanticRelationKind::offset_into}) {
        auto module{valid_integer_scalar_module()};
        auto& scalar{std::get<codegen::IntegerScalarSchema>(module.declarations.front())};
        scalar.signedness = true;
        scalar.minimum_value = -10;
        scalar.maximum_value = 10;
        scalar.named_codes.clear();
        scalar.relationship =
            SemanticRelationSchema{.kind = kind,
                                   .target = TypeRef{"DamageReason"},
                                   .unit = kind == SemanticRelationKind::offset_into
                                             ? std::optional{SemanticRelationUnit::bytes}
                                             : std::nullopt};
        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }

    auto module{valid_integer_scalar_module()};
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::references,
                               .target = TypeRef{"DamageReason"},
                               .unit = std::nullopt};
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::offset_into,
                               .target = TypeRef{"DamageReason"},
                               .unit = std::nullopt};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::index_into,
                               .target = TypeRef{"DamageReason"},
                               .unit = SemanticRelationUnit::elements};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    std::get<codegen::IntegerScalarSchema>(module.declarations.front()).relationship =
        SemanticRelationSchema{.kind = SemanticRelationKind::offset_into,
                               .target = TypeRef{"DamageReason"},
                               .unit = SemanticRelationUnit::bytes};
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, ValidatesLinearQuantizedRepresentationsAndEmitsOnlyConfiguredHeader) {
    auto manifest{valid_linear_quantized_manifest()};
    auto lowered{lower_modules(manifest)};
    ASSERT_EQ(lowered.size(), 2U);
    ASSERT_TRUE(lowered[1].header.has_value());
    EXPECT_EQ(lowered[1].header->path, "Representations.h");
    EXPECT_EQ(render_modules(lowered)[1].content.find("struct "), std::string::npos);

    manifest = valid_linear_quantized_manifest();
    auto& signed_source{std::get<codegen::IntegerScalarSchema>(
        std::get<NormalModuleSchema>(manifest.modules[0]).declarations.front())};
    signed_source.signedness = true;
    signed_source.minimum_value = -100;
    signed_source.maximum_value = 100;
    signed_source.named_codes.clear();
    EXPECT_NO_THROW(lower_modules(manifest));

    manifest = valid_linear_quantized_manifest();
    std::get<codegen::LinearQuantizedSchema>(
        std::get<NormalModuleSchema>(manifest.modules[1]).declarations.front())
        .bit_width = 1;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest();
    auto& representation{std::get<codegen::LinearQuantizedSchema>(
        std::get<NormalModuleSchema>(manifest.modules[1]).declarations.front())};
    representation.bit_width = 2;
    representation.reserved_codes = 3;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest();
    std::get<codegen::IntegerScalarSchema>(
        std::get<NormalModuleSchema>(manifest.modules[0]).declarations.front())
        .maximum_value = 0;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest();
    std::get<codegen::LinearQuantizedSchema>(
        std::get<NormalModuleSchema>(manifest.modules[1]).declarations.front())
        .source = TypeRef{"std::uint32_t"};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest();
    std::get<NormalModuleSchema>(manifest.modules[1]).settings.source = "Representations.cpp";
    EXPECT_EQ(render_modules(lower_modules(manifest)).size(), 3U);
}

TEST(Validation, ValidatesIntegerVarintSourceAndSignedness) {
    EXPECT_NO_THROW(lower_modules(valid_integer_varint_manifest()));
    EXPECT_NO_THROW(
        lower_modules(valid_integer_varint_manifest(true, IntegerVarintEncoding::signed_varint)));
    EXPECT_NO_THROW(
        lower_modules(valid_integer_varint_manifest(true, IntegerVarintEncoding::zigzag_varint)));

    EXPECT_THROW(
        lower_modules(valid_integer_varint_manifest(true, IntegerVarintEncoding::unsigned_varint)),
        std::invalid_argument);
    EXPECT_THROW(
        lower_modules(valid_integer_varint_manifest(false, IntegerVarintEncoding::signed_varint)),
        std::invalid_argument);
    EXPECT_THROW(
        lower_modules(valid_integer_varint_manifest(false, IntegerVarintEncoding::zigzag_varint)),
        std::invalid_argument);

    auto manifest{valid_integer_varint_manifest()};
    std::get<codegen::IntegerVarintSchema>(
        std::get<NormalModuleSchema>(manifest.modules[1]).declarations.front())
        .source = TypeRef{"std::uint32_t"};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_integer_varint_manifest();
    auto& representations{std::get<NormalModuleSchema>(manifest.modules[1])};
    representations.declarations.push_back(LinearQuantizedSchema{
        .name = std::get<codegen::IntegerVarintSchema>(representations.declarations.front()).name,
        .source = TypeRef{"DamageReason"},
        .bit_width = 4,
        .reserved_codes = 0,
        .clipping = QuantizationClipping::reject});
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, ValidatesFixedPointWidthsAndEmitsOnlyConfiguredHeader) {
    auto manifest{valid_fixed_point_manifest()};
    auto lowered{lower_modules(manifest)};
    ASSERT_EQ(lowered.size(), 1U);
    ASSERT_TRUE(lowered.front().header.has_value());
    EXPECT_EQ(lowered.front().header->path, "Representations.h");
    EXPECT_EQ(render_modules(lowered).front().content.find("struct "), std::string::npos);

    manifest = valid_fixed_point_manifest(false);
    auto& unsigned_fixed{std::get<codegen::FixedPointSchema>(
        std::get<NormalModuleSchema>(manifest.modules.front()).declarations.front())};
    unsigned_fixed.total_bits = 8;
    unsigned_fixed.fractional_bits = 8;
    EXPECT_NO_THROW(lower_modules(manifest));

    manifest = valid_fixed_point_manifest();
    std::get<codegen::FixedPointSchema>(
        std::get<NormalModuleSchema>(manifest.modules.front()).declarations.front())
        .fractional_bits = 16;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_fixed_point_manifest();
    std::get<codegen::FixedPointSchema>(
        std::get<NormalModuleSchema>(manifest.modules.front()).declarations.front())
        .total_bits = 0;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_fixed_point_manifest();
    auto& representations{std::get<NormalModuleSchema>(manifest.modules.front())};
    representations.declarations.push_back(
        std::get<codegen::FixedPointSchema>(representations.declarations.front()));
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, ValidatesMiniFloatEncodingAndEmitsNoPhysicalType) {
    auto manifest{valid_mini_float_manifest()};
    auto lowered{lower_modules(manifest)};
    ASSERT_EQ(lowered.size(), 1U);
    ASSERT_TRUE(lowered.front().header.has_value());
    EXPECT_EQ(lowered.front().header->path, "Representations.h");
    EXPECT_EQ(render_modules(lowered).front().content.find("struct "), std::string::npos);

    auto invalidate = [](auto edit) {
        auto invalid{valid_mini_float_manifest()};
        edit(std::get<codegen::MiniFloatSchema>(
            std::get<NormalModuleSchema>(invalid.modules.front()).declarations.front()));
        EXPECT_THROW(lower_modules(invalid), std::invalid_argument);
    };
    invalidate([](MiniFloatSchema& value) { value.sign_bits = 2; });
    invalidate([](MiniFloatSchema& value) { value.exponent_bits = 1; });
    invalidate([](MiniFloatSchema& value) { value.exponent_bits = 16; });
    invalidate([](MiniFloatSchema& value) { value.significand_bits = 63; });
    invalidate([](MiniFloatSchema& value) {
        value.exponent_bits = 15;
        value.significand_bits = 49;
    });
    invalidate([](MiniFloatSchema& value) { value.exponent_bias = -32'769; });
    invalidate([](MiniFloatSchema& value) { value.exponent_bias = 32'768; });

    manifest = valid_mini_float_manifest();
    auto& representations{std::get<NormalModuleSchema>(manifest.modules.front())};
    representations.declarations.push_back(FixedPointSchema{
        .name = std::get<codegen::MiniFloatSchema>(representations.declarations.front()).name,
        .signedness = true,
        .total_bits = 16,
        .fractional_bits = 4,
        .rounding = FixedPointRounding::nearest_even,
    });
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, ValidatesOptionalSentinelSourceAndCodeRole) {
    EXPECT_NO_THROW(lower_modules(valid_optional_sentinel_manifest()));

    auto manifest{valid_optional_sentinel_manifest()};
    auto& optional{std::get<codegen::OptionalSentinelSchema>(
        std::get<NormalModuleSchema>(manifest.modules[1]).declarations.front())};
    optional.source = TypeRef{"std::uint32_t"};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest();
    std::get<codegen::OptionalSentinelSchema>(
        std::get<NormalModuleSchema>(manifest.modules[1]).declarations.front())
        .sentinel = "Missing";
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest();
    std::get<codegen::OptionalSentinelSchema>(
        std::get<NormalModuleSchema>(manifest.modules[1]).declarations.front())
        .sentinel = "Unknown";
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest();
    std::get<codegen::IntegerScalarSchema>(
        std::get<NormalModuleSchema>(manifest.modules[0]).declarations.front())
        .named_codes.clear();
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest();
    auto& representations{std::get<NormalModuleSchema>(manifest.modules[1])};
    representations.declarations.push_back(FixedPointSchema{
        .name =
            std::get<codegen::OptionalSentinelSchema>(representations.declarations.front()).name,
        .signedness = false,
        .total_bits = 8,
        .fractional_bits = 0,
        .rounding = FixedPointRounding::nearest_even,
    });
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, ValidatesOptionalPresenceBitSourceAndSharedNames) {
    EXPECT_NO_THROW(lower_modules(valid_optional_presence_bit_manifest()));

    auto manifest{valid_optional_presence_bit_manifest()};
    std::get<codegen::OptionalPresenceBitSchema>(
        std::get<NormalModuleSchema>(manifest.modules[1]).declarations.front())
        .source = TypeRef{"std::uint32_t"};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_presence_bit_manifest();
    auto& representations{std::get<NormalModuleSchema>(manifest.modules[1])};
    representations.declarations.push_back(OptionalSentinelSchema{
        .name =
            std::get<codegen::OptionalPresenceBitSchema>(representations.declarations.front()).name,
        .source = TypeRef{"DamageReason"},
        .sentinel = "Invalid"});
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumModuleConfiguration) {
    auto module{valid_enum_module()};
    module.settings.namespace_name = "project";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.settings.source.reset();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enum_helper_namespace = "bad-name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front())
        .conversions.push_back(EnumConversion::string_view);
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).reflection = EnumReflection::none;
    std::get<codegen::EnumSchema>(module.declarations.front()).values.front().hidden = true;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedNativeEnumCombinations) {
    auto module{valid_native_enum_module()};
    module.declarations.push_back(
        std::get<codegen::EnumSchema>(valid_enum_module().declarations.front()));
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).reflection = EnumReflection::uenum;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).enum_array = true;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).conversions = {
        EnumConversion::string_view};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).export_specifier = "PROJECT_API";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).unreal_projection =
        EnumUnrealProjection{
            .name = "ENativeMode",
            .header = "Project/NativeMode.h",
            .header_include = "Project/NativeMode.h",
            .conversion_header = "Project/NativeModeConversion.h",
            .native_header_include = "project/NativeEnums.h",
            .reflection = EnumReflection::none,
        };
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).unreal_projection =
        EnumUnrealProjection{
            .name = "ENativeMode",
            .header = "Project/NativeMode.h",
            .header_include = "Project/NativeMode.h",
            .conversion_header = "Project/NativeModeConversion.h",
            .native_header_include = "project/NativeEnums.h",
        };
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsIncompleteOrAmbiguousSerializedEnumNames) {
    auto module{valid_enum_module()};
    std::get<codegen::EnumSchema>(module.declarations.front())
        .conversions.push_back(EnumConversion::try_parse_serialized);
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).values.front().serialized_name =
        "value";
    std::get<codegen::EnumSchema>(module.declarations.front())
        .values.push_back(EnumeratorSchema{"Other", std::nullopt, std::nullopt, false, "value"});
    std::get<codegen::EnumSchema>(module.declarations.front())
        .conversions.push_back(EnumConversion::try_parse_serialized);
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumArrayDefinitions) {
    auto module{valid_enum_module()};
    std::get<codegen::EnumSchema>(module.declarations.front()).enum_array = true;
    std::get<codegen::EnumSchema>(module.declarations.front()).values.front().initializer = "0";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).enum_array = true;
    std::get<codegen::EnumSchema>(module.declarations.front()).values.front().hidden = true;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).count = "Value";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).enum_array = true;
    std::get<codegen::EnumSchema>(module.declarations.front()).count = "Missing";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).enum_array = true;
    std::get<codegen::EnumSchema>(module.declarations.front())
        .values.push_back(EnumeratorSchema{"COUNT", std::nullopt, std::nullopt, true});
    std::get<codegen::EnumSchema>(module.declarations.front())
        .values.push_back(EnumeratorSchema{"After"});
    std::get<codegen::EnumSchema>(module.declarations.front()).count = "COUNT";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).enum_array = true;
    std::get<codegen::EnumSchema>(module.declarations.front())
        .values.push_back(EnumeratorSchema{"COUNT"});
    std::get<codegen::EnumSchema>(module.declarations.front()).count = "COUNT";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    std::get<codegen::EnumSchema>(module.declarations.front()).enum_array = true;
    std::get<codegen::EnumSchema>(module.declarations.front()).values = {
        EnumeratorSchema{"COUNT", std::nullopt, std::nullopt, true}};
    std::get<codegen::EnumSchema>(module.declarations.front()).count = "COUNT";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, SupportsSignedEnumValuesWhenTheyFitTheUnderlyingType) {
    auto module{valid_native_enum_module()};
    auto& schema{std::get<codegen::EnumSchema>(module.declarations.front())};
    schema.underlying_type = TypeRef{"@native_int8"};
    schema.values = {EnumeratorSchema{"Minimum", "-128"}, EnumeratorSchema{"Maximum", "127"}};
    schema.count.reset();
    auto const types{TypeRegistry{
        {"native_int8", RegisteredTypeSchema{.cpp_type = CppType{"std::int8_t", "cstdint"}}},
    }};

    EXPECT_NO_THROW(static_cast<void>(lower_modules(manifest_with(module, types))));

    schema.values.back().initializer = "128";
    EXPECT_THROW(static_cast<void>(lower_modules(manifest_with(std::move(module), types))),
                 std::invalid_argument);
}

TEST(Validation, RequiresKnownSemanticFactsWhenEnumBackingIsDerived) {
    auto module{valid_native_enum_module()};
    auto& schema{std::get<codegen::EnumSchema>(module.declarations.front())};
    schema.underlying_type.reset();
    schema.bit_width = 12;
    schema.signedness = false;

    EXPECT_NO_THROW(static_cast<void>(lower_modules(manifest_with(module))));

    schema.values.front().initializer = "calculate_state()";
    schema.values.back().initializer.reset();
    schema.bit_width.reset();
    schema.signedness.reset();
    EXPECT_THROW(static_cast<void>(lower_modules(manifest_with(std::move(module)))),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableModuleConfiguration) {
    auto module{valid_static_table_module()};
    module.declarations.clear();
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));

    module = valid_static_table_module();
    module.settings.source = "Tables.cpp";
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));

    module = valid_static_table_module();
    std::get<codegen::StaticTableSchema>(module.declarations.front()).rows.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    std::get<codegen::StaticTableSchema>(module.declarations.front()).columns.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableNamesAndTypes) {
    auto module{valid_static_table_module()};
    module.declarations.push_back(
        std::get<codegen::StaticTableSchema>(module.declarations.front()));
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    std::get<codegen::StaticTableSchema>(module.declarations.front())
        .rows.push_back(StaticTableRowSchema{"first"});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    std::get<codegen::StaticTableSchema>(module.declarations.front()).rows.front().name =
        "bad-name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    std::get<codegen::StaticTableSchema>(module.declarations.front())
        .columns.push_back(
            std::get<codegen::StaticTableSchema>(module.declarations.front()).columns.front());
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    std::get<codegen::StaticTableSchema>(module.declarations.front()).columns.front().type =
        TypeRef{"@missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsStaticTableColumnsCollidingWithGeneratedApis) {
    for (auto const& name :
         {"FValues", "num_rows", "num", "apply_arrays", "apply_array_pairs", "first_index"}) {
        SCOPED_TRACE(name);
        auto module{valid_static_table_module()};
        std::get<codegen::StaticTableSchema>(module.declarations.front()).columns.front().name =
            name;
        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }
}

TEST(Validation, RejectsInvalidStaticTableGroups) {
    auto module{valid_static_table_module()};
    std::get<codegen::StaticTableSchema>(module.declarations.front()).groups = {
        StaticTableGroupSchema{"point", TypeRef{"FPoint"}, {"ids"}}};

    auto duplicate_group{module};
    std::get<codegen::StaticTableSchema>(duplicate_group.declarations.front())
        .groups.push_back(std::get<codegen::StaticTableSchema>(duplicate_group.declarations.front())
                              .groups.front());
    EXPECT_THROW(lower_modules(manifest_with(std::move(duplicate_group))), std::invalid_argument);

    auto invalid_name{module};
    std::get<codegen::StaticTableSchema>(invalid_name.declarations.front()).groups.front().name =
        "bad-name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(invalid_name))), std::invalid_argument);

    auto unknown_type{module};
    std::get<codegen::StaticTableSchema>(unknown_type.declarations.front()).groups.front().type =
        TypeRef{"@missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(unknown_type))), std::invalid_argument);

    auto empty_columns{module};
    std::get<codegen::StaticTableSchema>(empty_columns.declarations.front())
        .groups.front()
        .columns.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(empty_columns))), std::invalid_argument);

    auto unknown_column{module};
    std::get<codegen::StaticTableSchema>(unknown_column.declarations.front())
        .groups.front()
        .columns = {"missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(unknown_column))), std::invalid_argument);

    auto duplicate_column{module};
    std::get<codegen::StaticTableSchema>(duplicate_column.declarations.front())
        .groups.front()
        .columns = {"ids", "ids"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(duplicate_column))), std::invalid_argument);
}

TEST(Validation, RejectsStaticTableGroupGetterCollisions) {
    auto module{valid_static_table_module()};
    std::get<codegen::StaticTableSchema>(module.declarations.front())
        .columns.push_back(StaticTableColumnSchema{"get_point", TypeRef{"float"}});
    std::get<codegen::StaticTableSchema>(module.declarations.front()).groups = {
        StaticTableGroupSchema{"point", TypeRef{"FPoint"}, {"ids"}}};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsEmptyModuleOutputs) {
    auto module{valid_soa_module()};
    module.settings.header.clear();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsEmptyOptionalModuleSettings) {
    auto module{valid_soa_module()};
    module.settings.header_include = "";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    module.settings.namespace_name = "";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateAndEmptyIncludeOrderPrefixes) {
    auto module{valid_soa_module()};
    module.settings.include_order = {"Project/", "Project/"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    module.settings.include_order = {""};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMalformedQualifiedNamespaces) {
    auto module{valid_soa_module()};
    module.settings.namespace_name = "project:::invalid";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateModuleNames) {
    auto first{valid_soa_module()};
    auto second{valid_soa_module()};
    second.settings.header = "Other.h";
    second.settings.source = "Other.cpp";
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .modules = {std::move(first), std::move(second)},
    };

    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedProgrammaticSchemaVersions) {
    auto manifest{manifest_with(valid_soa_module())};
    manifest.schema_version = manifest_schema_version + 1;

    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, RejectsManifestsWithoutModules) {
    Manifest const manifest{.schema_version = manifest_schema_version};

    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, RejectsKeywordsAndReservedIdentifiers) {
    for (auto const& invalid : {"class", "__generated", "_Reserved"}) {
        SCOPED_TRACE(invalid);
        auto module{valid_soa_module()};
        std::get<codegen::SoaSchema>(module.declarations.front()).members.front().name = invalid;
        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }

    auto module{valid_soa_module()};
    module.settings.namespace_name = "project::namespace";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsEmptyTypeSpellings) {
    auto module{valid_soa_module()};
    EXPECT_THROW(lower_modules(manifest_with(
                     std::move(module), {{"empty", RegisteredTypeSchema{.cpp_type = CppType{}}}})),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidTypeDependencyAndOperationSpellings) {
    auto empty_header{CppType{"FValue", ""}};
    EXPECT_THROW(lower_modules(manifest_with(
                     valid_soa_module(),
                     {{"value", RegisteredTypeSchema{.cpp_type = std::move(empty_header)}}})),
                 std::invalid_argument);

    CppType invalid_operation{"FValue"};
    invalid_operation.member_operations.emplace(TypeOperation::remove_at_swap, "bad-name");
    EXPECT_THROW(lower_modules(manifest_with(
                     valid_soa_module(),
                     {{"value", RegisteredTypeSchema{.cpp_type = std::move(invalid_operation)}}})),
                 std::invalid_argument);
}

TEST(Validation, RejectsEmptySoaNames) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).name.clear();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateSoaMembers) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front())
        .members.push_back(SoaMemberSchema{"values", SoaMemberKind::array, TypeRef{"float"}});

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMalformedSoaMemberIdentifiers) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).members.front().name = "bad-name";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsIncompleteSoaFieldMaskDeclarations) {
    auto module{valid_mask_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).field_enum_name.reset();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_mask_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).field_mask_name.reset();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidSoaFieldMaskMembers) {
    auto module{valid_mask_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).members.front().type =
        TypeRef{"uint8"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_mask_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).members.back().mask_field = false;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).members.front().mask_field = true;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidSoaMaskDimensions) {
    auto module{valid_mask_soa_module()};
    auto& member{std::get<codegen::SoaSchema>(module.declarations.front()).members.back()};
    member.mask_dimensions.push_back({"field_index", "4"});
    member.mask_dimensions.push_back({"field_index", "2"});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_mask_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front())
        .members.back()
        .mask_dimensions.push_back({"bad-index", "4"});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMembersCollidingWithGeneratedStorageApis) {
    for (auto const& name : {"num", "reset", "copy_element"}) {
        SCOPED_TRACE(name);
        auto module{valid_soa_module()};
        std::get<codegen::SoaSchema>(module.declarations.front()).members.front().name = name;
        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }

    auto homogeneous{valid_homogeneous_module()};
    std::get<codegen::HomogeneousLayoutSchema>(homogeneous.declarations.front())
        .components.front() = "add";
    EXPECT_THROW(lower_modules(manifest_with(std::move(homogeneous))), std::invalid_argument);

    auto vector{valid_vector_module()};
    std::get<codegen::VectorSoaSchema>(vector.declarations.front()).components.front() = "get_view";
    EXPECT_THROW(lower_modules(manifest_with(std::move(vector))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownSoaTypeReferences) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).members.front().type =
        TypeRef{"@missing"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFixedSchemasOnArrayMembers) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).members.front().fixed_schema =
        "FChild";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsOpaqueNestedMembersInFixedSoas) {
    auto module{valid_soa_module()};
    auto& schema{std::get<codegen::SoaSchema>(module.declarations.front())};
    schema.fixed = FixedSoaSchema{"TDataStorage", {}};
    schema.members = {
        SoaMemberSchema{"child", SoaMemberKind::nested, TypeRef{"FChild"}},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownNestedFixedSchemas) {
    auto module{valid_soa_module()};
    auto& schema{std::get<codegen::SoaSchema>(module.declarations.front())};
    schema.fixed = FixedSoaSchema{"TDataStorage", {}};
    schema.members = {
        SoaMemberSchema{"child", SoaMemberKind::nested, TypeRef{"FChild"}, "FChild"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFixedSchemaCycles) {
    auto module{valid_soa_module()};
    auto& schema{std::get<codegen::SoaSchema>(module.declarations.front())};
    schema.fixed = FixedSoaSchema{"TDataStorage", {}};
    schema.members = {
        SoaMemberSchema{"children", SoaMemberKind::nested, TypeRef{"FData"}, "FData"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsCollidingGeneratedSoaTypeNames) {
    auto module{valid_soa_module()};
    auto second{std::get<codegen::SoaSchema>(module.declarations.front())};
    second.name = "FDataView";
    module.declarations.push_back(std::move(second));

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateFixedStorageAndContainerNames) {
    auto module{valid_soa_module()};
    auto& first{std::get<codegen::SoaSchema>(module.declarations.front())};
    first.fixed = FixedSoaSchema{"TShared", {"TFixedData"}};
    auto second{first};
    second.name = "FOther";
    second.fixed = FixedSoaSchema{"TShared", {"TFixedOther"}};
    module.declarations.push_back(std::move(second));

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).fixed =
        FixedSoaSchema{"TShared", {"TShared"}};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsSoaModulesWithoutSourceOutput) {
    auto module{valid_soa_module()};
    module.settings.source.reset();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsEmptyOptionalSoaViewNames) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).view_name = "";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsBlankSoaUsingDeclarations) {
    for (auto const* declaration : {"", " \t\r\n"}) {
        SCOPED_TRACE(declaration);
        auto module{valid_soa_module()};
        std::get<codegen::SoaSchema>(module.declarations.front())
            .using_declarations.emplace_back(declaration);

        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }
}

TEST(Validation, RejectsDuplicateCustomSoaFunctionParameters) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "set",
        .return_type = TypeRef{"void"},
        .parameters =
            {
                ParameterSchema{TypeRef{"int32"}, "value"},
                ParameterSchema{TypeRef{"float"}, "value"},
            },
    }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsNonTrailingCustomSoaDefaultArguments) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "set",
        .return_type = TypeRef{"void"},
        .parameters =
            {
                ParameterSchema{TypeRef{"int32"}, "first", "0"},
                ParameterSchema{TypeRef{"int32"}, "second"},
            },
    }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "set",
        .return_type = TypeRef{"void"},
        .parameters = {ParameterSchema{TypeRef{"int32"}, "value", " \t"}},
    }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownCustomSoaDependencies) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "reset_values",
        .return_type = TypeRef{"void"},
        .dependencies = {"missing"},
    }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "reset_values",
        .return_type = TypeRef{"void"},
        .dependencies = {" \t"},
    }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidCustomSoaFunctionDefinitions) {
    auto module{valid_soa_module()};
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "class",
        .return_type = TypeRef{"void"},
        .is_inline = true,
    }};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "update",
        .return_type = TypeRef{"void"},
        .is_inline = true,
        .definition_in_source = true,
    }};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "invalid_static_const",
        .return_type = TypeRef{"void"},
        .is_const = true,
        .is_static = true,
    }};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "invalid_trailing_return",
        .return_type = TypeRef{"void"},
        .trailing_return_type = TypeRef{"int32"},
    }};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "invalid_template",
        .return_type = TypeRef{"void"},
        .template_parameters = " \t",
    }};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
        .name = "invalid_requires",
        .return_type = TypeRef{"void"},
        .requires_clause = " \t",
    }};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsCustomSoaFunctionsCollidingWithGeneratedApisAndTypeNames) {
    for (auto const& name : {"sort", "get_view", "FData"}) {
        SCOPED_TRACE(name);
        auto module{valid_soa_module()};
        std::get<codegen::SoaSchema>(module.declarations.front()).functions = {FunctionSchema{
            .name = name,
            .return_type = TypeRef{"void"},
            .is_inline = true,
        }};
        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }
}

TEST(Validation, RejectsMalformedExportSpecifiers) {
    auto soa{valid_soa_module()};
    std::get<codegen::SoaSchema>(soa.declarations.front()).export_specifier = "bad-specifier";
    EXPECT_THROW(lower_modules(manifest_with(std::move(soa))), std::invalid_argument);

    auto homogeneous{valid_homogeneous_module()};
    std::get<codegen::HomogeneousLayoutSchema>(homogeneous.declarations.front()).export_specifier =
        "class";
    EXPECT_THROW(lower_modules(manifest_with(std::move(homogeneous))), std::invalid_argument);

    auto facade{valid_facade_module()};
    std::get<codegen::FacadeSchema>(facade.declarations.front()).export_specifier = "two words";
    EXPECT_THROW(lower_modules(manifest_with(std::move(facade))), std::invalid_argument);

    auto table{valid_static_table_module()};
    std::get<codegen::StaticTableSchema>(table.declarations.front()).export_specifier =
        "bad-specifier";
    EXPECT_THROW(lower_modules(manifest_with(std::move(table))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousComponents) {
    NormalModuleSchema module{
        .settings =
            ModuleSettings{.name = "homogeneous", .header = "Values.h", .source = "Values.cpp"},
        .declarations = {HomogeneousLayoutSchema{
            .name = "Values",
            .components = {"xs", "xs"},
            .value_types = {HomogeneousValueSchema{TypeRef{"float"}, "f"}},
        }}};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousLayoutsWithoutValueTypes) {
    NormalModuleSchema module{
        .settings =
            ModuleSettings{.name = "homogeneous", .header = "Values.h", .source = "Values.cpp"},
        .declarations = {HomogeneousLayoutSchema{
            .name = "Values",
            .components = {"xs"},
        }}};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousModulesWithoutSourceOutput) {
    auto module{valid_homogeneous_module()};
    module.settings.source.reset();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousLayoutNames) {
    auto module{valid_homogeneous_module()};
    module.declarations.push_back(
        std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front()));

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousValueSuffixes) {
    auto module{valid_homogeneous_module()};
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front())
        .value_types.push_back(HomogeneousValueSchema{TypeRef{"double"}, "f"});

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMalformedHomogeneousSuffixes) {
    auto module{valid_homogeneous_module()};
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front())
        .value_types.front()
        .suffix = "-float";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsCollidingHomogeneousStorageNames) {
    auto module{valid_homogeneous_module()};
    module.declarations = {HomogeneousLayoutSchema{
                               .name = "Value",
                               .components = {"xs"},
                               .value_types = {HomogeneousValueSchema{TypeRef{"float"}, "sf"}},
                           },
                           HomogeneousLayoutSchema{
                               .name = "Values",
                               .components = {"xs"},
                               .value_types = {HomogeneousValueSchema{TypeRef{"float"}, "f"}},
                           }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousEquivalentSpecialisations) {
    auto module{valid_homogeneous_module()};
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front()).value_types = {
        HomogeneousValueSchema{TypeRef{"float"}, "f", TypeRef{"FVector2f"}},
        HomogeneousValueSchema{TypeRef{"float"}, "other", TypeRef{"FOtherVector2f"}},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousInputTypes) {
    auto module{valid_homogeneous_module()};
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front())
        .value_types.front()
        .input_types = {TypeRef{"FVector2f"}, TypeRef{"FVector2f"}};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousInputOverloadsWithMoreThanThreeComponents) {
    auto module{valid_homogeneous_module()};
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front()).input_members = {"X"};

    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front()).input_members.clear();
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front()).components = {
        "xs", "ys", "zs", "ws"};
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front())
        .value_types.front()
        .input_types = {TypeRef{"FVector4f"}};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousInputComponentsWithDuplicateInitials) {
    auto module{valid_homogeneous_module()};
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front()).components = {
        "x_values", "x_weights"};
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front())
        .value_types.front()
        .input_types = {TypeRef{"FInput"}};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousComponentParameterCollisionsWithoutInputTypes) {
    auto module{valid_homogeneous_module()};
    std::get<codegen::HomogeneousLayoutSchema>(module.declarations.front()).components = {
        "x_values", "x_weights"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsVectorsWithMoreThanThreeComponents) {
    auto module{valid_vector_module()};
    std::get<codegen::VectorSoaSchema>(module.declarations.front()).components = {
        "xs", "ys", "zs", "ws"};
    std::get<codegen::VectorSoaSchema>(module.declarations.front()).equivalent_type =
        TypeRef{"FVector4f"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateVectorComponents) {
    auto module{valid_vector_module()};
    std::get<codegen::VectorSoaSchema>(module.declarations.front()).components = {"xs", "xs"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsVectorModulesWithoutSourceOutput) {
    auto module{valid_vector_module()};
    module.settings.source.reset();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMalformedVectorStorageIdentifiers) {
    auto module{valid_vector_module()};
    std::get<codegen::VectorSoaSchema>(module.declarations.front()).name = "F Vectors";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsVectorComponentsWithDuplicateParameterInitials) {
    auto module{valid_vector_module()};
    std::get<codegen::VectorSoaSchema>(module.declarations.front()).components = {"x_values",
                                                                                  "x_weights"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFacadesWithoutMethods) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front()).methods.clear();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownFacadeAccess) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front()).method_access = "protected";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMalformedFacadeMethodIdentifiers) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front()).methods.front().name = "bad-name";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsOutOfLineFacadesWithoutSourceOutput) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front()).definitions_in_source = true;

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, AllowsInlineFacadesInModulesWithSourceOutput) {
    auto module{valid_facade_module()};
    module.settings.source = "Facade.cpp";

    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, RejectsDuplicateFacadeMethodParameters) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front()).methods.front().parameters = {
        ParameterSchema{TypeRef{"int32"}, "value"},
        ParameterSchema{TypeRef{"float"}, "value"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateFacadeMethodSignatures) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front())
        .methods.push_back(
            std::get<codegen::FacadeSchema>(module.declarations.front()).methods.front());

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFacadeOverloadsDifferingOnlyByNoexcept) {
    auto module{valid_facade_module()};
    auto second{std::get<codegen::FacadeSchema>(module.declarations.front()).methods.front()};
    second.is_noexcept = true;
    std::get<codegen::FacadeSchema>(module.declarations.front())
        .methods.push_back(std::move(second));

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFacadeMethodsCollidingWithBind) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front()).methods = {FacadeMethodSchema{
        .name = "bind",
        .return_type = TypeRef{"void"},
        .parameters = {ParameterSchema{TypeRef{"FTarget&"}, "target"}},
    }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsNonTrailingFacadeDefaultArguments) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front()).methods.front().parameters = {
        ParameterSchema{TypeRef{"int32"}, "first", "0"},
        ParameterSchema{TypeRef{"int32"}, "second"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownFacadeValidationDependencies) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front()).validation_dependencies = {
        "missing"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidFacadeFriendAndMemberCollisions) {
    auto module{valid_facade_module()};
    std::get<codegen::FacadeSchema>(module.declarations.front()).friend_kind = "union";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_facade_module();
    std::get<codegen::FacadeSchema>(module.declarations.front()).friends = {"class"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_facade_module();
    std::get<codegen::FacadeSchema>(module.declarations.front()).target_member_name = "bind";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_facade_module();
    std::get<codegen::FacadeSchema>(module.declarations.front()).methods.front().name =
        std::get<codegen::FacadeSchema>(module.declarations.front()).target_member_name;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateUmbrellaHeaders) {
    UmbrellaModuleSchema module{
        .settings = ModuleSettings{.name = "all", .header = "All.h"},
        .headers = {"Value.h", "Value.h"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUmbrellaModulesWithoutHeaders) {
    UmbrellaModuleSchema module{
        .settings = ModuleSettings{.name = "all", .header = "All.h"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedUmbrellaSourceAndNamespaceSettings) {
    UmbrellaModuleSchema module{
        .settings = ModuleSettings{.name = "all", .header = "All.h", .source = "All.cpp"},
        .headers = {"Value.h"},
    };
    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    module.settings.source.reset();
    module.settings.namespace_name = "project";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidSettingsDefinitions) {
    auto module = SettingsModuleSchema{
        .settings =
            ModuleSettings{.name = "settings", .header = "Settings.h", .source = "Settings.cpp"},
        .api_name = "TSettingsAccess",
        .state_name = "FSettingsState",
        .categories = {{"video", "Video"}},
        .settings_list = {SettingSchema{
            .name = "vsync",
            .label = "VSync",
            .category = "missing",
            .value_type = TypeRef{"bool"},
            .backend = "engine",
            .apply_mode = SettingApplyMode::deferred,
            .control = SettingControlSchema{.kind = SettingControlKind::toggle},
        }},
    };
    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    module.settings_list.front().category = "video";
    module.settings_list.front().control.kind = SettingControlKind::choice;
    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    module.settings_list.front().control.kind = SettingControlKind::custom;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, NormalModuleReservesGeneratedNamesAcrossDeclarationKinds) {
    auto vector = VectorSoaSchema{.name = "Foo",
                                  .value_type = TypeRef{"float"},
                                  .components = {"xs", "ys"},
                                  .equivalent_type = TypeRef{"int32"}};
    auto module = normal_module({vector,
                                 EnumSchema{.name = "FooView",
                                            .underlying_type = TypeRef{"uint8"},
                                            .values = {{.name = "One"}}}},
                                "Mixed.cpp");
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = normal_module({std::move(vector),
                            RecordSchema{.name = "FooConstView",
                                         .members = {{.name = "value", .type = TypeRef{"int32"}}}}},
                           "Mixed.cpp");
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto layout = HomogeneousLayoutSchema{
        .name = "Values",
        .components = {"xs", "ys"},
        .value_types = {{.type = TypeRef{"float"},
                         .suffix = "f",
                         .equivalent_type = TypeRef{"int32"}}},
    };
    module = normal_module({std::move(layout),
                            RecordSchema{.name = "TValuesView",
                                         .members = {{.name = "value", .type = TypeRef{"int32"}}}}},
                           "Mixed.cpp");
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    layout = HomogeneousLayoutSchema{
        .name = "Values",
        .components = {"xs", "ys"},
        .value_types = {{.type = TypeRef{"float"},
                         .suffix = "f",
                         .equivalent_type = TypeRef{"int32"}}},
    };
    module = normal_module({std::move(layout),
                            RecordSchema{.name = "TValuesEquivalentType",
                                         .members = {{.name = "value", .type = TypeRef{"int32"}}}}},
                           "Mixed.cpp");
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto soa = SoaSchema{
        .name = "Data",
        .members = {{.name = "values", .kind = SoaMemberKind::array, .type = TypeRef{"int32"}}}};
    module = normal_module({std::move(soa),
                            EnumSchema{.name = "DataView",
                                       .underlying_type = TypeRef{"uint8"},
                                       .values = {{.name = "One"}}}},
                           "Mixed.cpp");
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = normal_module(
        {EnumSchema{
             .name = "State", .underlying_type = TypeRef{"uint8"}, .values = {{.name = "One"}}},
         RecordSchema{.name = "Snapshot",
                      .members = {{.name = "value", .type = TypeRef{"int32"}}}}});
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, NormalModuleAppliesSourceRequirementsPerDeclaration) {
    auto facade = FacadeSchema{.name = "InlineFacade",
                               .target_type = TypeRef{"int32"},
                               .target_member_name = "target",
                               .methods = {{.name = "reset", .return_type = TypeRef{"void"}}}};
    auto layout =
        HomogeneousLayoutSchema{.name = "Values",
                                .components = {"xs", "ys"},
                                .value_types = {{.type = TypeRef{"float"}, .suffix = "f"}}};
    auto module = normal_module({facade, layout}, "Mixed.cpp");
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));

    module = normal_module({std::move(layout)});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto source_facade = facade;
    source_facade.definitions_in_source = true;
    module = normal_module({std::move(source_facade)});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, NormalModuleCanMixIndependentEnumApis) {
    auto module = normal_module({EnumSchema{.name = "NativeMode",
                                            .underlying_type = TypeRef{"uint8"},
                                            .values = {{.name = "One"}},
                                            .native_api = true},
                                 EnumSchema{.name = "UnrealMode",
                                            .underlying_type = TypeRef{"uint8"},
                                            .values = {{.name = "One"}}}});
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, NormalModuleMatchesSoaAllocatorVariantRestrictions) {
    auto const allocator{SoaAllocatorVariant{"Custom", TypeRef{"FAllocator"}}};
    auto module{normal_soa_module(plain_soa(), {allocator}, SoaBackend::standard_library)};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = normal_soa_module(plain_soa(), {SoaAllocatorVariant{"1Bad", TypeRef{"FAllocator"}}});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto fixed{plain_soa()};
    fixed.fixed = FixedSoaSchema{"DataStorage", {}};
    module = normal_soa_module(std::move(fixed), {allocator});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto equivalent{plain_soa()};
    equivalent.equivalent_type = TypeRef{"int32"};
    module = normal_soa_module(std::move(equivalent), {allocator});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto functions{plain_soa()};
    functions.functions = {{.name = "custom", .return_type = TypeRef{"void"}}};
    module = normal_soa_module(std::move(functions), {allocator});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto mutable_functions{plain_soa()};
    mutable_functions.mutable_view_functions = {{.name = "custom", .return_type = TypeRef{"void"}}};
    module = normal_soa_module(std::move(mutable_functions), {allocator});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto declarations{plain_soa()};
    declarations.using_declarations = {"value_type = int32"};
    module = normal_soa_module(std::move(declarations), {allocator});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto nested{plain_soa()};
    nested.members = {{.name = "child",
                       .kind = SoaMemberKind::nested,
                       .type = TypeRef{"int32"},
                       .nested_schema = "Missing"}};
    module = normal_soa_module(std::move(nested), {allocator});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = NormalModuleSchema{
        .settings = ModuleSettings{.name = "mixed", .header = "Mixed.h", .source = "Mixed.cpp"},
        .declarations = {plain_soa("Data"), plain_soa("CustomData")},
        .soa_array_allocators = {allocator},
    };
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = normal_soa_module(plain_soa(), {allocator});
    EXPECT_NO_THROW(lower_modules(manifest_with(module)));
    auto const files{render_modules(lower_modules(manifest_with(std::move(module))))};
    ASSERT_EQ(files.size(), 2U);
    EXPECT_NE(files.front().content.find("struct CustomData"), std::string::npos);
}

TEST(Validation, FieldMaskSoaCannotUseArrayAllocatorVariants) {
    auto const allocator{SoaAllocatorVariant{"Custom", TypeRef{"FAllocator"}}};
    auto const masked{std::get<codegen::SoaSchema>(valid_mask_soa_module().declarations.front())};
    auto normal{normal_soa_module(masked, {allocator})};
    auto normal_manifest{manifest_with(std::move(normal))};
    EXPECT_THROW(validate_manifest(normal_manifest), std::invalid_argument);
    EXPECT_THROW(lower_modules(normal_manifest), std::invalid_argument);

    auto legacy{valid_mask_soa_module()};
    legacy.soa_array_allocators = {allocator};
    auto legacy_manifest{manifest_with(std::move(legacy))};
    EXPECT_THROW(validate_manifest(legacy_manifest), std::invalid_argument);
}

TEST(Validation, NormalModuleReservesFixedSoaStorageAndContainerNames) {
    auto fixed{plain_soa()};
    fixed.fixed = FixedSoaSchema{"DataStorage", {"FixedData"}};
    auto module =
        normal_module({fixed,
                       RecordSchema{.name = "DataStorage",
                                    .members = {{.name = "value", .type = TypeRef{"int32"}}}}},
                      "Mixed.cpp");
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = normal_module({std::move(fixed),
                            RecordSchema{.name = "FixedData",
                                         .members = {{.name = "value", .type = TypeRef{"int32"}}}}},
                           "Mixed.cpp");
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto vector = VectorSoaSchema{.name = "Vectors",
                                  .value_type = TypeRef{"float"},
                                  .components = {"xs", "ys"},
                                  .equivalent_type = TypeRef{"int32"},
                                  .fixed = FixedSoaSchema{"VectorStorage", {"FixedVectors"}}};
    module = normal_module({std::move(vector),
                            RecordSchema{.name = "FixedVectors",
                                         .members = {{.name = "value", .type = TypeRef{"int32"}}}}},
                           "Mixed.cpp");
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    auto valid{plain_soa()};
    valid.fixed = FixedSoaSchema{"DataStorage", {"FixedData"}};
    module = normal_module({std::move(valid),
                            RecordSchema{.name = "Snapshot",
                                         .members = {{.name = "value", .type = TypeRef{"int32"}}}}},
                           "Mixed.cpp");
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

} // namespace
} // namespace codegen
