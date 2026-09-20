#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>

namespace codegen {
namespace {

template <typename T>
auto manifest_with(T module, std::map<std::string, CppType> types = {}) -> Manifest {
    return Manifest{
        .schema_version = manifest_schema_version,
        .types = std::move(types),
        .modules = {ModuleSchema{std::move(module)}},
    };
}

auto valid_soa_module() -> SoaModuleSchema {
    return SoaModuleSchema{
        .settings = ModuleSettings{.name = "soa", .header = "Soa.h", .source = "Soa.cpp"},
        .structs = {SoaSchema{
            .name = "FData",
            .members =
                {
                    SoaMemberSchema{"values", SoaMemberKind::array, TypeRef{"int32"}},
                },
        }},
    };
}

auto valid_mask_soa_module() -> SoaModuleSchema {
    auto module{valid_soa_module()};
    auto& schema{module.structs.front()};
    schema.field_mask_name = "FFieldMask";
    schema.field_enum_name = "EField";
    schema.members.insert(schema.members.begin(),
                          SoaMemberSchema{"masks", SoaMemberKind::array, TypeRef{"FFieldMask"}});
    schema.members.back().mask_field = true;
    return module;
}

auto valid_vector_module() -> VectorModuleSchema {
    return VectorModuleSchema{
        .settings =
            ModuleSettings{.name = "vectors", .header = "Vectors.h", .source = "Vectors.cpp"},
        .storage_name = "FVectors",
        .value_type = TypeRef{"float"},
        .components = {"xs", "ys"},
        .equivalent_type = TypeRef{"FVector2f"},
    };
}

auto valid_facade_module() -> FacadeModuleSchema {
    return FacadeModuleSchema{
        .settings = ModuleSettings{.name = "facade", .header = "Facade.h"},
        .facade =
            FacadeSchema{
                .name = "FFacade",
                .target_type = TypeRef{"FTarget"},
                .target_member_name = "target",
                .methods = {FacadeMethodSchema{
                    .name = "reset",
                    .return_type = TypeRef{"void"},
                }},
            },
    };
}

auto valid_homogeneous_module() -> HomogeneousModuleSchema {
    return HomogeneousModuleSchema{
        .settings =
            ModuleSettings{
                .name = "homogeneous",
                .header = "Values.h",
                .source = "Values.cpp",
            },
        .layouts = {HomogeneousLayoutSchema{
            .name = "Values",
            .components = {"xs", "ys"},
            .value_types = {HomogeneousValueSchema{TypeRef{"float"}, "f"}},
        }},
    };
}

auto valid_enum_module() -> EnumModuleSchema {
    return EnumModuleSchema{
        .settings =
            ModuleSettings{
                .name = "enums",
                .header = "Enums.h",
                .source = "Enums.cpp",
            },
        .helper_namespace = "project",
        .enums = {EnumSchema{
            .name = "EMode",
            .underlying_type = TypeRef{"uint8"},
            .reflection = EnumReflection::uenum,
            .values = {EnumeratorSchema{"Value"}},
            .conversions = {EnumConversion::string_view},
        }},
    };
}

auto valid_native_enum_module() -> EnumModuleSchema {
    return EnumModuleSchema{
        .settings =
            ModuleSettings{
                .name = "native_enums",
                .header = "NativeEnums.h",
                .namespace_name = "project",
            },
        .enums = {EnumSchema{
            .name = "NativeMode",
            .underlying_type = TypeRef{"std::uint8_t"},
            .values = {EnumeratorSchema{"Idle", "0", std::nullopt, false, "idle"},
                       EnumeratorSchema{"COUNT", "1", std::nullopt, true}},
            .count = "COUNT",
            .native_api = true,
        }},
    };
}

auto valid_integer_scalar_module() -> ScalarModuleSchema {
    return ScalarModuleSchema{
        .settings = ModuleSettings{.name = "scalars", .header = "Scalars.h"},
        .scalars = {IntegerScalarSchema{
            .name = "DamageReason",
            .signedness = false,
            .minimum_value = 0,
            .maximum_value = 10,
            .bit_width = std::nullopt,
            .named_codes = {{.name = "Unknown", .value = 0, .sentinel = false},
                            {.name = "Invalid", .value = 15, .sentinel = true}},
        }},
    };
}

auto valid_linear_quantized_manifest() -> Manifest {
    auto scalar{valid_integer_scalar_module()};
    RepresentationModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .linear_quantized = {LinearQuantizedSchema{
            .name = "DamageReasonQ4",
            .source = TypeRef{"DamageReason"},
            .bit_width = 4,
            .reserved_codes = 1,
            .clipping = QuantizationClipping::reject,
        }},
        .integer_varints = {},
        .fixed_points = {},
        .optional_sentinels = {},
    };
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(scalar), std::move(representations)}};
}

