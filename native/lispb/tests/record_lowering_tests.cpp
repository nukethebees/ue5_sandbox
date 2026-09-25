#include <codegen/generator.h>
#include <lispb/schema/type_graph.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace codegen {
namespace {

TEST(RecordLowering, GeneratesSemanticMembersAndFixedArrays) {
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .types = {{"value", RegisteredTypeSchema{.cpp_type = CppType{"Value", "Project/Value.h"}}}},
        .modules = {NormalModuleSchema{
            .settings = ModuleSettings{.name = "records",
                                       .header = "Records.h",
                                       .namespace_name = "project"},
            .declarations =
                {RecordSchema{.name = "Position",
                              .members = {{.name = "x", .type = TypeRef{"float"}},
                                          {.name = "y", .type = TypeRef{"float"}}}},
                 RecordSchema{
                     .name = "Sample",
                     .members = {{.name = "position", .type = TypeRef{"Position"}},
                                 {.name = "values", .type = TypeRef{"@value"}, .count = 3}},
                     .export_specifier = "PROJECT_API"}}}},
    };

    auto const files{render_modules(lower_modules(manifest))};

    ASSERT_EQ(files.size(), 1U);
    auto const& output{files.front().content};
    EXPECT_NE(output.find("#include <array>"), std::string::npos);
    EXPECT_NE(output.find("#include \"Project/Value.h\""), std::string::npos);
    EXPECT_NE(output.find("namespace project"), std::string::npos);
    EXPECT_NE(output.find("struct Position"), std::string::npos);
    EXPECT_NE(output.find("float x;"), std::string::npos);
    EXPECT_NE(output.find("struct PROJECT_API Sample"), std::string::npos);
    EXPECT_NE(output.find("Position position;"), std::string::npos);
    EXPECT_NE(output.find("std::array<Value, 3> values;"), std::string::npos);
}

TEST(RecordLowering, AllowsEmptyAndRejectsDuplicateAndZeroCountMembers) {
    auto module{NormalModuleSchema{
        .settings = ModuleSettings{.name = "records", .header = "Records.h"},
        .declarations = {RecordSchema{.name = "Record",
                                      .members = {{.name = "value", .type = TypeRef{"float"}}}}}}};
    auto manifest{Manifest{.schema_version = manifest_schema_version, .modules = {module}}};
    EXPECT_NO_THROW(static_cast<void>(lower_modules(manifest)));

    std::get<codegen::RecordSchema>(
        std::get<NormalModuleSchema>(manifest.modules.front()).declarations.front())
        .members.clear();
    auto const files{render_modules(lower_modules(manifest))};
    ASSERT_EQ(files.size(), 1U);
    EXPECT_NE(files.front().content.find("struct Record"), std::string::npos);

    auto& members{std::get<codegen::RecordSchema>(
                      std::get<NormalModuleSchema>(manifest.modules.front()).declarations.front())
                      .members};
    members = {{.name = "value", .type = TypeRef{"float"}},
               {.name = "value", .type = TypeRef{"float"}}};
    EXPECT_THROW(static_cast<void>(lower_modules(manifest)), std::invalid_argument);

    members = {{.name = "values", .type = TypeRef{"float"}, .count = 0}};
    EXPECT_THROW(static_cast<void>(lower_modules(manifest)), std::invalid_argument);
}

TEST(RecordLowering, EmitsByValueDependenciesBeforeTheirUsers) {
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .modules = {NormalModuleSchema{
            .settings = ModuleSettings{.name = "records", .header = "Records.h"},
            .declarations = {RecordSchema{
                                 .name = "Sample",
                                 .members = {{.name = "position", .type = TypeRef{"Position"}}}},
                             RecordSchema{.name = "Position",
                                          .members = {{.name = "x", .type = TypeRef{"float"}}}}}}},
    };

    auto const files{render_modules(lower_modules(manifest))};
    auto const& output{files.front().content};
    EXPECT_LT(output.find("struct Position"), output.find("struct Sample"));
}

TEST(RecordLowering, ForwardDeclaresMutuallyReferringPointerRecords) {
    Manifest const manifest{
        .schema_version = manifest_schema_version,
        .modules = {NormalModuleSchema{
            .settings = ModuleSettings{.name = "records", .header = "Records.h"},
            .declarations = {
                RecordSchema{.name = "First",
                             .members = {{.name = "next", .type = TypeRef{"Second", "*"}}}},
                RecordSchema{.name = "Second",
                             .members = {{.name = "next", .type = TypeRef{"First", "*"}}}}}}}};
    auto const files{render_modules(lower_modules(manifest))};
    auto const& output{files.front().content};
    EXPECT_NE(output.find("struct First;"), std::string::npos);
    EXPECT_NE(output.find("struct Second;"), std::string::npos);
    EXPECT_NE(output.find("Second* next;"), std::string::npos);
    EXPECT_NE(output.find("First* next;"), std::string::npos);
}

