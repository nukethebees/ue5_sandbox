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
        .settings = ModuleSettings{
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

TEST(Validation, RejectsInvalidEnumArrayDefinitions) {
    auto module{valid_enum_module()};
    module.enums.front().enum_array = true;
    module.enums.front().values.front().initializer = "0";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);

    module = valid_enum_module();
    module.enums.front().enum_array = true;
    module.enums.front().values.front().hidden = true;
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
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
    module.tables.front().groups = {
        StaticTableGroupSchema{"point", TypeRef{"FPoint"}, {"ids"}}};

    auto duplicate_group{module};
    duplicate_group.tables.front().groups.push_back(
        duplicate_group.tables.front().groups.front());
    EXPECT_THROW(lower_modules(manifest_with(std::move(duplicate_group))),
                 std::invalid_argument);

    auto invalid_name{module};
    invalid_name.tables.front().groups.front().name = "bad-name";
    EXPECT_THROW(lower_modules(manifest_with(std::move(invalid_name))), std::invalid_argument);

    auto unknown_type{module};
    unknown_type.tables.front().groups.front().type = TypeRef{"@missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(unknown_type))), std::invalid_argument);

    auto empty_columns{module};
    empty_columns.tables.front().groups.front().columns.clear();
    EXPECT_THROW(lower_modules(manifest_with(std::move(empty_columns))),
                 std::invalid_argument);

    auto unknown_column{module};
    unknown_column.tables.front().groups.front().columns = {"missing"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(unknown_column))),
                 std::invalid_argument);

    auto duplicate_column{module};
    duplicate_column.tables.front().groups.front().columns = {"ids", "ids"};
    EXPECT_THROW(lower_modules(manifest_with(std::move(duplicate_column))),
                 std::invalid_argument);
}

TEST(Validation, RejectsStaticTableGroupGetterCollisions) {
    auto module{valid_static_table_module()};
    module.tables.front().columns.push_back(
        StaticTableColumnSchema{"get_point", TypeRef{"float"}});
    module.tables.front().groups = {
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

} // namespace
} // namespace codegen
