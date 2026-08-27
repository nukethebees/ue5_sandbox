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
        .settings = ModuleSettings{.name = "soa", .header = "Soa.h"},
        .structs = {SoaSchema{
            .name = "FData",
            .members = {
                SoaMemberSchema{"values", SoaMemberKind::array, TypeRef{"int32"}},
            },
        }},
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

TEST(Validation, RejectsDuplicateModuleNames) {
    auto first{valid_soa_module()};
    auto second{valid_soa_module()};
    second.settings.header = "Other.h";
    Manifest const manifest{
        .schema_version = 1,
        .modules = {std::move(first), std::move(second)},
    };

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

TEST(Validation, RejectsHomogeneousModulesWithoutLayouts) {
    HomogeneousModuleSchema module{
        .settings = ModuleSettings{.name = "homogeneous", .header = "Values.h"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateHomogeneousComponents) {
    HomogeneousModuleSchema module{
        .settings = ModuleSettings{.name = "homogeneous", .header = "Values.h"},
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
        .settings = ModuleSettings{.name = "homogeneous", .header = "Values.h"},
        .layouts = {HomogeneousLayoutSchema{
            .name = "Values",
            .components = {"xs"},
        }},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsVectorsWithMoreThanThreeComponents) {
    VectorModuleSchema module{
        .settings = ModuleSettings{.name = "vectors", .header = "Vectors.h"},
        .storage_name = "FVectors",
        .value_type = TypeRef{"float"},
        .components = {"xs", "ys", "zs", "ws"},
        .equivalent_type = TypeRef{"FVector4f"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

TEST(Validation, RejectsDuplicateVectorComponents) {
    VectorModuleSchema module{
        .settings = ModuleSettings{.name = "vectors", .header = "Vectors.h"},
        .storage_name = "FVectors",
        .value_type = TypeRef{"float"},
        .components = {"xs", "xs"},
        .equivalent_type = TypeRef{"FVector2f"},
    };

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

TEST(Validation, RejectsDuplicateUmbrellaHeaders) {
    UmbrellaModuleSchema module{
        .settings = ModuleSettings{.name = "all", .header = "All.h"},
        .headers = {"Value.h", "Value.h"},
    };

    EXPECT_THROW(lower_modules(manifest_with(std::move(module))), std::invalid_argument);
}

} // namespace
} // namespace codegen
