#include <codegen/generator.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>

namespace codegen {
namespace {

template <typename T>
auto manifest_with(T module,
                   std::map<std::string, CppType> types = {}) -> Manifest {
    return Manifest{
        .schema_version = 1,
        .types = std::move(types),
        .modules = {ModuleSchema{std::move(module)}},
    };
}

auto valid_soa_module() -> SoaModuleSchema {
    return SoaModuleSchema{
        .settings = ModuleSettings{.name = "soa", .header = "Soa.h", .source = "Soa.cpp"},
        .structs = {SoaSchema{
            .name = "FData",
            .members = {
                SoaMemberSchema{"values", SoaMemberKind::array, TypeRef{"int32"}},
            },
        }},
    };
}

auto valid_vector_module() -> VectorModuleSchema {
    return VectorModuleSchema{
        .settings = ModuleSettings{
            .name = "vectors", .header = "Vectors.h", .source = "Vectors.cpp"},
        .storage_name = "FVectors",
        .value_type = TypeRef{"float"},
        .components = {"xs", "ys"},
        .equivalent_type = TypeRef{"FVector2f"},
    };
}

auto valid_facade_module() -> FacadeModuleSchema {
    return FacadeModuleSchema{
        .settings = ModuleSettings{.name = "facade", .header = "Facade.h"},
        .facade = FacadeSchema{
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
        .settings = ModuleSettings{
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

TEST(Validation, RejectsEmptyModuleNames) {
    auto module{valid_soa_module()};
    module.settings.name.clear();

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
        .schema_version = 1,
        .modules = {std::move(first), std::move(second)},
    };

    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedProgrammaticSchemaVersions) {
    auto manifest{manifest_with(valid_soa_module())};
    manifest.schema_version = 2;

    EXPECT_THROW(lower_modules(manifest), std::invalid_argument);
}

TEST(Validation, RejectsEmptyTypeSpellings) {
    auto module{valid_soa_module()};
    EXPECT_THROW(lower_modules(manifest_with(std::move(module), {{"empty", CppType{}}})),
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
        .parameters = {
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
        .parameters = {
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

TEST(Validation, RejectsHomogeneousModulesWithoutLayouts) {
    HomogeneousModuleSchema module{
        .settings = ModuleSettings{
            .name = "homogeneous", .header = "Values.h", .source = "Values.cpp"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousComponents) {
    HomogeneousModuleSchema module{
        .settings = ModuleSettings{
            .name = "homogeneous", .header = "Values.h", .source = "Values.cpp"},
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
        .settings = ModuleSettings{
            .name = "homogeneous", .header = "Values.h", .source = "Values.cpp"},
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
    module.layouts.front().value_types.push_back(
        HomogeneousValueSchema{TypeRef{"double"}, "f"});

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
    module.layouts.front().value_types.front().input_types = {
        TypeRef{"FVector2f"}, TypeRef{"FVector2f"}};

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

TEST(Validation, RejectsDuplicateUmbrellaHeaders) {
    UmbrellaModuleSchema module{
        .settings = ModuleSettings{.name = "all", .header = "All.h"},
        .headers = {"Value.h", "Value.h"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsUnsupportedUmbrellaSourceAndNamespaceSettings) {
    UmbrellaModuleSchema module{
        .settings = ModuleSettings{
            .name = "all", .header = "All.h", .source = "All.cpp"},
        .headers = {"Value.h"},
    };
    EXPECT_THROW(lower_modules(manifest_with(module)), std::invalid_argument);

    module.settings.source.reset();
    module.settings.namespace_name = "project";
    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

} // namespace
} // namespace codegen