TEST(RecordLowering, SoaPointerCyclesRetainSemanticEdgesInEitherSourceOrder) {
    NormalModuleSchema module{};
    module.settings = {
        .name = "pointers", .header = "Pointers.h", .namespace_name = "example::inner"};
    module.soa_backend = SoaBackend::standard_library;
    for (auto const& [name, other] : {std::pair{"A", "B"}, std::pair{"B", "A"}}) {
        module.declarations.emplace_back(SoaSchema{.name = name,
                                                   .members = {{.name = "others",
                                                                .kind = SoaMemberKind::array,
                                                                .type = TypeRef{other, "*"}}},
                                                   .operations = all_storage_operations()});
    }
    for (int order{}; order < 2; ++order) {
        Manifest const manifest{.schema_version = manifest_schema_version, .modules = {module}};
        auto const graph{lispb::schema::resolve_type_graph(manifest)};
        for (auto const& [name, other] : {std::pair{"A", "B"}, std::pair{"B", "A"}}) {
            auto const dependencies{graph.dependencies_of(*graph.find_declared("pointers", name))};
            EXPECT_NE(std::ranges::find(dependencies, *graph.find_declared("pointers", other)),
                      dependencies.end());
        }
        auto const output{render_modules(lower_modules(manifest)).front().content};
        EXPECT_LT(output.find("namespace example::inner"), output.find("struct A;"));
        EXPECT_LT(output.find("struct A;"), output.find("struct BView"));
        EXPECT_LT(output.find("struct B;"), output.find("struct AView"));
        EXPECT_EQ(output.find("struct A;", output.find("struct A;") + 1), std::string::npos);
        std::ranges::reverse(module.declarations);
    }
}

TEST(RecordLowering, ForwardDeclarationsMatchGeneratedRecordKindsAndExcludeAliasesAndExternals) {
    NormalModuleSchema module{};
    module.settings = {
        .name = "kinds", .header = "Kinds.h", .source = "Kinds.cpp", .header_include = "Kinds.h"};
    RecordSchema uses{.name = "Uses"};
    for (auto const name : {"Record",
                            "Raw",
                            "Tagged",
                            "Packed",
                            "Table",
                            "Facade",
                            "FValuesf",
                            "Alias",
                            "Tag",
                            "External",
                            "LayoutOnly"}) {
        uses.members.push_back({.name = std::string{"p"} + name, .type = TypeRef{name, "*"}});
    }
    uses.members.push_back({.name = "facade_reference", .type = TypeRef{"Facade", "&"}});
    module.declarations = {
        uses,
        SoaSchema{
            .name = "LayoutOnly",
            .members = {{.name = "values", .kind = SoaMemberKind::array, .type = TypeRef{"float"}}},
            .layout_only = true},
        RecordSchema{.name = "Record"},
        UnionSchema{.name = "Raw", .alternatives = {{.name = "value", .type = TypeRef{"float"}}}},
        EnumSchema{.name = "Tag",
                   .underlying_type = TypeRef{"std::uint8_t"},
                   .values = {{.name = "Value"}}},
        TaggedUnionSchema{
            .name = "Tagged",
            .discriminant = TypeRef{"Tag"},
            .alternatives = {{.name = "value", .type = TypeRef{"float"}, .tag = "Value"}}},
        PackedValueSchema{.name = "Packed",
                          .storage_type = TypeRef{"std::uint8_t"},
                          .segments = {PackedFieldSchema{
                              .name = "value", .type = TypeRef{"std::uint8_t"}, .bits = 8}}},
        StaticTableSchema{
            .name = "Table", .rows = {{"row"}}, .columns = {{"value", TypeRef{"float"}}}},
        FacadeSchema{.name = "Facade",
                     .target_type = TypeRef{"External"},
                     .target_member_name = "target",
                     .methods = {{.name = "reset", .return_type = TypeRef{"void"}}}},
        HomogeneousLayoutSchema{
            .name = "Values", .components = {"x", "y"}, .value_types = {{TypeRef{"float"}, "f"}}},
        IntegerScalarSchema{.name = "Alias",
                            .minimum_value = 0,
                            .maximum_value = 255,
                            .bit_width = 8,
                            .cpp_emission = IntegerScalarCppEmission::alias,
                            .cpp_type = TypeRef{"std::uint8_t"}}};
    Manifest const manifest{.schema_version = manifest_schema_version, .modules = {module}};
    auto const output{render_modules(lower_modules(manifest)).front().content};
    for (auto const name : {"Record", "Tagged", "Packed", "Table", "FValuesf"}) {
        EXPECT_NE(output.find(std::string{"struct "} + name + ";"), std::string::npos) << name;
    }
    EXPECT_NE(output.find("union Raw;"), std::string::npos);
    EXPECT_NE(output.find("class Facade;"), std::string::npos);
    EXPECT_EQ(output.find("struct Facade;"), std::string::npos);
    for (auto const name : {"Alias", "Tag", "External", "LayoutOnly"}) {
        EXPECT_EQ(output.find(std::string{"struct "} + name + ";"), std::string::npos) << name;
    }
    EXPECT_LT(output.find("using Alias ="), output.find("struct Uses"));
}

} // namespace
} // namespace codegen