auto valid_integer_varint_manifest(
    bool const signedness = false,
    IntegerVarintEncoding const encoding = IntegerVarintEncoding::unsigned_varint) -> Manifest {
    auto scalar{valid_integer_scalar_module()};
    if (signedness) {
        scalar.scalars.front().signedness = true;
        scalar.scalars.front().minimum_value = -100;
        scalar.scalars.front().maximum_value = 100;
        scalar.scalars.front().named_codes.clear();
    }
    RepresentationModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .linear_quantized = {},
        .integer_varints = {IntegerVarintSchema{
            .name = "DamageReasonVarint", .source = TypeRef{"DamageReason"}, .encoding = encoding}},
        .fixed_points = {},
        .optional_sentinels = {},
    };
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(scalar), std::move(representations)}};
}

auto valid_fixed_point_manifest(bool const signedness = true) -> Manifest {
    RepresentationModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .linear_quantized = {},
        .integer_varints = {},
        .fixed_points = {FixedPointSchema{.name = "VelocityQ12_4",
                                          .signedness = signedness,
                                          .total_bits = 16,
                                          .fractional_bits = 4,
                                          .rounding = FixedPointRounding::nearest_even}},
        .optional_sentinels = {},
    };
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(representations)}};
}

auto valid_mini_float_manifest() -> Manifest {
    RepresentationModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .linear_quantized = {},
        .integer_varints = {},
        .fixed_points = {},
        .optional_sentinels = {},
        .optional_presence_bits = {},
        .mini_floats = {MiniFloatSchema{.name = "CompactFloat",
                                        .sign_bits = 1,
                                        .exponent_bits = 5,
                                        .significand_bits = 10,
                                        .exponent_bias = 15}},
    };
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(representations)}};
}

auto valid_optional_sentinel_manifest() -> Manifest {
    auto scalar{valid_integer_scalar_module()};
    RepresentationModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .linear_quantized = {},
        .integer_varints = {},
        .fixed_points = {},
        .optional_sentinels = {OptionalSentinelSchema{.name = "OptionalDamageReason",
                                                      .source = TypeRef{"DamageReason"},
                                                      .sentinel = "Invalid"}},
    };
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(scalar), std::move(representations)}};
}

auto valid_optional_presence_bit_manifest() -> Manifest {
    auto scalar{valid_integer_scalar_module()};
    RepresentationModuleSchema representations{
        .settings = ModuleSettings{.name = "representations", .header = "Representations.h"},
        .linear_quantized = {},
        .integer_varints = {},
        .fixed_points = {},
        .optional_sentinels = {},
        .optional_presence_bits = {OptionalPresenceBitSchema{.name = "PresentDamageReason",
                                                             .source = TypeRef{"DamageReason"}}},
    };
    return Manifest{.schema_version = manifest_schema_version,
                    .types = {},
                    .modules = {std::move(scalar), std::move(representations)}};
}

auto valid_static_table_module() -> StaticTableModuleSchema {
    return StaticTableModuleSchema{
        .settings = ModuleSettings{.name = "tables", .header = "Tables.h"},
        .tables = {StaticTableSchema{
            .name = "FValues",
            .rows = {StaticTableRowSchema{"first"}, StaticTableRowSchema{"second"}},
            .columns = {StaticTableColumnSchema{"ids", TypeRef{"int32"}}},
        }},
    };
}

TEST(Validation, RejectsEmptyModuleNames) {
    auto module{valid_soa_module()};
    module.settings.name.clear();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumDefinitions) {
    auto module{valid_enum_module()};
    module.enums.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().values.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().values.push_back(EnumeratorSchema{"Value"});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().values.front().name = "bad-name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().underlying_type = TypeRef{"@missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsEnumSemanticWidthsThatCannotRepresentKnownValues) {
    auto module{valid_enum_module()};
    module.enums.front().reflection = EnumReflection::none;
    module.enums.front().bit_width = 0;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().reflection = EnumReflection::none;
    module.enums.front().bit_width = 65;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().reflection = EnumReflection::none;
    module.enums.front().values = {EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Seven", "7"}};
    module.enums.front().bit_width = 2;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().reflection = EnumReflection::none;
    module.enums.front().values = {EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Seven", "7"}};
    module.enums.front().bit_width = 3;
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, AppliesExplicitEnumSignednessToTheSemanticDomain) {
    auto module{valid_enum_module()};
    module.enums.front().reflection = EnumReflection::none;
    module.enums.front().values = {EnumeratorSchema{"Negative", "-1"},
                                   EnumeratorSchema{"Positive", "1"}};
    module.enums.front().signedness = false;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().reflection = EnumReflection::none;
    module.enums.front().values = {EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Seven", "7"}};
    module.enums.front().signedness = true;
    module.enums.front().bit_width = 3;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().reflection = EnumReflection::none;
    module.enums.front().values = {EnumeratorSchema{"Zero", "0"}, EnumeratorSchema{"Seven", "7"}};
    module.enums.front().signedness = true;
    module.enums.front().bit_width = 4;
    EXPECT_NO_THROW(lower_modules(manifest_with(std::move(module))));
}

TEST(Validation, RejectsInvalidIntegerScalarDomainsAndNamedCodes) {
    auto module{valid_integer_scalar_module()};
    module.scalars.front().bit_width = 3;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    module.scalars.front().minimum_value = -1;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    module.scalars.front().named_codes[1].value = 10;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    module.scalars.front().named_codes[0].value = 11;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    module.scalars.front().named_codes[1].name = "Unknown";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    module.scalars.front().named_codes[1].value = 0;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    module.settings.source = "Scalars.cpp";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_integer_scalar_module();
    auto const lowered{lower_modules(manifest_with(std::move(module)))};
    ASSERT_EQ(lowered.size(), 1U);
    ASSERT_TRUE(lowered.front().header.has_value());
    EXPECT_EQ(lowered.front().header->path, "Scalars.h");
    EXPECT_TRUE(lowered.front().header->nodes.empty());
}

TEST(Validation, ValidatesLinearQuantizedRepresentationsAndEmitsOnlyConfiguredHeader) {
    auto manifest{valid_linear_quantized_manifest()};
    auto lowered{lower_modules(manifest)};
    ASSERT_EQ(lowered.size(), 2U);
    ASSERT_TRUE(lowered[1].header.has_value());
    EXPECT_EQ(lowered[1].header->path, "Representations.h");
    EXPECT_TRUE(lowered[1].header->nodes.empty());

    manifest = valid_linear_quantized_manifest();
    auto& signed_source{std::get<ScalarModuleSchema>(manifest.modules[0]).scalars.front()};
    signed_source.signedness = true;
    signed_source.minimum_value = -100;
    signed_source.maximum_value = 100;
    signed_source.named_codes.clear();
    EXPECT_NO_THROW(lower_modules(manifest));

    manifest = valid_linear_quantized_manifest();
    std::get<RepresentationModuleSchema>(manifest.modules[1]).linear_quantized.front().bit_width =
        1;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest();
    auto& representation{
        std::get<RepresentationModuleSchema>(manifest.modules[1]).linear_quantized.front()};
    representation.bit_width = 2;
    representation.reserved_codes = 3;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest();
    std::get<ScalarModuleSchema>(manifest.modules[0]).scalars.front().maximum_value = 0;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest();
    std::get<RepresentationModuleSchema>(manifest.modules[1]).linear_quantized.front().source =
        TypeRef{"std::uint32_t"};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_linear_quantized_manifest();
    std::get<RepresentationModuleSchema>(manifest.modules[1]).settings.source =
        "Representations.cpp";
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
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
    std::get<RepresentationModuleSchema>(manifest.modules[1]).integer_varints.front().source =
        TypeRef{"std::uint32_t"};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_integer_varint_manifest();
    auto& representations{std::get<RepresentationModuleSchema>(manifest.modules[1])};
    representations.linear_quantized.push_back(
        LinearQuantizedSchema{.name = representations.integer_varints.front().name,
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
    EXPECT_TRUE(lowered.front().header->nodes.empty());

    manifest = valid_fixed_point_manifest(false);
    auto& unsigned_fixed{
        std::get<RepresentationModuleSchema>(manifest.modules.front()).fixed_points.front()};
    unsigned_fixed.total_bits = 8;
    unsigned_fixed.fractional_bits = 8;
    EXPECT_NO_THROW(lower_modules(manifest));

    manifest = valid_fixed_point_manifest();
    std::get<RepresentationModuleSchema>(manifest.modules.front())
        .fixed_points.front()
        .fractional_bits = 16;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_fixed_point_manifest();
    std::get<RepresentationModuleSchema>(manifest.modules.front()).fixed_points.front().total_bits =
        0;
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_fixed_point_manifest();
    auto& representations{std::get<RepresentationModuleSchema>(manifest.modules.front())};
    representations.fixed_points.push_back(representations.fixed_points.front());
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, ValidatesMiniFloatEncodingAndEmitsNoPhysicalType) {
    auto manifest{valid_mini_float_manifest()};
    auto lowered{lower_modules(manifest)};
    ASSERT_EQ(lowered.size(), 1U);
    ASSERT_TRUE(lowered.front().header.has_value());
    EXPECT_EQ(lowered.front().header->path, "Representations.h");
    EXPECT_TRUE(lowered.front().header->nodes.empty());

    auto invalidate = [](auto edit) {
        auto invalid{valid_mini_float_manifest()};
        edit(std::get<RepresentationModuleSchema>(invalid.modules.front()).mini_floats.front());
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
    auto& representations{std::get<RepresentationModuleSchema>(manifest.modules.front())};
    representations.fixed_points.push_back(FixedPointSchema{
        .name = representations.mini_floats.front().name,
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
    auto& optional{
        std::get<RepresentationModuleSchema>(manifest.modules[1]).optional_sentinels.front()};
    optional.source = TypeRef{"std::uint32_t"};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest();
    std::get<RepresentationModuleSchema>(manifest.modules[1]).optional_sentinels.front().sentinel =
        "Missing";
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest();
    std::get<RepresentationModuleSchema>(manifest.modules[1]).optional_sentinels.front().sentinel =
        "Unknown";
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest();
    std::get<ScalarModuleSchema>(manifest.modules[0]).scalars.front().named_codes.clear();
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_sentinel_manifest();
    auto& representations{std::get<RepresentationModuleSchema>(manifest.modules[1])};
    representations.fixed_points.push_back(FixedPointSchema{
        .name = representations.optional_sentinels.front().name,
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
    std::get<RepresentationModuleSchema>(manifest.modules[1])
        .optional_presence_bits.front()
        .source = TypeRef{"std::uint32_t"};
    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);

    manifest = valid_optional_presence_bit_manifest();
    auto& representations{std::get<RepresentationModuleSchema>(manifest.modules[1])};
    representations.optional_sentinels.push_back(
        OptionalSentinelSchema{.name = representations.optional_presence_bits.front().name,
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
    module.helper_namespace = "bad-name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().conversions.push_back(EnumConversion::string_view);
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().reflection = EnumReflection::none;
    module.enums.front().values.front().hidden = true;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedNativeEnumCombinations) {
    auto module{valid_native_enum_module()};
    module.enums.push_back(valid_enum_module().enums.front());
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    module.enums.front().reflection = EnumReflection::uenum;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    module.enums.front().enum_array = true;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    module.enums.front().conversions = {EnumConversion::string_view};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    module.enums.front().export_specifier = "PROJECT_API";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_native_enum_module();
    module.enums.front().unreal_projection = EnumUnrealProjection{
        .name = "ENativeMode",
        .header = "Project/NativeMode.h",
        .header_include = "Project/NativeMode.h",
        .conversion_header = "Project/NativeModeConversion.h",
        .native_header_include = "project/NativeEnums.h",
        .reflection = EnumReflection::none,
    };
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().unreal_projection = EnumUnrealProjection{
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
    module.enums.front().conversions.push_back(EnumConversion::try_parse_serialized);
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().values.front().serialized_name = "value";
    module.enums.front().values.push_back(
        EnumeratorSchema{"Other", std::nullopt, std::nullopt, false, "value"});
    module.enums.front().conversions.push_back(EnumConversion::try_parse_serialized);
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidEnumArrayDefinitions) {
    auto module{valid_enum_module()};
    module.enums.front().enum_array = true;
    module.enums.front().values.front().initializer = "0";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().enum_array = true;
    module.enums.front().values.front().hidden = true;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().count = "Value";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().enum_array = true;
    module.enums.front().count = "Missing";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().enum_array = true;
    module.enums.front().values.push_back(
        EnumeratorSchema{"COUNT", std::nullopt, std::nullopt, true});
    module.enums.front().values.push_back(EnumeratorSchema{"After"});
    module.enums.front().count = "COUNT";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().enum_array = true;
    module.enums.front().values.push_back(EnumeratorSchema{"COUNT"});
    module.enums.front().count = "COUNT";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().enum_array = true;
    module.enums.front().values = {EnumeratorSchema{"COUNT", std::nullopt, std::nullopt, true}};
    module.enums.front().count = "COUNT";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, SupportsSignedEnumValuesWhenTheyFitTheUnderlyingType) {
    auto module{valid_native_enum_module()};
    auto& schema{module.enums.front()};
    schema.underlying_type = TypeRef{"@native_int8"};
    schema.values = {EnumeratorSchema{"Minimum", "-128"}, EnumeratorSchema{"Maximum", "127"}};
    schema.count.reset();
    auto const types{std::map<std::string, CppType>{
        {"native_int8", CppType{"std::int8_t", "cstdint"}},
    }};

    EXPECT_NO_THROW(static_cast<void>(lower_modules(manifest_with(module, types))));

    schema.values.back().initializer = "128";
    EXPECT_THROW(static_cast<void>(lower_modules(manifest_with(std::move(module), types))),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableModuleConfiguration) {
    auto module{valid_static_table_module()};
    module.tables.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    module.settings.source = "Tables.cpp";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    module.tables.front().rows.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    module.tables.front().columns.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidStaticTableNamesAndTypes) {
    auto module{valid_static_table_module()};
    module.tables.push_back(module.tables.front());
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    module.tables.front().rows.push_back(StaticTableRowSchema{"first"});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    module.tables.front().rows.front().name = "bad-name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    module.tables.front().columns.push_back(module.tables.front().columns.front());
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_static_table_module();
    module.tables.front().columns.front().type = TypeRef{"@missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsStaticTableColumnsCollidingWithGeneratedApis) {
    for (auto const& name :
         {"FValues", "num_rows", "num", "apply_arrays", "apply_array_pairs", "first_index"}) {
        SCOPED_TRACE(name);
        auto module{valid_static_table_module()};
        module.tables.front().columns.front().name = name;
        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }
}

TEST(Validation, RejectsInvalidStaticTableGroups) {
    auto module{valid_static_table_module()};
    module.tables.front().groups = {StaticTableGroupSchema{"point", TypeRef{"FPoint"}, {"ids"}}};

    auto duplicate_group{module};
    duplicate_group.tables.front().groups.push_back(duplicate_group.tables.front().groups.front());
    EXPECT_THROW(lower_modules(manifest_with(std::move(duplicate_group))), std::invalid_argument);

    auto invalid_name{module};
    invalid_name.tables.front().groups.front().name = "bad-name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(invalid_name))), std::invalid_argument);

    auto unknown_type{module};
    unknown_type.tables.front().groups.front().type = TypeRef{"@missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(unknown_type))), std::invalid_argument);

    auto empty_columns{module};
    empty_columns.tables.front().groups.front().columns.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(empty_columns))), std::invalid_argument);

    auto unknown_column{module};
    unknown_column.tables.front().groups.front().columns = {"missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(unknown_column))), std::invalid_argument);

    auto duplicate_column{module};
    duplicate_column.tables.front().groups.front().columns = {"ids", "ids"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(duplicate_column))), std::invalid_argument);
}

TEST(Validation, RejectsStaticTableGroupGetterCollisions) {
    auto module{valid_static_table_module()};
    module.tables.front().columns.push_back(StaticTableColumnSchema{"get_point", TypeRef{"float"}});
    module.tables.front().groups = {StaticTableGroupSchema{"point", TypeRef{"FPoint"}, {"ids"}}};

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
        module.structs.front().members.front().name = invalid;
        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }

    auto module{valid_soa_module()};
    module.settings.namespace_name = "project::namespace";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsEmptyTypeSpellings) {
    auto module{valid_soa_module()};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module), {{"empty", CppType{}}})),
                 std::invalid_argument);
}

TEST(Validation, RejectsInvalidTypeDependencyAndOperationSpellings) {
    auto empty_header{CppType{"FValue", ""}};
    EXPECT_THROW(
        lower_modules(manifest_with(valid_soa_module(), {{"value", std::move(empty_header)}})),
        std::invalid_argument);

    CppType invalid_operation{"FValue"};
    invalid_operation.member_operations.emplace(TypeOperation::remove_at_swap, "bad-name");
    EXPECT_THROW(
        lower_modules(manifest_with(valid_soa_module(), {{"value", std::move(invalid_operation)}})),
        std::invalid_argument);
}

TEST(Validation, RejectsSoaModulesWithoutSchemas) {
    SoaModuleSchema module{
        .settings = ModuleSettings{.name = "soa", .header = "Soa.h"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsEmptySoaNames) {
    auto module{valid_soa_module()};
    module.structs.front().name.clear();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateSoaMembers) {
    auto module{valid_soa_module()};
    module.structs.front().members.push_back(
        SoaMemberSchema{"values", SoaMemberKind::array, TypeRef{"float"}});

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMalformedSoaMemberIdentifiers) {
    auto module{valid_soa_module()};
    module.structs.front().members.front().name = "bad-name";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsIncompleteSoaFieldMaskDeclarations) {
    auto module{valid_mask_soa_module()};
    module.structs.front().field_enum_name.reset();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_mask_soa_module();
    module.structs.front().field_mask_name.reset();
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidSoaFieldMaskMembers) {
    auto module{valid_mask_soa_module()};
    module.structs.front().members.front().type = TypeRef{"uint8"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_mask_soa_module();
    module.structs.front().members.back().mask_field = false;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    module.structs.front().members.front().mask_field = true;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidSoaMaskDimensions) {
    auto module{valid_mask_soa_module()};
    auto& member{module.structs.front().members.back()};
    member.mask_dimensions.push_back({"field_index", "4"});
    member.mask_dimensions.push_back({"field_index", "2"});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_mask_soa_module();
    module.structs.front().members.back().mask_dimensions.push_back({"bad-index", "4"});
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMembersCollidingWithGeneratedStorageApis) {
    for (auto const& name : {"num", "reset", "copy_element"}) {
        SCOPED_TRACE(name);
        auto module{valid_soa_module()};
        module.structs.front().members.front().name = name;
        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }

    auto homogeneous{valid_homogeneous_module()};
    homogeneous.layouts.front().components.front() = "add";
    EXPECT_THROW(lower_modules(manifest_with(std::move(homogeneous))), std::invalid_argument);

    auto vector{valid_vector_module()};
    vector.components.front() = "get_view";
    EXPECT_THROW(lower_modules(manifest_with(std::move(vector))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownSoaTypeReferences) {
    auto module{valid_soa_module()};
    module.structs.front().members.front().type = TypeRef{"@missing"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFixedSchemasOnArrayMembers) {
    auto module{valid_soa_module()};
    module.structs.front().members.front().fixed_schema = "FChild";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsOpaqueNestedMembersInFixedSoas) {
    auto module{valid_soa_module()};
    auto& schema{module.structs.front()};
    schema.fixed = FixedSoaSchema{"TDataStorage", {}};
    schema.members = {
        SoaMemberSchema{"child", SoaMemberKind::nested, TypeRef{"FChild"}},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownNestedFixedSchemas) {
    auto module{valid_soa_module()};
    auto& schema{module.structs.front()};
    schema.fixed = FixedSoaSchema{"TDataStorage", {}};
    schema.members = {
        SoaMemberSchema{"child", SoaMemberKind::nested, TypeRef{"FChild"}, "FChild"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFixedSchemaCycles) {
    auto module{valid_soa_module()};
    auto& schema{module.structs.front()};
    schema.fixed = FixedSoaSchema{"TDataStorage", {}};
    schema.members = {
        SoaMemberSchema{"children", SoaMemberKind::nested, TypeRef{"FData"}, "FData"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsCollidingGeneratedSoaTypeNames) {
    auto module{valid_soa_module()};
    auto second{module.structs.front()};
    second.name = "FDataView";
    module.structs.push_back(std::move(second));

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateFixedStorageAndContainerNames) {
    auto module{valid_soa_module()};
    auto& first{module.structs.front()};
    first.fixed = FixedSoaSchema{"TShared", {"TFixedData"}};
    auto second{first};
    second.name = "FOther";
    second.fixed = FixedSoaSchema{"TShared", {"TFixedOther"}};
    module.structs.push_back(std::move(second));

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    module.structs.front().fixed = FixedSoaSchema{"TShared", {"TShared"}};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsSoaModulesWithoutSourceOutput) {
    auto module{valid_soa_module()};
    module.settings.source.reset();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsEmptyOptionalSoaViewNames) {
    auto module{valid_soa_module()};
    module.structs.front().view_name = "";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsBlankSoaUsingDeclarations) {
    for (auto const* declaration : {"", " \t\r\n"}) {
        SCOPED_TRACE(declaration);
        auto module{valid_soa_module()};
        module.structs.front().using_declarations.emplace_back(declaration);

        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }
}

TEST(Validation, RejectsDuplicateCustomSoaFunctionParameters) {
    auto module{valid_soa_module()};
    module.structs.front().functions = {FunctionSchema{
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
    module.structs.front().functions = {FunctionSchema{
        .name = "set",
        .return_type = TypeRef{"void"},
        .parameters =
            {
                ParameterSchema{TypeRef{"int32"}, "first", "0"},
                ParameterSchema{TypeRef{"int32"}, "second"},
            },
    }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownCustomSoaDependencies) {
    auto module{valid_soa_module()};
    module.structs.front().functions = {FunctionSchema{
        .name = "reset_values",
        .return_type = TypeRef{"void"},
        .dependencies = {"missing"},
    }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidCustomSoaFunctionDefinitions) {
    auto module{valid_soa_module()};
    module.structs.front().functions = {FunctionSchema{
        .name = "class",
        .return_type = TypeRef{"void"},
        .is_inline = true,
    }};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    module.structs.front().functions = {FunctionSchema{
        .name = "update",
        .return_type = TypeRef{"void"},
        .is_inline = true,
        .definition_in_source = true,
    }};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_soa_module();
    module.structs.front().functions = {FunctionSchema{
        .name = "invalid_static_const",
        .return_type = TypeRef{"void"},
        .is_const = true,
        .is_static = true,
    }};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsCustomSoaFunctionsCollidingWithGeneratedApisAndTypeNames) {
    for (auto const& name : {"sort", "get_view", "FData"}) {
        SCOPED_TRACE(name);
        auto module{valid_soa_module()};
        module.structs.front().functions = {FunctionSchema{
            .name = name,
            .return_type = TypeRef{"void"},
            .is_inline = true,
        }};
        EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
    }
}

TEST(Validation, RejectsMalformedExportSpecifiers) {
    auto soa{valid_soa_module()};
    soa.structs.front().export_specifier = "bad-specifier";
    EXPECT_THROW(lower_modules(manifest_with(std::move(soa))), std::invalid_argument);

    auto homogeneous{valid_homogeneous_module()};
    homogeneous.layouts.front().export_specifier = "class";
    EXPECT_THROW(lower_modules(manifest_with(std::move(homogeneous))), std::invalid_argument);

    auto facade{valid_facade_module()};
    facade.facade.export_specifier = "two words";
    EXPECT_THROW(lower_modules(manifest_with(std::move(facade))), std::invalid_argument);

    auto table{valid_static_table_module()};
    table.tables.front().export_specifier = "bad-specifier";
    EXPECT_THROW(lower_modules(manifest_with(std::move(table))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousModulesWithoutLayouts) {
    HomogeneousModuleSchema module{
        .settings =
            ModuleSettings{.name = "homogeneous", .header = "Values.h", .source = "Values.cpp"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousComponents) {
    HomogeneousModuleSchema module{
        .settings =
            ModuleSettings{.name = "homogeneous", .header = "Values.h", .source = "Values.cpp"},
        .layouts = {HomogeneousLayoutSchema{
            .name = "Values",
            .components = {"xs", "xs"},
            .value_types = {HomogeneousValueSchema{TypeRef{"float"}, "f"}},
        }},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousLayoutsWithoutValueTypes) {
    HomogeneousModuleSchema module{
        .settings =
            ModuleSettings{.name = "homogeneous", .header = "Values.h", .source = "Values.cpp"},
        .layouts = {HomogeneousLayoutSchema{
            .name = "Values",
            .components = {"xs"},
        }},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousModulesWithoutSourceOutput) {
    auto module{valid_homogeneous_module()};
    module.settings.source.reset();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousLayoutNames) {
    auto module{valid_homogeneous_module()};
    module.layouts.push_back(module.layouts.front());

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousValueSuffixes) {
    auto module{valid_homogeneous_module()};
    module.layouts.front().value_types.push_back(HomogeneousValueSchema{TypeRef{"double"}, "f"});

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMalformedHomogeneousSuffixes) {
    auto module{valid_homogeneous_module()};
    module.layouts.front().value_types.front().suffix = "-float";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsCollidingHomogeneousStorageNames) {
    auto module{valid_homogeneous_module()};
    module.layouts = {
        HomogeneousLayoutSchema{
            .name = "Value",
            .components = {"xs"},
            .value_types = {HomogeneousValueSchema{TypeRef{"float"}, "sf"}},
        },
        HomogeneousLayoutSchema{
            .name = "Values",
            .components = {"xs"},
            .value_types = {HomogeneousValueSchema{TypeRef{"float"}, "f"}},
        },
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousEquivalentSpecialisations) {
    auto module{valid_homogeneous_module()};
    module.layouts.front().value_types = {
        HomogeneousValueSchema{TypeRef{"float"}, "f", TypeRef{"FVector2f"}},
        HomogeneousValueSchema{TypeRef{"float"}, "other", TypeRef{"FOtherVector2f"}},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousInputTypes) {
    auto module{valid_homogeneous_module()};
    module.layouts.front().value_types.front().input_types = {TypeRef{"FVector2f"},
                                                              TypeRef{"FVector2f"}};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousInputOverloadsWithMoreThanThreeComponents) {
    auto module{valid_homogeneous_module()};
    module.layouts.front().input_members = {"X"};

    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    module.layouts.front().input_members.clear();
    module.layouts.front().components = {"xs", "ys", "zs", "ws"};
    module.layouts.front().value_types.front().input_types = {TypeRef{"FVector4f"}};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousInputComponentsWithDuplicateInitials) {
    auto module{valid_homogeneous_module()};
    module.layouts.front().components = {"x_values", "x_weights"};
    module.layouts.front().value_types.front().input_types = {TypeRef{"FInput"}};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsHomogeneousComponentParameterCollisionsWithoutInputTypes) {
    auto module{valid_homogeneous_module()};
    module.layouts.front().components = {"x_values", "x_weights"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsVectorsWithMoreThanThreeComponents) {
    auto module{valid_vector_module()};
    module.components = {"xs", "ys", "zs", "ws"};
    module.equivalent_type = TypeRef{"FVector4f"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateVectorComponents) {
    auto module{valid_vector_module()};
    module.components = {"xs", "xs"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsVectorModulesWithoutSourceOutput) {
    auto module{valid_vector_module()};
    module.settings.source.reset();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMalformedVectorStorageIdentifiers) {
    auto module{valid_vector_module()};
    module.storage_name = "F Vectors";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsVectorComponentsWithDuplicateParameterInitials) {
    auto module{valid_vector_module()};
    module.components = {"x_values", "x_weights"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFacadesWithoutMethods) {
    auto module{valid_facade_module()};
    module.facade.methods.clear();

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownFacadeAccess) {
    auto module{valid_facade_module()};
    module.facade.method_access = "protected";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsMalformedFacadeMethodIdentifiers) {
    auto module{valid_facade_module()};
    module.facade.methods.front().name = "bad-name";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsOutOfLineFacadesWithoutSourceOutput) {
    auto module{valid_facade_module()};
    module.facade.definitions_in_source = true;

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInlineFacadesWithSourceOutput) {
    auto module{valid_facade_module()};
    module.settings.source = "Facade.cpp";

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateFacadeMethodParameters) {
    auto module{valid_facade_module()};
    module.facade.methods.front().parameters = {
        ParameterSchema{TypeRef{"int32"}, "value"},
        ParameterSchema{TypeRef{"float"}, "value"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateFacadeMethodSignatures) {
    auto module{valid_facade_module()};
    module.facade.methods.push_back(module.facade.methods.front());

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFacadeOverloadsDifferingOnlyByNoexcept) {
    auto module{valid_facade_module()};
    auto second{module.facade.methods.front()};
    second.is_noexcept = true;
    module.facade.methods.push_back(std::move(second));

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsFacadeMethodsCollidingWithBind) {
    auto module{valid_facade_module()};
    module.facade.methods = {FacadeMethodSchema{
        .name = "bind",
        .return_type = TypeRef{"void"},
        .parameters = {ParameterSchema{TypeRef{"FTarget&"}, "target"}},
    }};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsNonTrailingFacadeDefaultArguments) {
    auto module{valid_facade_module()};
    module.facade.methods.front().parameters = {
        ParameterSchema{TypeRef{"int32"}, "first", "0"},
        ParameterSchema{TypeRef{"int32"}, "second"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnknownFacadeValidationDependencies) {
    auto module{valid_facade_module()};
    module.facade.validation_dependencies = {"missing"};

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsInvalidFacadeFriendAndMemberCollisions) {
    auto module{valid_facade_module()};
    module.facade.friend_kind = "union";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_facade_module();
    module.facade.friends = {"class"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_facade_module();
    module.facade.target_member_name = "bind";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_facade_module();
    module.facade.methods.front().name = module.facade.target_member_name;
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

} // namespace
} // namespace codegen
